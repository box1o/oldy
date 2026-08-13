#include "woki/task/algorithms.hpp"

namespace woki::task {

Future<void> WhenAll(std::vector<Future<void>> futures) {
    detail::Promise<void> promise;
    auto output = promise.GetFuture();
    if (futures.empty()) {
        promise.Set(Ok());
        return output;
    }

    struct State {
        State(std::size_t count, detail::Promise<void> completion)
            : remaining(count),
              promise(std::move(completion)) {}

        std::mutex mutex;
        std::size_t remaining;
        std::optional<Error> error;
        bool completion_claimed = false;
        detail::Promise<void> promise;
    };

    auto state = std::make_shared<State>(futures.size(), promise);
    for (auto& future : futures) {
        if (!detail::FutureAccess::Get(future)) {
            std::lock_guard lock(state->mutex);
            if (!state->error) {
                state->error.emplace(MakeError(ErrorCode::InvalidState, "WhenAll received an invalid future"));
            }
            --state->remaining;
            continue;
        }
        auto child = detail::FutureAccess::Get(future);
        detail::Attach<void>(child, [child, state] {
            std::optional<Result<void>> completion;
            {
                std::lock_guard lock(state->mutex);
                if (!*child->result && !state->error) {
                    state->error.emplace(child->result->error());
                }
                if (--state->remaining == 0) {
                    state->completion_claimed = true;
                    completion.emplace(state->error ? Result<void>(std::unexpected(std::move(*state->error))) : Ok());
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

} // namespace woki::task
