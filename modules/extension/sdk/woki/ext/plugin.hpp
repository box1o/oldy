#pragma once

// Allocation-free C++23 facade for Woki raw ABI v1 extensions.
// Implement any supported OnLoad/OnTick/OnEvent/OnCommand/OnUnload callbacks,
// then place WOKI_PLUGIN(YourType) in exactly one translation unit.

#include <woki/ext/detail/events.hpp>
#include <woki/ext/detail/lifecycle.hpp>
#include <woki/ext/detail/allocator_export.hpp>
#include <woki/ext/detail/views_status_log.hpp>
