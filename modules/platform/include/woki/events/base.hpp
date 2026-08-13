#pragma once

#include <concepts>
#include <string_view>

#include "woki/events/type.hpp"
#include "woki/events/category.hpp"

namespace woki::events {

using WindowId = u32;
using DeviceId = u64;
using PointerId = u64;

enum class EventSource : u8 { kUnknown, kNative, kWeb, kSynthetic };

enum class Modifier : u16 {
    kNone = 0,
    kShift = 1 << 0,
    kControl = 1 << 1,
    kAlt = 1 << 2,
    kSuper = 1 << 3,
    kCapsLock = 1 << 4,
    kNumLock = 1 << 5,
};

using ModifierFlags = u16;

struct EventMetadata final {
    f64 timestamp{0.0};
    u64 sequence{0};
    WindowId window{0};
    DeviceId device{0};
    ModifierFlags modifiers{0};
    EventSource source{EventSource::kUnknown};
};

template <typename T>
concept EventLike = requires(const T event) {
    { T::GetStaticType() } -> std::same_as<EventType>;
    { event.GetEventType() } -> std::same_as<EventType>;
    { event.GetCategoryFlags() } -> std::same_as<u16>;
    { event.GetName() } -> std::convertible_to<std::string_view>;
};

class Event {
public:
    virtual ~Event() = default;

    [[nodiscard]] virtual EventType GetEventType() const noexcept = 0;
    [[nodiscard]] virtual u16 GetCategoryFlags() const noexcept = 0;
    [[nodiscard]] virtual std::string_view GetName() const noexcept = 0;

    [[nodiscard]] bool IsInCategory(EventCategory category) const noexcept {
        return (GetCategoryFlags() & category) != 0;
    }

    bool handled{false};
    EventMetadata metadata{};

    [[nodiscard]] f64 Timestamp() const noexcept {
        return metadata.timestamp;
    }
};

template <EventType Type, EventCategory... Categories>
class TypedEvent : public Event {
public:
    [[nodiscard]] static constexpr EventType GetStaticType() noexcept {
        return Type;
    }

    [[nodiscard]] EventType GetEventType() const noexcept final {
        return Type;
    }

    [[nodiscard]] u16 GetCategoryFlags() const noexcept final {
        return (static_cast<u16>(Categories) | ... | static_cast<u16>(EventCategory::kNone));
    }
};

} // namespace woki::events
