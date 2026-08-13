#pragma once

// IWYU pragma: private, include "woki/task.hpp"

#include <atomic>
#include <memory>
#include <thread>
#include <vector>
#include <cstddef>
#include <algorithm>

#include "future.hpp"
#include "cancellation.hpp"

namespace woki::task {

template <typename F>
[[nodiscard]] auto Schedule(Executor& executor, F&& function, CancellationToken cancellation = {})
    -> Result<Future<detail::InvokeValue<std::decay_t<F>>>> {
    using Function = std::decay_t<F>;
    using T = detail::InvokeValue<Function>;
    detail::Promise<T> promise;
    detail::Work work;
    work.run = [function = Function(std::forward<F>(function)), cancellation, promise]() mutable {
        if (cancellation.IsCancellationRequested()) {
            promise.Fail(MakeError(ErrorCode::Cancelled, "task was cancelled before execution"));
            return;
        }
        try {
            promise.Set(detail::Invoke(function));
        } catch (...) {
            promise.Fail(detail::ExceptionError());
        }
    };
    work.reject = [promise](Error error) mutable { promise.Fail(std::move(error)); };
    auto submitted = detailSubmit(executor, std::move(work));
    if (!submitted) {
        return Err(std::move(submitted).error());
    }
    return Ok(promise.GetFuture());
}

template <typename T>
requires std::copy_constructible<T>
[[nodiscard]] Future<std::vector<T>> WhenAll(std::vector<Future<T>> futures) {
    detail::Promise<std::vector<T>> promise;
    auto output = promise.GetFuture();
    if (futures.empty()) {
        promise.Set(Ok(std::vector<T>{}));
        return output;
    }

    struct State {
        State(std::size_t count, detail::Promise<std::vector<T>> completion)
            : remaining(count),
              values(count),
              promise(std::move(completion)) {}

        std::mutex mutex;
        std::size_t remaining;
        std::vector<std::optional<T>> values;
        std::optional<Error> error;
        bool completion_claimed = false;
        detail::Promise<std::vector<T>> promise;
    };

    auto state = std::make_shared<State>(futures.size(), promise);
    for (std::size_t index = 0; index < futures.size(); ++index) {
        if (!detail::FutureAccess::Get(futures[index])) {
            std::lock_guard lock(state->mutex);
            if (!state->error) {
                state->error.emplace(MakeError(ErrorCode::InvalidState, "WhenAll received an invalid future"));
            }
            --state->remaining;
            continue;
        }
        auto child = detail::FutureAccess::Get(futures[index]);
        detail::Attach<T>(child, [child, state, index] {
            std::optional<Result<std::vector<T>>> completion;
            {
                std::lock_guard lock(state->mutex);
                const auto& result = *child->result;
                if (result) {
                    state->values[index].emplace(*result);
                } else if (!state->error) {
                    state->error.emplace(result.error());
                }
                if (--state->remaining == 0) {
                    state->completion_claimed = true;
                    if (state->error) {
                        completion.emplace(std::unexpected(std::move(*state->error)));
                    } else {
                        std::vector<T> values;
                        values.reserve(state->values.size());
                        for (auto& value : state->values) {
                            values.emplace_back(std::move(*value));
                        }
                        completion.emplace(Ok(std::move(values)));
                    }
                }
            }
            if (completion) {
                state->promise.Set(std::move(*completion));
            }
        });
    }
    {
        std::optional<Error> error;
        {
            std::lock_guard lock(state->mutex);
            if (state->remaining == 0 && state->error && !state->completion_claimed) {
                state->completion_claimed = true;
                error.emplace(std::move(*state->error));
            }
        }
        if (error) {
            state->promise.Fail(std::move(*error));
        }
    }
    return output;
}

[[nodiscard]] Future<void> WhenAll(std::vector<Future<void>> futures);

template <typename F>
[[nodiscard]] Future<void> ParallelFor(
    Executor& executor,
    std::size_t begin,
    std::size_t end,
    F&& function,
    CancellationToken cancellation = {},
    std::size_t chunk_size = 64,
    std::size_t max_workers = 0
) {
    detail::Promise<void> promise;
    auto output = promise.GetFuture();
    if (begin >= end) {
        promise.Set(Ok());
        return output;
    }

    using Function = std::decay_t<F>;

    struct State {
        State(
            std::size_t first,
            std::size_t last,
            std::size_t chunk_size,
            std::size_t worker_count,
            Function task,
            CancellationToken token,
            detail::Promise<void> completion
        )
            : next(first),
              end(last),
              chunk(chunk_size),
              remaining(worker_count),
              function(std::move(task)),
              cancellation(std::move(token)),
              promise(std::move(completion)) {}

        std::atomic_size_t next;
        std::size_t end;
        std::size_t chunk;
        std::atomic_size_t remaining;
        std::atomic_bool failed{false};
        std::mutex error_mutex;
        std::optional<Error> error;
        Function function;
        CancellationToken cancellation;
        detail::Promise<void> promise;
    };

    chunk_size = std::max<std::size_t>(1, chunk_size);
    const std::size_t chunk_count = (end - begin + chunk_size - 1) / chunk_size;
    const std::size_t desired = max_workers == 0 ? std::max(1u, std::thread::hardware_concurrency()) : max_workers;
    const std::size_t workers = std::min(chunk_count, std::max<std::size_t>(1, desired));
    auto state = std::make_shared<State>(
        begin,
        end,
        chunk_size,
        workers,
        Function(std::forward<F>(function)),
        cancellation,
        promise
    );

    auto finish = [state](std::optional<Error> error = {}) {
        if (error) {
            state->failed.store(true, std::memory_order_release);
            std::lock_guard lock(state->error_mutex);
            if (!state->error) {
                state->error.emplace(std::move(*error));
            }
        }
        if (state->remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            std::optional<Error> final_error;
            {
                std::lock_guard lock(state->error_mutex);
                if (state->error) {
                    final_error.emplace(std::move(*state->error));
                }
            }
            if (final_error) {
                state->promise.Fail(std::move(*final_error));
            } else {
                state->promise.Set(Ok());
            }
        }
    };

    for (std::size_t worker = 0; worker < workers; ++worker) {
        detail::Work work;
        work.run = [state, finish] {
            try {
                while (!state->failed.load(std::memory_order_acquire)) {
                    if (state->cancellation.IsCancellationRequested()) {
                        finish(MakeError(ErrorCode::Cancelled, "parallel loop was cancelled"));
                        return;
                    }
                    const std::size_t first = state->next.fetch_add(state->chunk, std::memory_order_relaxed);
                    if (first >= state->end) {
                        break;
                    }
                    const std::size_t last = std::min(state->end, first + state->chunk);
                    for (std::size_t index = first; index < last; ++index) {
                        auto result = detail::Invoke(state->function, index);
                        if (!result) {
                            finish(std::move(result).error());
                            return;
                        }
                    }
                }
                finish();
            } catch (...) {
                finish(detail::ExceptionError());
            }
        };
        work.reject = [finish](Error error) mutable { finish(std::move(error)); };
        auto submitted = detailSubmit(executor, std::move(work));
        if (!submitted) {
            finish(std::move(submitted).error());
        }
    }
    return output;
}

} // namespace woki::task
