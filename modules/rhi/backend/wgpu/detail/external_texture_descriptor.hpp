#pragma once

#include <webgpu/webgpu.h>

#include <woki/rhi/descriptors.hpp>

#include "string.hpp"
#include "copy_convert.hpp"
#include "../wgpu_enums.hpp"
#include "native_helpers.hpp"

namespace woki::rhi::wgpu::detail {

using convert::ToWgpu;

#ifndef __EMSCRIPTEN__
struct ExternalTextureDescriptorStorage final {
    WGPUExternalTextureDescriptor native = WGPU_EXTERNAL_TEXTURE_DESCRIPTOR_INIT;

    explicit ExternalTextureDescriptorStorage(const ExternalTextureDesc& desc) {
        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
        native.label = ToStringView(desc.label);
        native.plane0 = NativeTextureView(desc.plane0);
        native.plane1 = NativeTextureView(desc.plane1);
        native.cropOrigin = copy::ToWgpu(desc.crop_origin);
        native.cropSize = copy::ToWgpu(desc.crop_size);
        native.apparentSize = copy::ToWgpu(desc.apparent_size);
        native.doYuvToRgbConversionOnly = desc.do_yuv_to_rgb_conversion_only ? WGPU_TRUE : WGPU_FALSE;
        native.yuvToRgbConversionMatrix = desc.yuv_to_rgb_conversion_matrix;
        native.srcTransferFunctionParameters = desc.src_transfer_function_parameters;
        native.dstTransferFunctionParameters = desc.dst_transfer_function_parameters;
        native.gamutConversionMatrix = desc.gamut_conversion_matrix;
        native.mirrored = desc.mirrored ? WGPU_TRUE : WGPU_FALSE;
        native.rotation = ToWgpu(desc.rotation);
    }
};
#endif

} // namespace woki::rhi::wgpu::detail
