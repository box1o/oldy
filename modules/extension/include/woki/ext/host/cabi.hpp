#pragma once

// IWYU pragma: private, include "woki/ext/ext.hpp"

#include "api.hpp"

namespace woki::ext::host::cabi {

// Status codes mirror modules/extension/sdk/types.h and host/cabi.hpp.

inline constexpr i32 kOk = 0;
inline constexpr i32 kErr = -1;
inline constexpr i32 kDenied = -2;
inline constexpr i32 kNoSpace = -3;
inline constexpr i32 kNotFound = -4;
inline constexpr i32 kInvalid = -5;

[[nodiscard]] i32 Log(Record& record, u32 level, const char* message, u32 len);
[[nodiscard]] i32 PathData(Record& record, char* out, u32 out_cap);
[[nodiscard]] i32 PathCache(Record& record, char* out, u32 out_cap);
[[nodiscard]] i32 FileRead(Record& record, const char* rel_path, u8* out, u32* inout_len);
[[nodiscard]] i32 FileRead(Record& record, const char* rel_path, u32 rel_path_len, u8* out, u32* inout_len);
[[nodiscard]] i32 FileWrite(Record& record, const char* rel_path, const u8* data, u32 len);
[[nodiscard]] i32 FileWrite(Record& record, const char* rel_path, u32 rel_path_len, const u8* data, u32 len);
[[nodiscard]] i32 FileAppend(Record& record, const char* rel_path, const u8* data, u32 len);
[[nodiscard]] i32 FileAppend(Record& record, const char* rel_path, u32 rel_path_len, const u8* data, u32 len);
[[nodiscard]] i32 ConfigGet(Record& record, const char* key, char* out, u32 out_cap);
[[nodiscard]] i32 ConfigSet(Record& record, const char* key, const char* value, u32 len);

} // namespace woki::ext::host::cabi
