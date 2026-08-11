#pragma once

#include <woki/extension/types.hpp>

namespace woki::events {

#define WOKI_CPP_EVENT_empty(name)                                                                                                                                                                                         \
    struct name##Event final {};
#define WOKI_CPP_EVENT_size(name)                                                                                                                                                                                          \
    struct name##Event final {                                                                                                                                                                                             \
        u32 width{};                                                                                                                                                                                                       \
        u32 height{};                                                                                                                                                                                                      \
    };
#define WOKI_CPP_EVENT_position(name)                                                                                                                                                                                      \
    struct name##Event final {                                                                                                                                                                                             \
        i32 x{};                                                                                                                                                                                                           \
        i32 y{};                                                                                                                                                                                                           \
    };
#define WOKI_CPP_EVENT_scale(name)                                                                                                                                                                                         \
    struct name##Event final {                                                                                                                                                                                             \
        f32 x{};                                                                                                                                                                                                           \
        f32 y{};                                                                                                                                                                                                           \
    };
#define WOKI_CPP_EVENT_key_pressed(name)                                                                                                                                                                                   \
    struct name##Event final {                                                                                                                                                                                             \
        u16 key{};                                                                                                                                                                                                         \
        u32 repeat_count{};                                                                                                                                                                                                \
    };
#define WOKI_CPP_EVENT_key_released(name)                                                                                                                                                                                  \
    struct name##Event final {                                                                                                                                                                                             \
        u16 key{};                                                                                                                                                                                                         \
    };
#define WOKI_CPP_EVENT_key_typed(name)                                                                                                                                                                                     \
    struct name##Event final {                                                                                                                                                                                             \
        u32 character{};                                                                                                                                                                                                   \
    };
#define WOKI_CPP_EVENT_mouse_scrolled(name)                                                                                                                                                                                \
    struct name##Event final {                                                                                                                                                                                             \
        f32 offset_x{};                                                                                                                                                                                                    \
        f32 offset_y{};                                                                                                                                                                                                    \
    };
#define WOKI_CPP_EVENT_mouse_button(name)                                                                                                                                                                                  \
    struct name##Event final {                                                                                                                                                                                             \
        u8 button{};                                                                                                                                                                                                       \
        f32 x{};                                                                                                                                                                                                           \
        f32 y{};                                                                                                                                                                                                           \
    };
#define WOKI_EXT_EVENT(cpp_name, c_name, name, id, layout) WOKI_CPP_EVENT_##layout(cpp_name)
#include <woki/ext/sdk/event_schema.def>
#undef WOKI_EXT_EVENT
#undef WOKI_CPP_EVENT_empty
#undef WOKI_CPP_EVENT_size
#undef WOKI_CPP_EVENT_position
#undef WOKI_CPP_EVENT_scale
#undef WOKI_CPP_EVENT_key_pressed
#undef WOKI_CPP_EVENT_key_released
#undef WOKI_CPP_EVENT_key_typed
#undef WOKI_CPP_EVENT_mouse_scrolled
#undef WOKI_CPP_EVENT_mouse_button

namespace detail {

template <typename T>
struct EventTraits;

#define WOKI_CPP_DECODE_empty(out, data) (void)(out), (void)(data)
#define WOKI_CPP_DECODE_size(out, data)                                                                                                                                                                                    \
    (out).width = woki_ext_event_read_u32_le(data);                                                                                                                                                                        \
    (out).height = woki_ext_event_read_u32_le((data) + 4u)
#define WOKI_CPP_DECODE_position(out, data)                                                                                                                                                                                \
    (out).x = woki_ext_event_read_i32_le(data);                                                                                                                                                                            \
    (out).y = woki_ext_event_read_i32_le((data) + 4u)
#define WOKI_CPP_DECODE_scale(out, data)                                                                                                                                                                                   \
    (out).x = woki_ext_event_read_f32_le(data);                                                                                                                                                                            \
    (out).y = woki_ext_event_read_f32_le((data) + 4u)
#define WOKI_CPP_DECODE_key_pressed(out, data)                                                                                                                                                                             \
    (out).key = woki_ext_event_read_u16_le(data);                                                                                                                                                                          \
    (out).repeat_count = woki_ext_event_read_u32_le((data) + 2u)
#define WOKI_CPP_DECODE_key_released(out, data) (out).key = woki_ext_event_read_u16_le(data)
#define WOKI_CPP_DECODE_key_typed(out, data) (out).character = woki_ext_event_read_u32_le(data)
#define WOKI_CPP_DECODE_mouse_scrolled(out, data)                                                                                                                                                                          \
    (out).offset_x = woki_ext_event_read_f32_le(data);                                                                                                                                                                     \
    (out).offset_y = woki_ext_event_read_f32_le((data) + 4u)
#define WOKI_CPP_DECODE_mouse_button(out, data)                                                                                                                                                                            \
    (out).button = (data)[0];                                                                                                                                                                                              \
    (out).x = woki_ext_event_read_f32_le((data) + 1u);                                                                                                                                                                     \
    (out).y = woki_ext_event_read_f32_le((data) + 5u)
#define WOKI_CPP_SIZE_empty 0u
#define WOKI_CPP_SIZE_size 8u
#define WOKI_CPP_SIZE_position 8u
#define WOKI_CPP_SIZE_scale 8u
#define WOKI_CPP_SIZE_key_pressed 6u
#define WOKI_CPP_SIZE_key_released 2u
#define WOKI_CPP_SIZE_key_typed 4u
#define WOKI_CPP_SIZE_mouse_scrolled 8u
#define WOKI_CPP_SIZE_mouse_button 9u
#define WOKI_EXT_EVENT(cpp_name, c_name, name, id, layout)                                                                                                                                                                 \
    template <>                                                                                                                                                                                                            \
    struct EventTraits<cpp_name##Event> final {                                                                                                                                                                            \
        static constexpr u32 kType = WOKI_EXT_EVENT_##c_name;                                                                                                                                                              \
        static constexpr u32 kSize = WOKI_CPP_SIZE_##layout;                                                                                                                                                               \
        static void Decode(cpp_name##Event& out, const u8* data) noexcept {                                                                                                                                                \
            WOKI_CPP_DECODE_##layout(out, data);                                                                                                                                                                           \
        }                                                                                                                                                                                                                  \
    };
#include <woki/ext/sdk/event_schema.def>
#undef WOKI_EXT_EVENT
#undef WOKI_CPP_DECODE_empty
#undef WOKI_CPP_DECODE_size
#undef WOKI_CPP_DECODE_position
#undef WOKI_CPP_DECODE_scale
#undef WOKI_CPP_DECODE_key_pressed
#undef WOKI_CPP_DECODE_key_released
#undef WOKI_CPP_DECODE_key_typed
#undef WOKI_CPP_DECODE_mouse_scrolled
#undef WOKI_CPP_DECODE_mouse_button
#undef WOKI_CPP_SIZE_empty
#undef WOKI_CPP_SIZE_size
#undef WOKI_CPP_SIZE_position
#undef WOKI_CPP_SIZE_scale
#undef WOKI_CPP_SIZE_key_pressed
#undef WOKI_CPP_SIZE_key_released
#undef WOKI_CPP_SIZE_key_typed
#undef WOKI_CPP_SIZE_mouse_scrolled
#undef WOKI_CPP_SIZE_mouse_button

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
    [[nodiscard]] constexpr bool Is() const noexcept {
        return type_ == detail::EventTraits<T>::kType && size_ == detail::EventTraits<T>::kSize && (size_ == 0u || payload_ != nullptr);
    }

private:
    template <typename>
    friend class DispatcherAccess;
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
        detail::EventTraits<T>::Decode(value, event_.payload_);
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
