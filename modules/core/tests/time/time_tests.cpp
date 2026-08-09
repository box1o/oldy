#include <thread>
#include <catch2/catch_test_macros.hpp>

#include <woki/core.hpp>

TEST_CASE("Clock time conversions are monotonic enough") {
    const auto start = woki::Clock::Now();
    const auto end = woki::Clock::Now();
    REQUIRE(end >= start);
}

TEST_CASE("Stopwatch measures elapsed time") {
    woki::Stopwatch stopwatch;
    stopwatch.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    stopwatch.Stop();

    REQUIRE(stopwatch.ElapsedMilliseconds() >= 1);
    REQUIRE_FALSE(stopwatch.Running());
}

TEST_CASE("FrameTimer advances frames") {
    woki::FrameTimer frame_timer;
    frame_timer.Tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    frame_timer.Tick();

    REQUIRE(frame_timer.FrameCount() == 2);
    REQUIRE(frame_timer.TotalSeconds() >= 0.0);
    REQUIRE(frame_timer.DeltaSeconds() >= 0.0);
}
