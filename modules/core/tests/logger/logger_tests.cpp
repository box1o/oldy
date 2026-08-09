#include <string>
#include <thread>
#include <barrier>
#include <catch2/catch_test_macros.hpp>

#include <woki/core.hpp>

TEST_CASE("Logger can be reconfigured while logging") {
    slog::Configure("woki-core-logger-initial", slog::Level::Off);
    std::barrier start{2};

    std::jthread configure([&start] {
        start.arrive_and_wait();
        for (int i = 0; i < 256; ++i) {
            slog::Configure("woki-core-logger-" + std::to_string(i), slog::Level::Off);
        }
    });
    std::jthread log([&start] {
        start.arrive_and_wait();
        for (int i = 0; i < 1024; ++i) {
            slog::Info("message {}", i);
        }
    });

    configure.join();
    log.join();
    SUCCEED();
}
