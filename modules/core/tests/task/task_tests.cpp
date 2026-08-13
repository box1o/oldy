#include <atomic>
#include <array>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <woki/task.hpp>

using namespace std::chrono_literals;
using namespace woki;
using namespace woki::task;

namespace {
class Gate {
public:
    void EnterAndWait() {
        std::unique_lock lock(mutex_);
        entered_ = true;
        condition_.notify_all();
        condition_.wait(lock, [&] { return open_; });
    }

    bool WaitForEntry() {
        std::unique_lock lock(mutex_);
        return condition_.wait_for(lock, 2s, [&] { return entered_; });
    }

    void Open() {
        std::lock_guard lock(mutex_);
        open_ = true;
        condition_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    bool entered_{};
    bool open_{};
};
} // namespace

TEST_CASE("CompletionQueue is bounded FIFO and drains by limit") {
    CompletionQueue queue(2);
    std::vector<int> order;
    REQUIRE(queue.Submit([&] { order.push_back(1); }));
    REQUIRE(queue.Submit([&] { order.push_back(2); }));
    const auto full = queue.Submit([] {});
    REQUIRE_FALSE(full);
    REQUIRE(full.error().Code() == ErrorCode::QueueFull);
    REQUIRE(queue.Drain(1) == 1);
    REQUIRE(order == std::vector<int>{1});
    REQUIRE(queue.Drain() == 1);
    REQUIRE((order == std::vector<int>{1, 2}));
    REQUIRE(queue.Drain() == 0);
    queue.RequestStop();
    REQUIRE(queue.IsStopped());
    REQUIRE(queue.Submit([] {}).error().Code() == ErrorCode::ExecutorStopped);
    REQUIRE(queue.Submit({}).error().Code() == ErrorCode::InvalidArgument);
}

#ifndef __EMSCRIPTEN__
TEST_CASE("threaded executor queues are bounded and shutdown rejects queued futures") {
    Scheduler scheduler(1, 1);
    Gate gate;
    auto running = Schedule(scheduler, [&] { gate.EnterAndWait(); });
    REQUIRE(running);
    const bool entered = gate.WaitForEntry();
    if (!entered) {
        scheduler.RequestStop();
        gate.Open();
        scheduler.Join();
        REQUIRE(entered);
    }
    auto queued = Schedule(scheduler, [] { return 2; });
    REQUIRE(queued);
    const auto full = Schedule(scheduler, [] { return 3; });
    REQUIRE_FALSE(full);
    REQUIRE(full.error().Code() == ErrorCode::QueueFull);

    scheduler.RequestStop();
    gate.Open();
    REQUIRE(running->Wait());
    const auto rejected = queued->Wait();
    REQUIRE_FALSE(rejected);
    REQUIRE(rejected.error().Code() == ErrorCode::ExecutorStopped);
    scheduler.Join();
    REQUIRE(scheduler.IsStopped());
}

TEST_CASE("Scheduler and IoExecutor use separate worker domains") {
    Scheduler scheduler(1, 4);
    IoExecutor io(1, 4);
    auto cpu = Schedule(scheduler, [] { return std::this_thread::get_id(); });
    auto blocking = Schedule(io, [] { return std::this_thread::get_id(); });
    REQUIRE(cpu);
    REQUIRE(blocking);
    const auto cpu_id = cpu->Wait();
    const auto io_id = blocking->Wait();
    REQUIRE(cpu_id);
    REQUIRE(io_id);
    REQUIRE(*cpu_id != *io_id);
    REQUIRE(*cpu_id != std::this_thread::get_id());
    REQUIRE(*io_id != std::this_thread::get_id());
}

TEST_CASE("joining an executor from its worker cannot self-deadlock") {
    Scheduler scheduler(1, 2);
    auto joined = Schedule(scheduler, [&scheduler] {
        scheduler.RequestStop();
        scheduler.Join();
        return 7;
    });
    REQUIRE(joined);
    const auto result = joined->Wait();
    REQUIRE(result);
    REQUIRE(*result == 7);
    REQUIRE(scheduler.IsStopped());
}
#else
TEST_CASE("Emscripten scheduler and I/O execution are inline") {
    Scheduler scheduler(4, 1);
    IoExecutor io(4, 1);
    const auto caller = std::this_thread::get_id();
    auto cpu = Schedule(scheduler, [] { return std::this_thread::get_id(); });
    auto blocking = Schedule(io, [] { return std::this_thread::get_id(); });
    REQUIRE(cpu);
    REQUIRE(blocking);
    REQUIRE(cpu->IsReady());
    REQUIRE(blocking->IsReady());
    REQUIRE(*cpu->Wait() == caller);
    REQUIRE(*blocking->Wait() == caller);
    REQUIRE(scheduler.WorkerCount() == 1);
    REQUIRE(io.WorkerCount() == 1);
}
#endif

TEST_CASE("cancellation is shared idempotent and checked before task execution") {
    CancellationSource source;
    const auto token = source.Token();
    REQUIRE(token);
    REQUIRE_FALSE(token.IsCancellationRequested());
    REQUIRE(source.RequestCancellation());
    REQUIRE_FALSE(source.RequestCancellation());
    REQUIRE(token.IsCancellationRequested());

    CompletionQueue queue;
    bool ran = false;
    auto scheduled = Schedule(queue, [&] { ran = true; }, token);
    REQUIRE(scheduled);
    REQUIRE(queue.Drain() == 1);
    const auto result = scheduled->Wait();
    REQUIRE_FALSE(result);
    REQUIRE(result.error().Code() == ErrorCode::Cancelled);
    REQUIRE_FALSE(ran);
}

TEST_CASE("futures carry values void results errors and exceptions") {
    CompletionQueue queue(8);
    auto value = Schedule(queue, [] { return 42; });
    auto nothing = Schedule(queue, [] {});
    auto error = Schedule(queue, []() -> Result<int> { return Err(ErrorCode::InvalidArgument, "bad input"); });
    auto exception = Schedule(queue, []() -> int { throw std::runtime_error("boom"); });
    REQUIRE(value);
    REQUIRE(nothing);
    REQUIRE(error);
    REQUIRE(exception);
    REQUIRE(queue.Drain() == 4);
    REQUIRE(*value->Wait() == 42);
    REQUIRE(nothing->Wait());
    REQUIRE(error->Wait().error().Code() == ErrorCode::InvalidArgument);
    REQUIRE(exception->Wait().error().Message() == "boom");
    REQUIRE(Future<int>{}.Wait().error().Code() == ErrorCode::InvalidState);
    REQUIRE(Future<void>{}.Wait().error().Code() == ErrorCode::InvalidState);
}

TEST_CASE("continuations run on their requested executor and propagate failures") {
    CompletionQueue source;
    CompletionQueue continuation;
    auto value = Schedule(source, [] { return 6; });
    REQUIRE(value);
    auto doubled = value->Then(continuation, [](const Result<int>& result) { return *result * 2; });
    REQUIRE(source.Drain() == 1);
    REQUIRE_FALSE(doubled.IsReady());
    REQUIRE(continuation.Drain() == 1);
    REQUIRE(*doubled.Wait() == 12);

    continuation.RequestStop();
    auto rejected = value->Then(continuation, [](const Result<int>& result) { return *result; });
    REQUIRE(rejected.Wait().error().Code() == ErrorCode::ExecutorStopped);
    auto invalid = Future<int>{}.Then(source, [](const Result<int>&) { return 0; });
    REQUIRE(invalid.Wait().error().Code() == ErrorCode::InvalidState);
}

TEST_CASE("WhenAll preserves value order and reports void value and invalid failures") {
    CompletionQueue queue(8);
    auto one = Schedule(queue, [] { return 1; });
    auto two = Schedule(queue, [] { return 2; });
    auto first_void = Schedule(queue, [] {});
    auto second_void = Schedule(queue, [] {});
    REQUIRE(one && two && first_void && second_void);
    auto values = WhenAll<int>({*one, *two});
    auto voids = WhenAll(std::vector<Future<void>>{*first_void, *second_void});
    REQUIRE(queue.Drain() == 4);
    REQUIRE((*values.Wait() == std::vector<int>{1, 2}));
    REQUIRE(voids.Wait());
    REQUIRE(WhenAll<int>({}).Wait()->empty());
    REQUIRE(WhenAll(std::vector<Future<void>>{}).Wait());
    REQUIRE(WhenAll<int>({Future<int>{}}).Wait().error().Code() == ErrorCode::InvalidState);
    REQUIRE(WhenAll(std::vector<Future<void>>{Future<void>{}}).Wait().error().Code() == ErrorCode::InvalidState);
}

TEST_CASE("ParallelFor covers each index once and stops on error or cancellation") {
    CompletionQueue queue(8);
    std::array<unsigned, 37> visits{};
    auto complete = ParallelFor(queue, 0, visits.size(), [&](std::size_t index) { ++visits[index]; }, {}, 5, 3);
    REQUIRE(queue.Drain() == 3);
    REQUIRE(complete.Wait());
    REQUIRE(std::ranges::all_of(visits, [](unsigned count) { return count == 1; }));

    auto failed = ParallelFor(
        queue,
        0,
        10,
        [](std::size_t index) -> Result<void> { return index == 4 ? Err(ErrorCode::InvalidArgument, "four") : Ok(); },
        {},
        10,
        1
    );
    REQUIRE(queue.Drain() == 1);
    REQUIRE(failed.Wait().error().Code() == ErrorCode::InvalidArgument);

    CancellationSource source;
    REQUIRE(source.RequestCancellation());
    auto cancelled = ParallelFor(queue, 0, 10, [](std::size_t) {}, source.Token(), 2, 1);
    REQUIRE(queue.Drain() == 1);
    REQUIRE(cancelled.Wait().error().Code() == ErrorCode::Cancelled);
    REQUIRE(ParallelFor(queue, 5, 5, [](std::size_t) {}).Wait());
}
