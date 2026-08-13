#include <catch2/catch_test_macros.hpp>

#include <woki/ui.hpp>

using namespace std::chrono_literals;
using namespace woki::ui;

TEST_CASE("motion interruption begins from the sampled value") {
    Motion motion;
    const auto start = Motion::Clock::time_point{};
    motion.Set(Key{1}, Property::Opacity, 0.0f, {.duration = 100ms, .curve = Curve::Linear}, start);
    motion.Set(Key{1}, Property::Opacity, 1.0f, {.duration = 100ms, .curve = Curve::Linear}, start);
    CHECK(std::get<woki::f32>(motion.Get(Key{1}, Property::Opacity, start + 50ms)) == 0.5f);

    motion.Set(Key{1}, Property::Opacity, 0.0f, {.duration = 100ms, .curve = Curve::Linear}, start + 50ms);
    CHECK(std::get<woki::f32>(motion.Get(Key{1}, Property::Opacity, start + 100ms)) == 0.25f);
}

TEST_CASE("color transitions interpolate each channel") {
    Motion motion;
    const auto start = Motion::Clock::time_point{};
    motion.Set(Key{4}, Property::Background, Color::rgba(0, 0, 0), {.duration = 100ms}, start);
    motion.Set(Key{4}, Property::Background, Color::rgba(1, 1, 1), {.duration = 100ms, .curve = Curve::Linear}, start);
    const Color color = std::get<Color>(motion.Get(Key{4}, Property::Background, start + 25ms));
    CHECK(color.r == 0.25f);
    CHECK(color.a == 1.0f);
}
