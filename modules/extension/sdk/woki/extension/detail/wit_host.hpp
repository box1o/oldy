#pragma once

// Generated from modules/extension/wit/host.wit. Do not edit directly.
#include <woki/extension/types.hpp>

namespace woki::extension::detail::wit {

inline Status Log(u32 level, StringView message) noexcept {
    return Status{host_log(level, message.Data(), message.Size())};
}

inline Status DataDir(char* out, u32 capacity) noexcept {
    return Status{host_path_data(out, capacity)};
}

inline Status CacheDir(char* out, u32 capacity) noexcept {
    return Status{host_path_cache(out, capacity)};
}

inline Status Read(StringView path, MutableBytes& out) noexcept {
    u32 size = out.Capacity();
    const Status status{host_file_read_n(path.Data(), path.Size(), out.Data(), &size)};
    out.SetSize(status.Ok() || status.Code() == WOKI_EXT_NO_SPACE ? size : 0u);
    return status;
}

inline Status Write(StringView path, Bytes data) noexcept {
    return Status{host_file_write_n(path.Data(), path.Size(), data.Data(), data.Size())};
}

inline Status Append(StringView path, Bytes data) noexcept {
    return Status{host_file_append_n(path.Data(), path.Size(), data.Data(), data.Size())};
}

inline Status ConfigGet(StringView key, char* out, u32 capacity) noexcept {
    if (key.Empty() || key.Data() == nullptr || key.Size() > WOKI_EXT_MAX_CONFIG_KEY_LEN)
        return Status::Invalid();
    char terminated[WOKI_EXT_MAX_CONFIG_KEY_LEN + 1u]{};
    for (u32 index = 0; index < key.Size(); ++index) {
        if (key.Data()[index] == 0)
            return Status::Invalid();
        terminated[index] = key.Data()[index];
    }
    return Status{host_config_get(terminated, out, capacity)};
}

inline Status ConfigSet(StringView key, StringView value) noexcept {
    if (key.Empty() || key.Data() == nullptr || key.Size() > WOKI_EXT_MAX_CONFIG_KEY_LEN)
        return Status::Invalid();
    char terminated[WOKI_EXT_MAX_CONFIG_KEY_LEN + 1u]{};
    for (u32 index = 0; index < key.Size(); ++index) {
        if (key.Data()[index] == 0)
            return Status::Invalid();
        terminated[index] = key.Data()[index];
    }
    return Status{host_config_set(terminated, value.Data(), value.Size())};
}

inline Status Emit(u32 type, Bytes payload) noexcept {
    return Status{host_event_emit(type, payload.Data(), payload.Size())};
}

inline Status EmitNamed(StringView topic, Bytes payload) noexcept {
    return Status{host_event_emit_named(topic.Data(), topic.Size(), payload.Data(), payload.Size())};
}

} // namespace woki::extension::detail::wit
