#pragma once

#include "device.hpp"

namespace woki::rhi::advanced {

// Tool/backend interop only. Runtime and GFX code must use normalized RHI APIs.
[[nodiscard]] inline NativeHandles Native(const Device& device) noexcept {
    return device.GetNativeHandles();
}

[[nodiscard]] inline NativeHandles Native(const Queue& queue) noexcept {
    return queue.GetNativeHandles();
}

} // namespace woki::rhi::advanced
