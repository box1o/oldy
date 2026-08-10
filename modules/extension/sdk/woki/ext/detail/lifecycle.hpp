#pragma once

#include <woki/ext/detail/events.hpp>

namespace woki::ext {

/** Services supplied to lifecycle callbacks by the host. */
class Context final {
public:
    [[nodiscard]] constexpr Log GetLog() const noexcept {
        return {};
    }

    [[nodiscard]] constexpr Events GetEvents() const noexcept {
        return {};
    }
};

/** Optional base class for extension implementations. */
class Plugin {};

namespace detail {

template <typename T>
T& Instance() noexcept {
    static T value{};
    return value;
}

inline Context& PluginContext() noexcept {
    static Context value{};
    return value;
}

inline Status AsStatus(Status value) noexcept {
    return value;
}

template <typename T>
Status AsStatus(T value) noexcept {
    return Status{static_cast<i32>(value)};
}

template <typename T>
Status Load(T& value, Context& context) noexcept {
    if constexpr (requires { value.OnLoad(context); }) {
        if constexpr (IsSame<decltype(value.OnLoad(context)), void>) {
            value.OnLoad(context);
            return Status::Success();
        } else
            return AsStatus(value.OnLoad(context));
    } else if constexpr (requires { value.OnLoad(); }) {
        if constexpr (IsSame<decltype(value.OnLoad()), void>) {
            value.OnLoad();
            return Status::Success();
        } else
            return AsStatus(value.OnLoad());
    } else
        return Status::Success();
}

template <typename T>
void Tick(T& value, Context& context, double delta_ms) noexcept {
    if constexpr (requires { value.OnTick(context, delta_ms); })
        value.OnTick(context, delta_ms);
    else if constexpr (requires { value.OnTick(delta_ms); })
        value.OnTick(delta_ms);
}

template <typename T>
void Deliver(T& value, Context& context, Event& event) noexcept {
    if constexpr (requires { value.OnEvent(context, event); })
        value.OnEvent(context, event);
    else if constexpr (requires { value.OnEvent(event); })
        value.OnEvent(event);
}

template <typename T>
void Unload(T& value, Context& context) noexcept {
    if constexpr (requires { value.OnUnload(context); })
        value.OnUnload(context);
    else if constexpr (requires { value.OnUnload(); })
        value.OnUnload();
}

template <typename T>
Status Command(T& value, Context& context, StringView name, Bytes payload) noexcept {
    if constexpr (requires { value.OnCommand(context, name, payload); })
        return AsStatus(value.OnCommand(context, name, payload));
    else if constexpr (requires { value.OnCommand(name, payload); })
        return AsStatus(value.OnCommand(name, payload));
    else
        return Status::NotFound();
}

} // namespace detail

} // namespace woki::ext
