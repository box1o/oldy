#pragma once

#include <webgpu/webgpu.h>

#include <woki/rhi/descriptors.hpp>

#include "../wgpu_enums.hpp"
#include "native_helpers.hpp"

namespace woki::rhi::wgpu::detail::copy {

using namespace woki::rhi::wgpu::convert;

[[nodiscard]] inline WGPUOrigin3D ToWgpu(const Origin3D& origin) noexcept {
    return WGPUOrigin3D{origin.x, origin.y, origin.z};
}

#ifndef __EMSCRIPTEN__
[[nodiscard]] inline WGPUOrigin2D ToWgpu(const Origin2D& origin) noexcept {
    return WGPUOrigin2D{origin.x, origin.y};
}
#endif

[[nodiscard]] inline WGPUExtent3D ToWgpu(const Extent3D& extent) noexcept {
    return WGPUExtent3D{extent.width, extent.height, extent.depth_or_array_layers};
}

#ifndef __EMSCRIPTEN__
[[nodiscard]] inline WGPUExtent2D ToWgpu(const Extent2D& extent) noexcept {
    return WGPUExtent2D{extent.width, extent.height};
}
#endif

[[nodiscard]] inline WGPUTexelCopyBufferLayout ToWgpu(const TexelCopyBufferLayout& layout) noexcept {
    WGPUTexelCopyBufferLayout native = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
    native.offset = layout.offset;
    native.bytesPerRow = layout.bytes_per_row;
    native.rowsPerImage = layout.rows_per_image;
    return native;
}

[[nodiscard]] inline WGPUTexelCopyBufferInfo ToWgpu(const TexelCopyBufferInfo& info) noexcept {
    WGPUTexelCopyBufferInfo native = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
    native.layout = ToWgpu(info.layout);
    native.buffer = info.buffer == nullptr ? nullptr : NativeBuffer(*info.buffer);
    return native;
}

[[nodiscard]] inline WGPUTexelCopyTextureInfo ToWgpu(const TexelCopyTextureInfo& info) noexcept {
    WGPUTexelCopyTextureInfo native = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    native.texture = info.texture == nullptr ? nullptr : NativeTexture(*info.texture);
    native.mipLevel = info.mip_level;
    native.origin = ToWgpu(info.origin);
    native.aspect = convert::ToWgpu(info.aspect);
    return native;
}

#ifndef __EMSCRIPTEN__
[[nodiscard]] inline WGPUCopyTextureForBrowserOptions ToWgpu(const CopyTextureForBrowserOptions& options) noexcept {
    WGPUCopyTextureForBrowserOptions native = WGPU_COPY_TEXTURE_FOR_BROWSER_OPTIONS_INIT;
    native.nextInChain = static_cast<WGPUChainedStruct*>(options.next_in_chain);
    native.flipY = options.flip_y;
    native.needsColorSpaceConversion = options.needs_color_space_conversion;
    native.srcAlphaMode = convert::ToWgpu(options.src_alpha_mode);
    native.srcTransferFunctionParameters = options.src_transfer_function_parameters;
    native.conversionMatrix = options.conversion_matrix;
    native.dstTransferFunctionParameters = options.dst_transfer_function_parameters;
    native.dstAlphaMode = convert::ToWgpu(options.dst_alpha_mode);
    native.internalUsage = options.internal_usage;
    return native;
}

[[nodiscard]] inline WGPUImageCopyExternalTexture ToWgpu(const ImageCopyExternalTexture& source) noexcept {
    WGPUImageCopyExternalTexture native = WGPU_IMAGE_COPY_EXTERNAL_TEXTURE_INIT;
    native.externalTexture = static_cast<WGPUExternalTexture>(source.external_texture);
    native.origin = ToWgpu(source.origin);
    native.naturalSize = ToWgpu(source.natural_size);
    return native;
}
#endif

} // namespace woki::rhi::wgpu::detail::copy
