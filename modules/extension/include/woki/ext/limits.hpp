#pragma once

// Host implementation detail. This header is not installed.

#include <cstddef>

extern "C" {
#include "woki_limits.h"
}

namespace woki::ext::limits {

inline constexpr std::size_t kMaxLogBytes = WOKI_EXT_MAX_LOG_LEN;
inline constexpr std::size_t kMaxConfigKeyBytes = WOKI_EXT_MAX_CONFIG_KEY_LEN;
inline constexpr std::size_t kMaxConfigValueBytes = WOKI_EXT_MAX_CONFIG_VALUE_LEN;
inline constexpr std::size_t kMaxPathBytes = WOKI_EXT_MAX_PATH_LEN;
inline constexpr std::size_t kMaxEventBytes = WOKI_EXT_MAX_EVENT_LEN;
inline constexpr std::size_t kMaxEventTopicBytes = WOKI_EXT_MAX_EVENT_TOPIC_LEN;
inline constexpr std::size_t kMaxFileBytes = WOKI_EXT_MAX_FILE_RW;
inline constexpr std::size_t kMaxMemoryPages = WOKI_EXT_MAX_MEMORY_PAGES;
inline constexpr std::size_t kMaxWasmBytes = 32u * 1024u * 1024u;
inline constexpr std::size_t kGuestBufferBytes = WOKI_EXT_GUEST_BUFFER_SIZE;

} // namespace woki::ext::limits
