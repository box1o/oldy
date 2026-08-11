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
concept HasNamedOnLoad = requires { &T::OnLoad; };
template <typename T>
concept HasNamedOnTick = requires { &T::OnTick; };
template <typename T>
concept HasNamedOnEvent = requires { &T::OnEvent; };
template <typename T>
concept HasNamedOnCommand = requires { &T::OnCommand; };
template <typename T>
concept HasNamedOnUnload = requires { &T::OnUnload; };

template <typename T>
concept OnLoadCallback = requires(T& value, Context& context) { value.OnLoad(context); };
template <typename T>
concept OnTickCallback = requires(T& value, Context& context, double delta_ms) { value.OnTick(context, delta_ms); };
template <typename T>
concept OnEventCallback = requires(T& value, Context& context, Event& event) { value.OnEvent(context, event); };
template <typename T>
concept OnCommandCallback = requires(T& value, Context& context, StringView name, Bytes payload) { value.OnCommand(context, name, payload); };
template <typename T>
concept OnUnloadCallback = requires(T& value, Context& context) { value.OnUnload(context); };

template <typename T>
concept ContextFreeOnLoad = requires(T& value) { value.OnLoad(); };
template <typename T>
concept ContextFreeOnTick = requires(T& value, double delta_ms) { value.OnTick(delta_ms); };
template <typename T>
concept ContextFreeOnEvent = requires(T& value, Event& event) { value.OnEvent(event); };
template <typename T>
concept ContextFreeOnCommand = requires(T& value, StringView name, Bytes payload) { value.OnCommand(name, payload); };
template <typename T>
concept ContextFreeOnUnload = requires(T& value) { value.OnUnload(); };

template <typename T>
inline constexpr bool IsStatusResult = IsSame<T, Status> || IsSame<T, i32>;

template <typename T>
constexpr void ValidatePluginCallbacks() noexcept {
    static_assert(!ContextFreeOnLoad<T>, "OnLoad must have signature void/Status/i32 OnLoad(Context&) noexcept");
    static_assert(!ContextFreeOnTick<T>, "OnTick must have signature void OnTick(Context&, double) noexcept");
    static_assert(!ContextFreeOnEvent<T>, "OnEvent must have signature void OnEvent(Context&, Event&) noexcept");
    static_assert(!ContextFreeOnCommand<T>, "OnCommand must have signature Status/i32 OnCommand(Context&, StringView, Bytes) noexcept");
    static_assert(!ContextFreeOnUnload<T>, "OnUnload must have signature void OnUnload(Context&) noexcept");
    static_assert(!HasNamedOnLoad<T> || OnLoadCallback<T>, "OnLoad has an unsupported signature");
    static_assert(!HasNamedOnTick<T> || OnTickCallback<T>, "OnTick has an unsupported signature");
    static_assert(!HasNamedOnEvent<T> || OnEventCallback<T>, "OnEvent has an unsupported signature");
    static_assert(!HasNamedOnCommand<T> || OnCommandCallback<T>, "OnCommand has an unsupported signature");
    static_assert(!HasNamedOnUnload<T> || OnUnloadCallback<T>, "OnUnload has an unsupported signature");
    if constexpr (OnLoadCallback<T>) {
        using Result = decltype(Declval<T&>().OnLoad(Declval<Context&>()));
        static_assert(IsSame<Result, void> || IsStatusResult<Result>, "OnLoad must return void, Status, or i32");
        static_assert(noexcept(Declval<T&>().OnLoad(Declval<Context&>())), "OnLoad must be noexcept");
    }
    if constexpr (OnTickCallback<T>) {
        static_assert(IsSame<decltype(Declval<T&>().OnTick(Declval<Context&>(), 0.0)), void>, "OnTick must return void");
        static_assert(noexcept(Declval<T&>().OnTick(Declval<Context&>(), 0.0)), "OnTick must be noexcept");
    }
    if constexpr (OnEventCallback<T>) {
        static_assert(IsSame<decltype(Declval<T&>().OnEvent(Declval<Context&>(), Declval<Event&>())), void>, "OnEvent must return void");
        static_assert(noexcept(Declval<T&>().OnEvent(Declval<Context&>(), Declval<Event&>())), "OnEvent must be noexcept");
    }
    if constexpr (OnCommandCallback<T>) {
        using Result = decltype(Declval<T&>().OnCommand(Declval<Context&>(), StringView{}, Bytes{}));
        static_assert(IsStatusResult<Result>, "OnCommand must return Status or i32");
        static_assert(noexcept(Declval<T&>().OnCommand(Declval<Context&>(), StringView{}, Bytes{})), "OnCommand must be noexcept");
    }
    if constexpr (OnUnloadCallback<T>) {
        static_assert(IsSame<decltype(Declval<T&>().OnUnload(Declval<Context&>())), void>, "OnUnload must return void");
        static_assert(noexcept(Declval<T&>().OnUnload(Declval<Context&>())), "OnUnload must be noexcept");
    }
}

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
    static_assert(IsSame<T, i32>, "Raw callback status values must use i32");
    return Status{value};
}

template <typename T>
Status Load(T& value, Context& context) noexcept {
    ValidatePluginCallbacks<T>();
    if constexpr (OnLoadCallback<T>) {
        if constexpr (IsSame<decltype(value.OnLoad(context)), void>) {
            value.OnLoad(context);
            return Status::Success();
        } else
            return AsStatus(value.OnLoad(context));
    } else
        return Status::Success();
}

template <typename T>
void Tick(T& value, Context& context, double delta_ms) noexcept {
    ValidatePluginCallbacks<T>();
    if constexpr (OnTickCallback<T>)
        value.OnTick(context, delta_ms);
}

template <typename T>
void Deliver(T& value, Context& context, Event& event) noexcept {
    ValidatePluginCallbacks<T>();
    if constexpr (OnEventCallback<T>)
        value.OnEvent(context, event);
}

template <typename T>
void Unload(T& value, Context& context) noexcept {
    ValidatePluginCallbacks<T>();
    if constexpr (OnUnloadCallback<T>)
        value.OnUnload(context);
}

template <typename T>
Status Command(T& value, Context& context, StringView name, Bytes payload) noexcept {
    ValidatePluginCallbacks<T>();
    if constexpr (OnCommandCallback<T>)
        return AsStatus(value.OnCommand(context, name, payload));
    else
        return Status::NotFound();
}

} // namespace detail

} // namespace woki::ext
