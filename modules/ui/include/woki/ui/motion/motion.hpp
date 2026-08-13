#pragma once

#include <chrono>
#include <variant>
#include <unordered_map>

#include "curve.hpp"
#include "../key.hpp"
#include "../color.hpp"

namespace woki::ui {

using MotionValue = std::variant<f32, Color>;

enum class Property : u8 { Opacity, Background, Foreground, Border, Width, Height, Gap, X, Y, Scale };

struct Transition {
    std::chrono::milliseconds duration{120};
    std::chrono::milliseconds delay{};
    Curve curve{Curve::Out};
    [[nodiscard]] bool operator==(const Transition&) const = default;
};

class Motion {
public:
    using Clock = std::chrono::steady_clock;

    void Set(Key key, Property property, MotionValue target, Transition transition, Clock::time_point now);
    [[nodiscard]] MotionValue Get(Key key, Property property, Clock::time_point now) const;
    [[nodiscard]] bool Active(Clock::time_point now) const;
    void Clear(Key key);

private:
    struct Id {
        u64 key{};
        Property property{};
        [[nodiscard]] bool operator==(const Id&) const = default;
    };

    struct Hash {
        size_t operator()(const Id& id) const {
            return std::hash<u64>{}(id.key ^ (static_cast<u64>(id.property) << 56));
        }
    };

    struct Track {
        MotionValue from;
        MotionValue to;
        Transition transition;
        Clock::time_point start;
    };

    [[nodiscard]] static MotionValue Sample(const Track& track, Clock::time_point now);

    std::unordered_map<Id, Track, Hash> tracks_;
};

} // namespace woki::ui
