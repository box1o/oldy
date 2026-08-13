#pragma once

// Recommended allocation-free C++23 extension API. Host calls retain raw ABI
// v1 while application-event payloads use the versioned event ABI v2 header.
#include <woki/extension/slog.hpp>
#include <woki/extension/events.hpp>
#include <woki/extension/command.hpp>
#include <woki/extension/services.hpp>
#include <woki/extension/detail/export.hpp>
