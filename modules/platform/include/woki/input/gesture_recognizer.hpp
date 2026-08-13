#pragma once

#include <map>
#include <vector>

#include "woki/events/events.hpp"

namespace woki {

class GestureRecognizer final {
public:
    [[nodiscard]] std::vector<scope<events::Event>> Process(const events::Event& event);
    [[nodiscard]] std::vector<scope<events::Event>> Update(f64 timestamp);
    void Reset() noexcept;

private:
    struct Contact {
        events::PointerData start;
        events::PointerData current;
        f64 started{0};
        bool panning{false};
        bool long_pressed{false};
    };

    std::map<events::PointerId, Contact> contacts_;
    f64 last_tap_time_{-1};
    f32 last_tap_x_{0};
    f32 last_tap_y_{0};
    f32 initial_distance_{0};
    f32 initial_angle_{0};
    f32 last_scale_{1};
    f32 last_angle_{0};
};

} // namespace woki
