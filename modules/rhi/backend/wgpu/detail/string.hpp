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
    TextureUsage result = TextureUsage::None;
    if ((flags & WGPUTextureUsage_CopySrc) != 0) {
        result = result | TextureUsage::CopySrc;
    }
    if ((flags & WGPUTextureUsage_CopyDst) != 0) {
        result = result | TextureUsage::CopyDst;
    }
    if ((flags & WGPUTextureUsage_TextureBinding) != 0) {
        result = result | TextureUsage::TextureBinding;
    }
    if ((flags & WGPUTextureUsage_StorageBinding) != 0) {
        result = result | TextureUsage::StorageBinding;
    }
    if ((flags & WGPUTextureUsage_RenderAttachment) != 0) {
        result = result | TextureUsage::RenderAttachment;
    }
    if ((flags & WGPUTextureUsage_TransientAttachment) != 0) {
        result = result | TextureUsage::TransientAttachment;
    }
    if ((flags & WGPUTextureUsage_StorageAttachment) != 0) {
        result = result | TextureUsage::StorageAttachment;
    }
    return result;
}

[[nodiscard]] inline WGPUTextureUsage TextureUsageToWgpu(TextureUsage usage) noexcept {
    return static_cast<WGPUTextureUsage>(static_cast<u64>(usage));
}

} // namespace woki::rhi::wgpu::detail
