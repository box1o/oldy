#include <string>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <string_view>

#include "woki/ext/limits.hpp"
#include "woki/ext/host/api.hpp"
#include "woki/ext/host/cabi.hpp"

#include "types.h"

namespace woki::ext::host::cabi {

namespace {

using woki::ext::limits::kMaxConfigKeyBytes;
using woki::ext::limits::kMaxConfigValueBytes;
using woki::ext::limits::kMaxFileBytes;
using woki::ext::limits::kMaxLogBytes;
using woki::ext::limits::kMaxPathBytes;

static_assert(cabi::kOk == WOKI_EXT_OK);
static_assert(cabi::kErr == WOKI_EXT_ERR);
static_assert(cabi::kDenied == WOKI_EXT_DENIED);
static_assert(cabi::kNoSpace == WOKI_EXT_NO_SPACE);
static_assert(cabi::kNotFound == WOKI_EXT_NOT_FOUND);
static_assert(cabi::kInvalid == WOKI_EXT_INVALID);

[[nodiscard]] i32 StatusFromError(const Error& error) noexcept {
    switch (error.Code()) {
        case ErrorCode::FileNotFound:
            return cabi::kNotFound;
        case ErrorCode::FileAccessDenied:
            return cabi::kDenied;
        case ErrorCode::ValidationOutOfRange:
            return cabi::kNoSpace;
        case ErrorCode::ValidationInvalidState:
            return cabi::kDenied;
        case ErrorCode::InvalidArgument:
        case ErrorCode::ParseInvalidFormat:
        case ErrorCode::ParseMissingField:
        case ErrorCode::ParseTypeMismatch:
            return cabi::kInvalid;
        default:
            return cabi::kErr;
    }
}

[[nodiscard]] Result<std::string_view> BoundedString(const char* data, u32 len, u32 max_len, std::string_view field) {
    if (data == nullptr) {
        return Err(ErrorCode::InvalidArgument, std::string(field) + " pointer is null.");
    }
    if (len > max_len) {
        return Err(ErrorCode::ValidationOutOfRange, std::string(field) + " exceeds size limit.");
    }
    return Ok(std::string_view(data, len));
}

[[nodiscard]] Result<std::string_view> BoundedCString(const char* data, u32 max_len, std::string_view field) {
    if (data == nullptr) {
        return Err(ErrorCode::InvalidArgument, std::string(field) + " pointer is null.");
    }

    std::size_t len = 0;
    while (len <= max_len && data[len] != '\0') {
        ++len;
    }
    if (len > max_len) {
        return Err(ErrorCode::ValidationOutOfRange, std::string(field) + " exceeds size limit.");
    }
    return Ok(std::string_view(data, len));
}

[[nodiscard]] i32 CopyString(std::string_view text, char* out, u32 out_cap) {
    if (out == nullptr) {
        return cabi::kInvalid;
    }
    if (text.size() + 1 > out_cap) {
        return cabi::kNoSpace;
    }

    std::memcpy(out, text.data(), text.size());
    out[text.size()] = '\0';
    return cabi::kOk;
}

[[nodiscard]] LogLevel ParseLogLevel(u32 level) noexcept {
    switch (level) {
        case 0:
            return LogLevel::Debug;
        case 1:
            return LogLevel::Info;
        case 2:
            return LogLevel::Warn;
        case 3:
            return LogLevel::Error;
        default:
            return LogLevel::Info;
    }
}

} // namespace

i32 Log(Record& record, u32 level, const char* message, u32 len) {
    if (!HasPermission(record.manifest, Permission::Log)) {
        return cabi::kDenied;
    }

    auto text = BoundedString(message, len, kMaxLogBytes, "log message");
    if (!text) {
        return StatusFromError(text.error());
    }

    HostApi host(record);
    host.Log(ParseLogLevel(level), *text);
    return cabi::kOk;
}

i32 PathData(Record& record, char* out, u32 out_cap) {
    HostApi host(record);
    auto path = host.DataPath();
    if (!path) {
        return StatusFromError(path.error());
    }
    return CopyString(path->string(), out, out_cap);
}

i32 PathCache(Record& record, char* out, u32 out_cap) {
    HostApi host(record);
    auto path = host.CachePath();
    if (!path) {
        return StatusFromError(path.error());
    }
    return CopyString(path->string(), out, out_cap);
}

i32 FileRead(Record& record, const char* rel_path, u8* out, u32* inout_len) {
    auto path = BoundedCString(rel_path, kMaxPathBytes, "path");
    if (!path) {
        return StatusFromError(path.error());
    }
    return FileRead(record, path->data(), static_cast<u32>(path->size()), out, inout_len);
}

i32 FileRead(Record& record, const char* rel_path, u32 rel_path_len, u8* out, u32* inout_len) {
    if (inout_len == nullptr) {
        return cabi::kInvalid;
    }
    auto path = BoundedString(rel_path, rel_path_len, kMaxPathBytes, "path");
    if (!path) {
        return StatusFromError(path.error());
    }

    HostApi host(record);
    auto data = host.ReadFile(std::filesystem::path(std::string(*path)));
    if (!data) {
        return StatusFromError(data.error());
    }
    if (data->size() > kMaxFileBytes) {
        return cabi::kNoSpace;
    }
    if (out == nullptr || *inout_len < data->size()) {
        *inout_len = static_cast<u32>(data->size());
        return cabi::kNoSpace;
    }

    std::ranges::copy(*data, out);
    *inout_len = static_cast<u32>(data->size());
    return cabi::kOk;
}

i32 FileWrite(Record& record, const char* rel_path, const u8* data, u32 len) {
    auto path = BoundedCString(rel_path, kMaxPathBytes, "path");
    if (!path) {
        return StatusFromError(path.error());
    }
    return FileWrite(record, path->data(), static_cast<u32>(path->size()), data, len);
}

i32 FileWrite(Record& record, const char* rel_path, u32 rel_path_len, const u8* data, u32 len) {
    if (data == nullptr && len != 0) {
        return cabi::kInvalid;
    }
    if (len > kMaxFileBytes) {
        return cabi::kNoSpace;
    }

    auto path = BoundedString(rel_path, rel_path_len, kMaxPathBytes, "path");
    if (!path) {
        return StatusFromError(path.error());
    }

    HostApi host(record);
    auto written = host.WriteFile(std::filesystem::path(std::string(*path)), std::span<const u8>(data, len));
    if (!written) {
        return StatusFromError(written.error());
    }
    return cabi::kOk;
}

i32 FileAppend(Record& record, const char* rel_path, const u8* data, u32 len) {
    auto path = BoundedCString(rel_path, kMaxPathBytes, "path");
    if (!path) {
        return StatusFromError(path.error());
    }
    return FileAppend(record, path->data(), static_cast<u32>(path->size()), data, len);
}

i32 FileAppend(Record& record, const char* rel_path, u32 rel_path_len, const u8* data, u32 len) {
    if (data == nullptr && len != 0) {
        return cabi::kInvalid;
    }
    if (len > kMaxFileBytes) {
        return cabi::kNoSpace;
    }

    auto path = BoundedString(rel_path, rel_path_len, kMaxPathBytes, "path");
    if (!path) {
        return StatusFromError(path.error());
    }

    HostApi host(record);
    auto appended = host.AppendFile(std::filesystem::path(std::string(*path)), std::span<const u8>(data, len));
    if (!appended) {
        return StatusFromError(appended.error());
    }
    return cabi::kOk;
}

i32 ConfigGet(Record& record, const char* key, char* out, u32 out_cap) {
    auto config_key = BoundedCString(key, kMaxConfigKeyBytes, "config key");
    if (!config_key) {
        return StatusFromError(config_key.error());
    }

    HostApi host(record);
    auto value = host.ReadConfig(*config_key);
    if (!value) {
        return StatusFromError(value.error());
    }
    return CopyString(*value, out, out_cap);
}

i32 ConfigSet(Record& record, const char* key, const char* value, u32 len) {
    auto config_key = BoundedCString(key, kMaxConfigKeyBytes, "config key");
    if (!config_key) {
        return StatusFromError(config_key.error());
    }
    auto config_value = BoundedString(value, len, kMaxConfigValueBytes, "config value");
    if (!config_value) {
        return StatusFromError(config_value.error());
    }

    HostApi host(record);
    auto written = host.WriteConfig(*config_key, *config_value);
    if (!written) {
        return StatusFromError(written.error());
    }
    return cabi::kOk;
}

} // namespace woki::ext::host::cabi
