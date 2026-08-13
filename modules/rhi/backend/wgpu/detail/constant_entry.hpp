#pragma once

#include <string>
#include <vector>
#include <webgpu/webgpu.h>

#include <woki/rhi/descriptors.hpp>

#include "string.hpp"

namespace woki::rhi::wgpu::detail {

struct ConstantEntryStorage final {
    std::vector<std::string> keys{};
    std::vector<WGPUConstantEntry> entries{};

    explicit ConstantEntryStorage(const std::span<const ConstantEntryDesc> constants) {
        keys.reserve(constants.size());
        entries.reserve(constants.size());
        for (const ConstantEntryDesc& constant : constants) {
            keys.push_back(constant.key);
            WGPUConstantEntry native = WGPU_CONSTANT_ENTRY_INIT;
            native.key = ToStringView(keys.back());
            native.value = constant.value;
            entries.push_back(native);
        }
    }
};

} // namespace woki::rhi::wgpu::detail
