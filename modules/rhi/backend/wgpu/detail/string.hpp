#pragma once

#include <string>
#include <string_view>
#include <webgpu/webgpu.h>

#include <woki/enums.hpp>
#include <woki/rhi/types.hpp>

namespace woki::rhi::wgpu::detail {

[[nodiscard]] inline WGPUStringView ToStringView(std::string_view value) noexcept {
    return WGPUStringView{value.data(), value.size()};
}

[[nodiscard]] inline std::string StringFromView(WGPUStringView value) {
    if (value.data == nullptr || value.length == 0) {
        return {};
    }
    return std::string(value.data, value.length);
}

[[nodiscard]] inline TextureUsage TextureUsageFromWgpu(WGPUTextureUsage flags) noexcept {
    return static_cast<TextureUsage>(static_cast<u64>(flags));
}

[[nodiscard]] inline WGPUTextureUsage TextureUsageToWgpu(TextureUsage usage) noexcept {
    return static_cast<WGPUTextureUsage>(static_cast<u64>(usage));
}

} // namespace woki::rhi::wgpu::detail
