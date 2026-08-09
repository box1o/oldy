#pragma once

#include <webgpu/webgpu.h>

#include <woki/rhi/descriptors.hpp>

#include "string.hpp"
#include "../wgpu_enums.hpp"

namespace woki::rhi::wgpu::detail {

using convert::FromWgpu;

[[nodiscard]] inline CompilationInfo FromWgpuCompilationInfo(const WGPUCompilationInfo& native) {
    CompilationInfo info{};
    info.messages.reserve(native.messageCount);
    for (size_t i = 0; i < native.messageCount; ++i) {
        const WGPUCompilationMessage& message = native.messages[i];
        info.messages.push_back(CompilationMessage{
            .type = FromWgpu(message.type),
            .message = StringFromView(message.message),
            .line_num = message.lineNum,
            .line_pos = message.linePos,
            .offset = message.offset,
            .length = message.length,
        });
    }
    return info;
}

} // namespace woki::rhi::wgpu::detail
