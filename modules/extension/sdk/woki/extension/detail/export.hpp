#pragma once

#include <woki/ext/sdk/ext.h>

#if __has_include(<new>)
#include <new>
#else
#ifndef WOKI_PLACEMENT_NEW_DEFINED
#define WOKI_PLACEMENT_NEW_DEFINED

inline void* operator new(__SIZE_TYPE__, void* address) noexcept {
    return address;
}
#endif
#endif

#include <woki/extension/detail/lifecycle.hpp>

namespace woki::extension::detail {

inline constexpr u32 kAllocatorSlots = 2u;
alignas(16) inline u8 allocator_buffers[kAllocatorSlots][WOKI_EXT_GUEST_BUFFER_SIZE]{};
inline u32 allocator_lengths[kAllocatorSlots]{};

} // namespace woki::extension::detail

#define WOKI_EXTENSION(Type)                                                                                                                                                                                               \
    static_assert(::woki::extension::detail::IsDefaultConstructible<Type>, "WOKI_EXTENSION requires a default-constructible type");                                                                                        \
    static_assert((::woki::extension::detail::ValidateExtensionCallbacks<Type>(), true), "WOKI_EXTENSION callback validation failed");                                                                                     \
    extern "C" void* memset(void* destination, int value, __SIZE_TYPE__ size) {                                                                                                                                            \
        volatile auto* bytes = static_cast<volatile unsigned char*>(destination);                                                                                                                                          \
        for (__SIZE_TYPE__ index = 0; index < size; ++index)                                                                                                                                                               \
            bytes[index] = static_cast<unsigned char>(value);                                                                                                                                                              \
        return destination;                                                                                                                                                                                                \
    }                                                                                                                                                                                                                      \
    extern "C" uint32_t ext_api_version(void) {                                                                                                                                                                            \
        return WOKI_EXT_API_VERSION;                                                                                                                                                                                       \
    }                                                                                                                                                                                                                      \
    extern "C" int32_t ext_init(void) {                                                                                                                                                                                    \
        return ::woki::extension::detail::Attach(::woki::extension::detail::ConstructInstance<Type>());                                                                                                                    \
    }                                                                                                                                                                                                                      \
    extern "C" void ext_on_tick(double delta_ms) {                                                                                                                                                                         \
        ::woki::extension::detail::Update(::woki::extension::detail::Instance<Type>(), delta_ms);                                                                                                                          \
    }                                                                                                                                                                                                                      \
    extern "C" void ext_on_event(uint32_t type, const uint8_t* payload, uint32_t size) {                                                                                                                                   \
        ::woki::events::Event event{type, payload, size};                                                                                                                                                                  \
        ::woki::extension::detail::Deliver(::woki::extension::detail::Instance<Type>(), event);                                                                                                                            \
    }                                                                                                                                                                                                                      \
    extern "C" void ext_on_event_named(const char* name, uint32_t name_size, const uint8_t* payload, uint32_t size) {                                                                                                      \
        ::woki::events::Event event{::woki::StringView{name, name_size}, payload, size};                                                                                                                                   \
        ::woki::extension::detail::Deliver(::woki::extension::detail::Instance<Type>(), event);                                                                                                                            \
    }                                                                                                                                                                                                                      \
    extern "C" void ext_on_unload(void) {                                                                                                                                                                                  \
        ::woki::extension::detail::DetachInstance<Type>();                                                                                                                                                                 \
    }                                                                                                                                                                                                                      \
    extern "C" int32_t ext_on_command(const char* name, uint32_t name_size, const uint8_t* payload, uint32_t size) {                                                                                                       \
        const ::woki::extension::Command command{{name, name_size}, {payload, size}};                                                                                                                                      \
        return ::woki::extension::detail::Invoke(::woki::extension::detail::Instance<Type>(), command);                                                                                                                    \
    }                                                                                                                                                                                                                      \
    extern "C" uint32_t ext_alloc(uint32_t size) {                                                                                                                                                                         \
        if (size == 0u || size > WOKI_EXT_GUEST_BUFFER_SIZE)                                                                                                                                                               \
            return 0u;                                                                                                                                                                                                     \
        for (uint32_t slot = 0; slot < ::woki::extension::detail::kAllocatorSlots; ++slot) {                                                                                                                               \
            if (::woki::extension::detail::allocator_lengths[slot] == 0u) {                                                                                                                                                \
                ::woki::extension::detail::allocator_lengths[slot] = size;                                                                                                                                                 \
                return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(::woki::extension::detail::allocator_buffers[slot]));                                                                                             \
            }                                                                                                                                                                                                              \
        }                                                                                                                                                                                                                  \
        return 0u;                                                                                                                                                                                                         \
    }                                                                                                                                                                                                                      \
    extern "C" void ext_free(uint32_t pointer, uint32_t size) {                                                                                                                                                            \
        for (uint32_t slot = 0; slot < ::woki::extension::detail::kAllocatorSlots; ++slot) {                                                                                                                               \
            if (pointer == static_cast<uint32_t>(reinterpret_cast<uintptr_t>(::woki::extension::detail::allocator_buffers[slot])) && size == ::woki::extension::detail::allocator_lengths[slot]) {                         \
                ::woki::extension::detail::allocator_lengths[slot] = 0u;                                                                                                                                                   \
                return;                                                                                                                                                                                                    \
            }                                                                                                                                                                                                              \
        }                                                                                                                                                                                                                  \
    }
