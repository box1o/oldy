#pragma once

#include <woki/extension/types.hpp>

namespace woki::events {

template <typename Tag, typename Payload>
struct ApplicationEvent final : Payload {};

#define WOKI_EXT_EVENT(cpp_name, c_name, name, id, layout)                                                                                                                                                                 \
    struct cpp_name##Tag final {};                                                                                                                                                                                         \
    using cpp_name##Event = ApplicationEvent<cpp_name##Tag, woki_ext_##name##_event_t>;
#include <woki/ext/sdk/event_schema.def>
#undef WOKI_EXT_EVENT

namespace detail {
template <typename T>
struct EventTraits;

#define WOKI_EXT_EVENT(cpp_name, c_name, name, id, layout)                                                                                                                                                                 \
    template <>                                                                                                                                                                                                            \
    struct EventTraits<cpp_name##Event> final {                                                                                                                                                                            \
        static constexpr u32 kType = WOKI_EXT_EVENT_##c_name;                                                                                                                                                              \
        static bool Validate(const u8* data, u32 size) noexcept {                                                                                                                                                          \
            woki_ext_##name##_event_t decoded{};                                                                                                                                                                           \
            return woki_ext_decode_##name##_event(data, size, &decoded) == WOKI_EXT_OK;                                                                                                                                    \
        }                                                                                                                                                                                                                  \
        static void Decode(cpp_name##Event& out, const u8* data, u32 size) noexcept {                                                                                                                                      \
            (void)woki_ext_decode_##name##_event(data, size, static_cast<woki_ext_##name##_event_t*>(&out));                                                                                                               \
        }                                                                                                                                                                                                                  \
    };
#include <woki/ext/sdk/event_schema.def>
#undef WOKI_EXT_EVENT
} // namespace detail

class Event final {
public:
    constexpr Event(u32 type, const u8* payload, u32 size) noexcept
        : type_(type),
          payload_(payload),
          size_(size) {}

    constexpr Event(StringView topic, const u8* payload, u32 size) noexcept
        : topic_(topic),
          payload_(payload),
          size_(size) {}

    [[nodiscard]] constexpr u32 Type() const noexcept {
        return type_;
    }

    [[nodiscard]] constexpr bool IsNamed() const noexcept {
        return !topic_.Empty();
    }

    [[nodiscard]] constexpr StringView Topic() const noexcept {
        return topic_;
    }

    [[nodiscard]] constexpr Bytes Payload() const noexcept {
        return {payload_, size_};
    }

    [[nodiscard]] constexpr bool Handled() const noexcept {
        return handled_;
    }

    template <typename T>
    [[nodiscard]] bool Is() const noexcept {
        return type_ == detail::EventTraits<T>::kType && detail::EventTraits<T>::Validate(payload_, size_);
    }

private:
    friend class EventDispatcher;
    u32 type_{};
    StringView topic_{};
    const u8* payload_{};
    u32 size_{};
    bool handled_{};
};

class EventDispatcher final {
public:
    constexpr explicit EventDispatcher(Event& event) noexcept
        : event_(event) {}

    template <typename T, typename Callable>
    bool Dispatch(Callable&& callable) noexcept {
        if (event_.handled_ || !event_.Is<T>())
            return false;
        T value{};
        detail::EventTraits<T>::Decode(value, event_.payload_, event_.size_);
        if constexpr (requires { static_cast<bool>(static_cast<Callable&&>(callable)(value)); })
            event_.handled_ = static_cast<bool>(static_cast<Callable&&>(callable)(value));
        else
            static_cast<Callable&&>(callable)(value);
        return true;
    }

private:
    Event& event_;
};

} // namespace woki::events
