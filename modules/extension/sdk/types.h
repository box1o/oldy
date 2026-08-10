#pragma once

#include <stdint.h>

typedef uint32_t woki_ext_log_level_t;
typedef uint32_t woki_ext_event_type_t;

// Type 0 subscribes to every event. Guest emissions must set the high bit;
// host event types occupy the remaining namespace.
#define WOKI_EXT_EVENT_WILDCARD ((woki_ext_event_type_t)0u)
#define WOKI_EXT_EVENT_NAMESPACE ((woki_ext_event_type_t)0x80000000u)

enum {
    WOKI_EXT_LOG_DEBUG = 0,
    WOKI_EXT_LOG_INFO = 1,
    WOKI_EXT_LOG_WARN = 2,
    WOKI_EXT_LOG_ERROR = 3,
};

enum {
    WOKI_EXT_OK = 0,
    WOKI_EXT_ERR = -1,
    WOKI_EXT_DENIED = -2,
    WOKI_EXT_NO_SPACE = -3,
    WOKI_EXT_NOT_FOUND = -4,
    WOKI_EXT_INVALID = -5,
};
