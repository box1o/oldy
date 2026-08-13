#pragma once

#include <algorithm>
#include <cmath>

#include <woki/core.hpp>

namespace woki::gfx {

enum class ExposureMode : u8 { Manual, LogAverage, Histogram };

struct ExposureSettings final {
    ExposureMode mode{ExposureMode::Manual};
    f32 manual{1.0F};
    f32 key{0.18F};
    f32 minimum{0.03F};
    f32 maximum{32.0F};
    f32 speed_up{3.0F};
    f32 speed_down{1.0F};
};

struct ExposureState final {
    f32 value{1.0F};
    bool valid{};

    void Update(const ExposureSettings& settings, const f32 log_average_luminance, const f32 delta_time) noexcept {
        const f32 target = settings.mode == ExposureMode::Manual ? settings.manual : std::clamp(settings.key / std::exp(log_average_luminance), settings.minimum, settings.maximum);
        const f32 speed = target > value ? settings.speed_up : settings.speed_down;
        value += (target - value) * (1.0F - std::exp(-speed * std::max(delta_time, 0.0F)));
        valid = true;
    }
};

} // namespace woki::gfx
