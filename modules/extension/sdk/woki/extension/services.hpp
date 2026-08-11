#pragma once

#include <woki/extension/detail/wit_host.hpp>

#ifndef WOKI_EXT_HAS_PATHS
#define WOKI_EXT_HAS_PATHS 0
#endif
#ifndef WOKI_EXT_HAS_STORAGE
#define WOKI_EXT_HAS_STORAGE 0
#endif
#ifndef WOKI_EXT_HAS_CONFIG
#define WOKI_EXT_HAS_CONFIG 0
#endif
#ifndef WOKI_EXT_HAS_EVENTS
#define WOKI_EXT_HAS_EVENTS 0
#endif

#if WOKI_EXT_HAS_PATHS
namespace woki::paths {

template <u32 Capacity>
[[nodiscard]] Status Data(StringBuffer<Capacity>& out) noexcept {
    return extension::detail::wit::DataDir(out.Data(), out.Size());
}

template <u32 Capacity>
[[nodiscard]] Status Cache(StringBuffer<Capacity>& out) noexcept {
    return extension::detail::wit::CacheDir(out.Data(), out.Size());
}

} // namespace woki::paths
#endif

#if WOKI_EXT_HAS_STORAGE
namespace woki::storage {

[[nodiscard]] inline Status Read(StringView path, MutableBytes& out) noexcept {
    return extension::detail::wit::Read(path, out);
}

[[nodiscard]] inline Status Write(StringView path, Bytes data) noexcept {
    return extension::detail::wit::Write(path, data);
}

[[nodiscard]] inline Status Append(StringView path, Bytes data) noexcept {
    return extension::detail::wit::Append(path, data);
}

} // namespace woki::storage
#endif

#if WOKI_EXT_HAS_CONFIG
namespace woki::config {

template <u32 Capacity>
[[nodiscard]] Status Get(StringView key, StringBuffer<Capacity>& out) noexcept {
    return extension::detail::wit::ConfigGet(key, out.Data(), out.Size());
}

[[nodiscard]] inline Status Set(StringView key, StringView value) noexcept {
    return extension::detail::wit::ConfigSet(key, value);
}

} // namespace woki::config
#endif

#if WOKI_EXT_HAS_EVENTS
namespace woki::events {

[[nodiscard]] inline Status Emit(u32 type, Bytes payload = {}) noexcept {
    return extension::detail::wit::Emit(type, payload);
}

[[nodiscard]] inline Status Emit(StringView topic, Bytes payload = {}) noexcept {
    return extension::detail::wit::EmitNamed(topic, payload);
}

} // namespace woki::events
#endif
