#include <atomic>
#include <vector>

#include "woki/task.hpp"

int main() {
    using namespace woki;
    using namespace woki::task;

    CompletionQueue publication(1);
    auto first = Schedule(publication, [] { return 21; });
    auto full = Schedule(publication, [] { return 0; });
    if (!first || full || full.error().Code() != ErrorCode::QueueFull || publication.Drain() != 1) {
        return 1;
    }

    auto doubled = first->Then(publication, [](const Result<int>& value) -> Result<int> {
        if (!value) {
            return Err(value.error());
        }
        return Ok(*value * 2);
    });
    if (publication.Drain() != 1) {
        return 2;
    }
    auto doubled_result = doubled.Wait();
    if (!doubled_result || *doubled_result != 42) {
        return 2;
    }

    Scheduler scheduler(2, 8);
    std::atomic_size_t total = 0;
    auto loop = ParallelFor(
        scheduler,
        0,
        100,
        [&total](std::size_t) { total.fetch_add(1, std::memory_order_relaxed); },
        {},
        8,
        2
    );
    auto one = Schedule(scheduler, [] { return 1; });
    auto two = Schedule(scheduler, [] { return 2; });
    if (!one || !two) {
        return 3;
    }
    auto all = WhenAll<int>({*one, *two});
    auto loop_result = loop.Wait();
    auto all_result = all.Wait();
    if (!loop_result || total.load(std::memory_order_relaxed) != 100 || !all_result
        || *all_result != std::vector<int>({1, 2})) {
        return 4;
    }

    CancellationSource source;
    if (!source.RequestCancellation()) {
        return 5;
    }
    auto cancelled = Schedule(scheduler, [] { return 1; }, source.Token());
    if (!cancelled || cancelled->Wait().error().Code() != ErrorCode::Cancelled) {
        return 6;
    }

    scheduler.RequestStop();
    scheduler.Join();
    return scheduler.Submit([] {}).error().Code() == ErrorCode::ExecutorStopped ? 0 : 7;
}
