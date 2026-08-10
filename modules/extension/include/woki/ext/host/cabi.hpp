#pragma once

// Host implementation detail. This header is not installed.

#include "api.hpp"

namespace woki::ext::host::cabi {

// Status codes mirror modules/extension/sdk/types.h and host/cabi.hpp.

inline constexpr i32 kOk = 0;
inline constexpr i32 kErr = -1;
inline constexpr i32 kDenied = -2;
inline constexpr i32 kNoSpace = -3;
inline constexpr i32 kNotFound = -4;
inline constexpr i32 kInvalid = -5;

[[nodiscard]] i32 Log(HostApi& host, u32 level, const char* message, u32 len);
[[nodiscard]] i32 PathData(HostApi& host, char* out, u32 out_cap);
[[nodiscard]] i32 PathCache(HostApi& host, char* out, u32 out_cap);
[[nodiscard]] i32 FileRead(HostApi& host, const char* rel_path, u8* out, u32* inout_len);
[[nodiscard]] i32 FileRead(HostApi& host, const char* rel_path, u32 rel_path_len, u8* out, u32* inout_len);
[[nodiscard]] i32 FileWrite(HostApi& host, const char* rel_path, const u8* data, u32 len);
[[nodiscard]] i32 FileWrite(HostApi& host, const char* rel_path, u32 rel_path_len, const u8* data, u32 len);
[[nodiscard]] i32 FileAppend(HostApi& host, const char* rel_path, const u8* data, u32 len);
[[nodiscard]] i32 FileAppend(HostApi& host, const char* rel_path, u32 rel_path_len, const u8* data, u32 len);
[[nodiscard]] i32 ConfigGet(HostApi& host, const char* key, char* out, u32 out_cap);
[[nodiscard]] i32 ConfigSet(HostApi& host, const char* key, const char* value, u32 len);
[[nodiscard]] i32 EventSubscribe(HostApi& host, u32 event_type);
[[nodiscard]] i32 EventEmit(HostApi& host, u32 event_type, const u8* payload, u32 len);
[[nodiscard]] i32 EventSubscribeNamed(HostApi& host, const char* name, u32 name_len);
[[nodiscard]] i32 EventEmitNamed(HostApi& host, const char* name, u32 name_len, const u8* payload, u32 len);

} // namespace woki::ext::host::cabi
