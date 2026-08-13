#pragma once

// IWYU pragma: private, include "woki/task.hpp"

#include <mutex>
#include <memory>
#include <vector>
#include <utility>
#include <concepts>
#include <optional>
#include <exception>
#include <functional>
#include <string_view>
#include <type_traits>
#include <condition_variable>

#include "executor.hpp"

namespace woki::task {

template <typename T>
class Future;

namespace detail {

struct FutureAccess;

inline Error ExceptionError() noexcept {
    try {
        throw;
    } catch (const std::exception& exception) {
        return MakeError(ErrorCode::UnknownError, exception.what());
    } catch (...) {
        return MakeError(ErrorCode::UnknownError, "task threw a non-standard exception");
    }
}

template <typename T>
struct FutureState {
    std::mutex mutex;
    std::condition_variable ready_cv;
    std::optional<Result<T>> result;
    std::vector<std::function<void()>> continuations;
};

template <typename T>
class Promise {
public:
    Promise()
        : state_(std::make_shared<FutureState<T>>()) {}

    [[nodiscard]] Future<T> GetFuture() const noexcept;

    bool Set(Result<T> result) const {
        std::vector<std::function<void()>> continuations;
        {
            std::lock_guard lock(state_->mutex);
            if (state_->result.has_value()) {
                return false;
            }
            state_->result.emplace(std::move(result));
            continuations.swap(state_->continuations);
        }
        state_->ready_cv.notify_all();
        for (auto& continuation : continuations) {
            continuation();
        }
        return true;
    }

    bool Fail(Error error) const {
        return Set(Result<T>(std::unexpected(std::move(error))));
    }

private:
    std::shared_ptr<FutureState<T>> state_;
};

template <typename R>
struct ResultTraits {
    using Value = R;

    static Result<R> Convert(R value) {
        return Ok(std::move(value));
    }
};

template <typename T>
struct ResultTraits<Result<T>> {
    using Value = T;

    static Result<T> Convert(Result<T> value) {
        return value;
    }
};

template <>
struct ResultTraits<void> {
    using Value = void;
};

template <typename F, typename... Args>
using InvokeValue = typename ResultTraits<std::invoke_result_t<F, Args...>>::Value;

template <typename F, typename... Args>
auto Invoke(F& function, Args&&... args) -> Result<InvokeValue<F, Args...>> {
    using Return = std::invoke_result_t<F, Args...>;
    if constexpr (std::is_void_v<Return>) {
        std::invoke(function, std::forward<Args>(args)...);
        return Ok();
    } else {
        return ResultTraits<Return>::Convert(std::invoke(function, std::forward<Args>(args)...));
    }
}

template <typename T>
void Attach(const std::shared_ptr<FutureState<T>>& state, std::function<void()> continuation) {
    bool ready = false;
    {
        std::lock_guard lock(state->mutex);
        ready = state->result.has_value();
        if (!ready) {
            state->continuations.emplace_back(std::move(continuation));
        }
    }
    if (ready) {
        continuation();
    }
}

} // namespace detail

template <typename T>
class Future {
public:
    Future() = default;

    [[nodiscard]] bool Valid() const noexcept {
        return state_ != nullptr;
    }

    [[nodiscard]] bool IsReady() const noexcept {
        if (state_ == nullptr) {
            return false;
        }
        std::lock_guard lock(state_->mutex);
        return state_->result.has_value();
    }

    [[nodiscard]] Result<T> Wait() const
    requires std::copy_constructible<T>
    {
        if (state_ == nullptr) {
            return Err(ErrorCode::InvalidState, "future has no state");
        }
#ifdef __EMSCRIPTEN__
        std::lock_guard lock(state_->mutex);
        if (!state_->result.has_value()) {
            return Err(ErrorCode::InvalidState, "an Emscripten future cannot block; drive its executor first");
        }
#else
        std::unique_lock lock(state_->mutex);
        state_->ready_cv.wait(lock, [this] { return state_->result.has_value(); });
#endif
        return *state_->result;
    }

    template <typename F>
    [[nodiscard]] auto Then(Executor& executor, F&& function) const
        -> Future<detail::InvokeValue<std::decay_t<F>, const Result<T>&>> {
        using Function = std::decay_t<F>;
        using U = detail::InvokeValue<Function, const Result<T>&>;
        detail::Promise<U> promise;
        auto next = promise.GetFuture();
        if (state_ == nullptr) {
            promise.Fail(MakeError(ErrorCode::InvalidState, "future has no state"));
            return next;
        }

        auto state = state_;
        detail::Attach<T>(state, [state, &executor, function = Function(std::forward<F>(function)), promise]() mutable {
            detail::Work work;
            work.run = [state, function = std::move(function), promise]() mutable {
                try {
                    promise.Set(detail::Invoke(function, std::as_const(*state->result)));
                } catch (...) {
                    promise.Fail(detail::ExceptionError());
                }
            };
            work.reject = [promise](Error error) mutable { promise.Fail(std::move(error)); };
            auto submitted = detailSubmit(executor, std::move(work));
            if (!submitted) {
                promise.Fail(std::move(submitted).error());
            }
        });
        return next;
    }

private:
    explicit Future(std::shared_ptr<detail::FutureState<T>> state)
        : state_(std::move(state)) {}

    std::shared_ptr<detail::FutureState<T>> state_;
    friend class detail::Promise<T>;
    friend struct detail::FutureAccess;
    template <typename U>
    friend class Future;
    template <typename U>
    friend void detail::Attach(const std::shared_ptr<detail::FutureState<U>>&, std::function<void()>);
};

template <>
class Future<void> {
public:
    Future() = default;

    [[nodiscard]] bool Valid() const noexcept {
        return state_ != nullptr;
    }

    [[nodiscard]] bool IsReady() const noexcept {
        if (state_ == nullptr) {
            return false;
        }
        std::lock_guard lock(state_->mutex);
        return state_->result.has_value();
    }

    [[nodiscard]] Result<void> Wait() const {
        if (state_ == nullptr) {
            return Err(ErrorCode::InvalidState, "future has no state");
        }
#ifdef __EMSCRIPTEN__
        std::lock_guard lock(state_->mutex);
        if (!state_->result.has_value()) {
            return Err(ErrorCode::InvalidState, "an Emscripten future cannot block; drive its executor first");
        }
#else
        std::unique_lock lock(state_->mutex);
        state_->ready_cv.wait(lock, [this] { return state_->result.has_value(); });
#endif
        return *state_->result;
    }

    template <typename F>
    [[nodiscard]] auto Then(Executor& executor, F&& function) const
        -> Future<detail::InvokeValue<std::decay_t<F>, const Result<void>&>> {
        using Function = std::decay_t<F>;
        using U = detail::InvokeValue<Function, const Result<void>&>;
        detail::Promise<U> promise;
        auto next = promise.GetFuture();
        if (state_ == nullptr) {
            promise.Fail(MakeError(ErrorCode::InvalidState, "future has no state"));
            return next;
        }

        auto state = state_;
        detail::Attach<void>(
            state,
            [state, &executor, function = Function(std::forward<F>(function)), promise]() mutable {
                detail::Work work;
                work.run = [state, function = std::move(function), promise]() mutable {
                    try {
                        promise.Set(detail::Invoke(function, std::as_const(*state->result)));
                    } catch (...) {
                        promise.Fail(detail::ExceptionError());
                    }
                };
                work.reject = [promise](Error error) mutable { promise.Fail(std::move(error)); };
                auto submitted = detailSubmit(executor, std::move(work));
                if (!submitted) {
                    promise.Fail(std::move(submitted).error());
                }
            }
        );
        return next;
    }

private:
    explicit Future(std::shared_ptr<detail::FutureState<void>> state)
        : state_(std::move(state)) {}

    std::shared_ptr<detail::FutureState<void>> state_;
    friend class detail::Promise<void>;
    friend struct detail::FutureAccess;
};

namespace detail {
struct FutureAccess {
    template <typename T>
    static const std::shared_ptr<FutureState<T>>& Get(const Future<T>& future) noexcept {
        return future.state_;
    }
};

template <typename T>
Future<T> Promise<T>::GetFuture() const noexcept {
    return Future<T>(state_);
}
} // namespace detail

} // namespace woki::task
