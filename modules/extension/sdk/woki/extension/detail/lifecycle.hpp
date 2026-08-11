#pragma once

#include <woki/extension/events.hpp>
#include <woki/extension/command.hpp>

namespace woki::extension::detail {

template <typename T>
T&& Declval() noexcept;

template <typename T, typename U>
inline constexpr bool IsSame = false;
template <typename T>
inline constexpr bool IsSame<T, T> = true;

template <typename T>
inline constexpr bool IsDefaultConstructible = __is_constructible(T);

template <typename T>
concept HasNamedOnAttach = requires { &T::OnAttach; };
template <typename T>
concept HasNamedOnUpdate = requires { &T::OnUpdate; };
template <typename T>
concept HasNamedOnEvent = requires { &T::OnEvent; };
template <typename T>
concept HasNamedOnCommand = requires { &T::OnCommand; };
template <typename T>
concept HasNamedOnDetach = requires { &T::OnDetach; };

template <typename T, typename R>
concept OnAttachReturning = requires { static_cast<R (T::*)() noexcept>(&T::OnAttach); };
template <typename T>
concept OnAttachCallback = OnAttachReturning<T, void> || OnAttachReturning<T, Status> || OnAttachReturning<T, i32>;
template <typename T>
concept OnUpdateCallback = requires { static_cast<void (T::*)(f64) noexcept>(&T::OnUpdate); };
template <typename T>
concept OnEventCallback = requires { static_cast<void (T::*)(events::Event&) noexcept>(&T::OnEvent); };
template <typename T, typename R>
concept OnCommandReturning = requires { static_cast<R (T::*)(const Command&) noexcept>(&T::OnCommand); };
template <typename T>
concept OnCommandCallback = OnCommandReturning<T, void> || OnCommandReturning<T, Status> || OnCommandReturning<T, i32>;
template <typename T>
concept OnDetachCallback = requires { static_cast<void (T::*)() noexcept>(&T::OnDetach); };

template <typename T>
constexpr void ValidateExtensionCallbacks() noexcept {
    static_assert(!HasNamedOnAttach<T> || OnAttachCallback<T>, "OnAttach must return void, Status, or i32 and be noexcept");
    static_assert(!HasNamedOnUpdate<T> || OnUpdateCallback<T>, "OnUpdate must have signature void OnUpdate(f64) noexcept");
    static_assert(!HasNamedOnEvent<T> || OnEventCallback<T>, "OnEvent must have signature void OnEvent(events::Event&) noexcept");
    static_assert(!HasNamedOnCommand<T> || OnCommandCallback<T>, "OnCommand must take const Command&, return void, Status, or i32, and be noexcept");
    static_assert(!HasNamedOnDetach<T> || OnDetachCallback<T>, "OnDetach must have signature void OnDetach() noexcept");
    if constexpr (OnUpdateCallback<T>) {
        static_assert(IsSame<decltype(Declval<T&>().OnUpdate(0.0)), void>, "OnUpdate must return void");
        static_assert(noexcept(Declval<T&>().OnUpdate(0.0)), "OnUpdate must be noexcept");
    }
    if constexpr (OnEventCallback<T>) {
        static_assert(IsSame<decltype(Declval<T&>().OnEvent(Declval<events::Event&>())), void>, "OnEvent must return void");
        static_assert(noexcept(Declval<T&>().OnEvent(Declval<events::Event&>())), "OnEvent must be noexcept");
    }
    if constexpr (OnDetachCallback<T>) {
        static_assert(IsSame<decltype(Declval<T&>().OnDetach()), void>, "OnDetach must return void");
        static_assert(noexcept(Declval<T&>().OnDetach()), "OnDetach must be noexcept");
    }
}

template <typename T>
struct InstanceStorage final {
    alignas(T) u8 bytes[sizeof(T)]{};
    bool alive{};
};

template <typename T>
inline InstanceStorage<T> instance_storage;

template <typename T>
T& Instance() noexcept {
    return *reinterpret_cast<T*>(instance_storage<T>.bytes);
}

template <typename T>
T& ConstructInstance() noexcept {
    T* value = ::new (instance_storage<T>.bytes) T{};
    instance_storage<T>.alive = true;
    return *value;
}

template <typename T>
i32 Attach(T& value) noexcept {
    ValidateExtensionCallbacks<T>();
    if constexpr (OnAttachReturning<T, Status>)
        return value.OnAttach().Code();
    else if constexpr (OnAttachReturning<T, i32>)
        return value.OnAttach();
    else if constexpr (OnAttachReturning<T, void>)
        value.OnAttach();
    return WOKI_EXT_OK;
}

template <typename T>
void Update(T& value, f64 delta_ms) noexcept {
    ValidateExtensionCallbacks<T>();
    if constexpr (OnUpdateCallback<T>)
        value.OnUpdate(delta_ms);
}

template <typename T>
void Deliver(T& value, events::Event& event) noexcept {
    ValidateExtensionCallbacks<T>();
    if constexpr (OnEventCallback<T>)
        value.OnEvent(event);
}

template <typename T>
i32 Invoke(T& value, const Command& command) noexcept {
    ValidateExtensionCallbacks<T>();
    if constexpr (OnCommandReturning<T, Status>)
        return value.OnCommand(command).Code();
    else if constexpr (OnCommandReturning<T, i32>)
        return value.OnCommand(command);
    else if constexpr (OnCommandReturning<T, void>) {
        value.OnCommand(command);
        return WOKI_EXT_OK;
    }
    return WOKI_EXT_NOT_FOUND;
}

template <typename T>
void Detach(T& value) noexcept {
    ValidateExtensionCallbacks<T>();
    if constexpr (OnDetachCallback<T>)
        value.OnDetach();
    value.~T();
    instance_storage<T>.alive = false;
}

template <typename T>
void DetachInstance() noexcept {
    if (instance_storage<T>.alive)
        Detach(Instance<T>());
}

} // namespace woki::extension::detail
