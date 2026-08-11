#pragma once

#include <woki/ext/sdk/ext.h>
#include <woki/ext/detail/lifecycle.hpp>

namespace woki::ext::detail {

inline constexpr u32 kAllocatorSlots = 2u;
alignas(16) inline u8 allocator_buffers[kAllocatorSlots][WOKI_EXT_GUEST_BUFFER_SIZE]{};
inline u32 allocator_lengths[kAllocatorSlots]{};

} // namespace woki::ext::detail

/** Defines the raw ABI exports and bounded allocator for one plugin type. */
#define WOKI_PLUGIN(Type)                                                                                                                                                                                                  \
    static_assert(::woki::ext::detail::IsTriviallyDefaultConstructible<Type>, "WOKI_PLUGIN requires a trivially default-constructible plugin type");                                                                       \
    static_assert(::woki::ext::detail::IsTriviallyDestructible<Type>, "WOKI_PLUGIN requires a trivially destructible plugin type");                                                                                        \
    static_assert((::woki::ext::detail::ValidatePluginCallbacks<Type>(), true), "WOKI_PLUGIN callback validation failed");                                                                                                 \
    extern "C" uint32_t ext_api_version(void) {                                                                                                                                                                            \
        return WOKI_EXT_API_VERSION;                                                                                                                                                                                       \
    }                                                                                                                                                                                                                      \
    extern "C" int32_t ext_init(void) {                                                                                                                                                                                    \
        return ::woki::ext::detail::Load(::woki::ext::detail::Instance<Type>(), ::woki::ext::detail::PluginContext()).Code();                                                                                              \
    }                                                                                                                                                                                                                      \
    extern "C" void ext_on_tick(double delta_ms) {                                                                                                                                                                         \
        ::woki::ext::detail::Tick(::woki::ext::detail::Instance<Type>(), ::woki::ext::detail::PluginContext(), delta_ms);                                                                                                  \
    }                                                                                                                                                                                                                      \
    extern "C" void ext_on_event(uint32_t type, const uint8_t* payload, uint32_t size) {                                                                                                                                   \
        ::woki::ext::Event event{type, payload, size};                                                                                                                                                                     \
        ::woki::ext::detail::Deliver(::woki::ext::detail::Instance<Type>(), ::woki::ext::detail::PluginContext(), event);                                                                                                  \
    }                                                                                                                                                                                                                      \
    extern "C" void ext_on_event_named(const char* name, uint32_t name_size, const uint8_t* payload, uint32_t size) {                                                                                                      \
        ::woki::ext::Event event{{name, name_size}, payload, size};                                                                                                                                                        \
        ::woki::ext::detail::Deliver(::woki::ext::detail::Instance<Type>(), ::woki::ext::detail::PluginContext(), event);                                                                                                  \
    }                                                                                                                                                                                                                      \
    extern "C" void ext_on_unload(void) {                                                                                                                                                                                  \
        ::woki::ext::detail::Unload(::woki::ext::detail::Instance<Type>(), ::woki::ext::detail::PluginContext());                                                                                                          \
    }                                                                                                                                                                                                                      \
    extern "C" int32_t ext_on_command(const char* name, uint32_t name_size, const uint8_t* payload, uint32_t size) {                                                                                                       \
        return ::woki::ext::detail::Command(::woki::ext::detail::Instance<Type>(), ::woki::ext::detail::PluginContext(), {name, name_size}, {payload, size}).Code();                                                       \
    }                                                                                                                                                                                                                      \
    extern "C" uint32_t ext_alloc(uint32_t size) {                                                                                                                                                                         \
        if (size == 0u || size > WOKI_EXT_GUEST_BUFFER_SIZE)                                                                                                                                                               \
            return 0u;                                                                                                                                                                                                     \
        for (uint32_t slot = 0; slot < ::woki::ext::detail::kAllocatorSlots; ++slot)                                                                                                                                       \
            if (::woki::ext::detail::allocator_lengths[slot] == 0u) {                                                                                                                                                      \
                ::woki::ext::detail::allocator_lengths[slot] = size;                                                                                                                                                       \
                return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(::woki::ext::detail::allocator_buffers[slot]));                                                                                                   \
            }                                                                                                                                                                                                              \
        return 0u;                                                                                                                                                                                                         \
    }                                                                                                                                                                                                                      \
    extern "C" void ext_free(uint32_t pointer, uint32_t size) {                                                                                                                                                            \
        for (uint32_t slot = 0; slot < ::woki::ext::detail::kAllocatorSlots; ++slot)                                                                                                                                       \
            if (pointer == static_cast<uint32_t>(reinterpret_cast<uintptr_t>(::woki::ext::detail::allocator_buffers[slot])) && size == ::woki::ext::detail::allocator_lengths[slot]) {                                     \
                ::woki::ext::detail::allocator_lengths[slot] = 0u;                                                                                                                                                         \
                return;                                                                                                                                                                                                    \
            }                                                                                                                                                                                                              \
    }
