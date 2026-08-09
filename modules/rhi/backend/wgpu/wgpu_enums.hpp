#pragma once

#include <webgpu/webgpu.h>

#include "../../include/woki/enums.hpp"

namespace woki::rhi::wgpu::convert {

static_assert(static_cast<u64>(BufferUsage::None) == WGPUBufferUsage_None);
static_assert(static_cast<u64>(BufferUsage::MapRead) == WGPUBufferUsage_MapRead);
static_assert(static_cast<u64>(BufferUsage::MapWrite) == WGPUBufferUsage_MapWrite);
static_assert(static_cast<u64>(BufferUsage::CopySrc) == WGPUBufferUsage_CopySrc);
static_assert(static_cast<u64>(BufferUsage::CopyDst) == WGPUBufferUsage_CopyDst);
static_assert(static_cast<u64>(BufferUsage::Index) == WGPUBufferUsage_Index);
static_assert(static_cast<u64>(BufferUsage::Vertex) == WGPUBufferUsage_Vertex);
static_assert(static_cast<u64>(BufferUsage::Uniform) == WGPUBufferUsage_Uniform);
static_assert(static_cast<u64>(BufferUsage::Storage) == WGPUBufferUsage_Storage);
static_assert(static_cast<u64>(BufferUsage::Indirect) == WGPUBufferUsage_Indirect);
static_assert(static_cast<u64>(BufferUsage::QueryResolve) == WGPUBufferUsage_QueryResolve);
static_assert(static_cast<u64>(BufferUsage::TexelBuffer) == WGPUBufferUsage_TexelBuffer);

static_assert(static_cast<u64>(ShaderStage::None) == WGPUShaderStage_None);
static_assert(static_cast<u64>(ShaderStage::Vertex) == WGPUShaderStage_Vertex);
static_assert(static_cast<u64>(ShaderStage::Fragment) == WGPUShaderStage_Fragment);
static_assert(static_cast<u64>(ShaderStage::Compute) == WGPUShaderStage_Compute);

static_assert(static_cast<u64>(TextureUsage::None) == WGPUTextureUsage_None);
static_assert(static_cast<u64>(TextureUsage::CopySrc) == WGPUTextureUsage_CopySrc);
static_assert(static_cast<u64>(TextureUsage::CopyDst) == WGPUTextureUsage_CopyDst);
static_assert(static_cast<u64>(TextureUsage::TextureBinding) == WGPUTextureUsage_TextureBinding);
static_assert(static_cast<u64>(TextureUsage::StorageBinding) == WGPUTextureUsage_StorageBinding);
static_assert(static_cast<u64>(TextureUsage::RenderAttachment) == WGPUTextureUsage_RenderAttachment);
static_assert(static_cast<u64>(TextureUsage::TransientAttachment) == WGPUTextureUsage_TransientAttachment);
static_assert(static_cast<u64>(TextureUsage::StorageAttachment) == WGPUTextureUsage_StorageAttachment);

static_assert(static_cast<u64>(ColorWriteMask::None) == WGPUColorWriteMask_None);
static_assert(static_cast<u64>(ColorWriteMask::Red) == WGPUColorWriteMask_Red);
static_assert(static_cast<u64>(ColorWriteMask::Green) == WGPUColorWriteMask_Green);
static_assert(static_cast<u64>(ColorWriteMask::Blue) == WGPUColorWriteMask_Blue);
static_assert(static_cast<u64>(ColorWriteMask::Alpha) == WGPUColorWriteMask_Alpha);
static_assert(static_cast<u64>(ColorWriteMask::All) == WGPUColorWriteMask_All);

static_assert(static_cast<u64>(HeapProperty::None) == WGPUHeapProperty_None);
static_assert(static_cast<u64>(HeapProperty::DeviceLocal) == WGPUHeapProperty_DeviceLocal);
static_assert(static_cast<u64>(HeapProperty::HostVisible) == WGPUHeapProperty_HostVisible);
static_assert(static_cast<u64>(HeapProperty::HostCoherent) == WGPUHeapProperty_HostCoherent);
static_assert(static_cast<u64>(HeapProperty::HostUncached) == WGPUHeapProperty_HostUncached);
static_assert(static_cast<u64>(HeapProperty::HostCached) == WGPUHeapProperty_HostCached);

static_assert(static_cast<u64>(MapMode::None) == WGPUMapMode_None);
static_assert(static_cast<u64>(MapMode::Read) == WGPUMapMode_Read);
static_assert(static_cast<u64>(MapMode::Write) == WGPUMapMode_Write);

[[nodiscard]] inline WGPUAdapterType ToWgpu(AdapterType value) noexcept {
    switch (value) {
        case AdapterType::DiscreteGPU:
            return WGPUAdapterType_DiscreteGPU;
        case AdapterType::IntegratedGPU:
            return WGPUAdapterType_IntegratedGPU;
        case AdapterType::CPU:
            return WGPUAdapterType_CPU;
        case AdapterType::Unknown:
            return WGPUAdapterType_Unknown;
    }
    return WGPUAdapterType_Unknown;
}

[[nodiscard]] inline AdapterType FromWgpu(WGPUAdapterType value) noexcept {
    switch (value) {
        case WGPUAdapterType_DiscreteGPU:
            return AdapterType::DiscreteGPU;
        case WGPUAdapterType_IntegratedGPU:
            return AdapterType::IntegratedGPU;
        case WGPUAdapterType_CPU:
            return AdapterType::CPU;
        case WGPUAdapterType_Unknown:
            return AdapterType::Unknown;
        default:
            return AdapterType::Unknown;
    }
}

[[nodiscard]] inline WGPUAddressMode ToWgpu(AddressMode value) noexcept {
    switch (value) {
        case AddressMode::Undefined:
            return WGPUAddressMode_Undefined;
        case AddressMode::ClampToEdge:
            return WGPUAddressMode_ClampToEdge;
        case AddressMode::Repeat:
            return WGPUAddressMode_Repeat;
        case AddressMode::MirrorRepeat:
            return WGPUAddressMode_MirrorRepeat;
    }
    return WGPUAddressMode_Undefined;
}

[[nodiscard]] inline AddressMode FromWgpu(WGPUAddressMode value) noexcept {
    switch (value) {
        case WGPUAddressMode_Undefined:
            return AddressMode::Undefined;
        case WGPUAddressMode_ClampToEdge:
            return AddressMode::ClampToEdge;
        case WGPUAddressMode_Repeat:
            return AddressMode::Repeat;
        case WGPUAddressMode_MirrorRepeat:
            return AddressMode::MirrorRepeat;
        default:
            return AddressMode::Undefined;
    }
}

[[nodiscard]] inline WGPUAlphaMode ToWgpu(AlphaMode value) noexcept {
    switch (value) {
        case AlphaMode::Opaque:
            return WGPUAlphaMode_Opaque;
        case AlphaMode::Premultiplied:
            return WGPUAlphaMode_Premultiplied;
        case AlphaMode::Unpremultiplied:
            return WGPUAlphaMode_Unpremultiplied;
    }
    return WGPUAlphaMode_Opaque;
}

[[nodiscard]] inline AlphaMode FromWgpu(WGPUAlphaMode value) noexcept {
    switch (value) {
        case WGPUAlphaMode_Opaque:
            return AlphaMode::Opaque;
        case WGPUAlphaMode_Premultiplied:
            return AlphaMode::Premultiplied;
        case WGPUAlphaMode_Unpremultiplied:
            return AlphaMode::Unpremultiplied;
        default:
            return AlphaMode::Opaque;
    }
}

[[nodiscard]] inline WGPUBackendType ToWgpu(BackendType value) noexcept {
    switch (value) {
        case BackendType::Undefined:
            return WGPUBackendType_Undefined;
        case BackendType::Null:
            return WGPUBackendType_Null;
        case BackendType::WebGPU:
            return WGPUBackendType_WebGPU;
        case BackendType::D3D11:
            return WGPUBackendType_D3D11;
        case BackendType::D3D12:
            return WGPUBackendType_D3D12;
        case BackendType::Metal:
            return WGPUBackendType_Metal;
        case BackendType::Vulkan:
            return WGPUBackendType_Vulkan;
        case BackendType::OpenGL:
            return WGPUBackendType_OpenGL;
        case BackendType::OpenGLES:
            return WGPUBackendType_OpenGLES;
    }
    return WGPUBackendType_Undefined;
}

[[nodiscard]] inline BackendType FromWgpu(WGPUBackendType value) noexcept {
    switch (value) {
        case WGPUBackendType_Undefined:
            return BackendType::Undefined;
        case WGPUBackendType_Null:
            return BackendType::Null;
        case WGPUBackendType_WebGPU:
            return BackendType::WebGPU;
        case WGPUBackendType_D3D11:
            return BackendType::D3D11;
        case WGPUBackendType_D3D12:
            return BackendType::D3D12;
        case WGPUBackendType_Metal:
            return BackendType::Metal;
        case WGPUBackendType_Vulkan:
            return BackendType::Vulkan;
        case WGPUBackendType_OpenGL:
            return BackendType::OpenGL;
        case WGPUBackendType_OpenGLES:
            return BackendType::OpenGLES;
        default:
            return BackendType::Undefined;
    }
}

[[nodiscard]] inline WGPUBlendFactor ToWgpu(BlendFactor value) noexcept {
    switch (value) {
        case BlendFactor::Undefined:
            return WGPUBlendFactor_Undefined;
        case BlendFactor::Zero:
            return WGPUBlendFactor_Zero;
        case BlendFactor::One:
            return WGPUBlendFactor_One;
        case BlendFactor::Src:
            return WGPUBlendFactor_Src;
        case BlendFactor::OneMinusSrc:
            return WGPUBlendFactor_OneMinusSrc;
        case BlendFactor::SrcAlpha:
            return WGPUBlendFactor_SrcAlpha;
        case BlendFactor::OneMinusSrcAlpha:
            return WGPUBlendFactor_OneMinusSrcAlpha;
        case BlendFactor::Dst:
            return WGPUBlendFactor_Dst;
        case BlendFactor::OneMinusDst:
            return WGPUBlendFactor_OneMinusDst;
        case BlendFactor::DstAlpha:
            return WGPUBlendFactor_DstAlpha;
        case BlendFactor::OneMinusDstAlpha:
            return WGPUBlendFactor_OneMinusDstAlpha;
        case BlendFactor::SrcAlphaSaturated:
            return WGPUBlendFactor_SrcAlphaSaturated;
        case BlendFactor::Constant:
            return WGPUBlendFactor_Constant;
        case BlendFactor::OneMinusConstant:
            return WGPUBlendFactor_OneMinusConstant;
        case BlendFactor::Src1:
            return WGPUBlendFactor_Src1;
        case BlendFactor::OneMinusSrc1:
            return WGPUBlendFactor_OneMinusSrc1;
        case BlendFactor::Src1Alpha:
            return WGPUBlendFactor_Src1Alpha;
        case BlendFactor::OneMinusSrc1Alpha:
            return WGPUBlendFactor_OneMinusSrc1Alpha;
    }
    return WGPUBlendFactor_Undefined;
}

[[nodiscard]] inline BlendFactor FromWgpu(WGPUBlendFactor value) noexcept {
    switch (value) {
        case WGPUBlendFactor_Undefined:
            return BlendFactor::Undefined;
        case WGPUBlendFactor_Zero:
            return BlendFactor::Zero;
        case WGPUBlendFactor_One:
            return BlendFactor::One;
        case WGPUBlendFactor_Src:
            return BlendFactor::Src;
        case WGPUBlendFactor_OneMinusSrc:
            return BlendFactor::OneMinusSrc;
        case WGPUBlendFactor_SrcAlpha:
            return BlendFactor::SrcAlpha;
        case WGPUBlendFactor_OneMinusSrcAlpha:
            return BlendFactor::OneMinusSrcAlpha;
        case WGPUBlendFactor_Dst:
            return BlendFactor::Dst;
        case WGPUBlendFactor_OneMinusDst:
            return BlendFactor::OneMinusDst;
        case WGPUBlendFactor_DstAlpha:
            return BlendFactor::DstAlpha;
        case WGPUBlendFactor_OneMinusDstAlpha:
            return BlendFactor::OneMinusDstAlpha;
        case WGPUBlendFactor_SrcAlphaSaturated:
            return BlendFactor::SrcAlphaSaturated;
        case WGPUBlendFactor_Constant:
            return BlendFactor::Constant;
        case WGPUBlendFactor_OneMinusConstant:
            return BlendFactor::OneMinusConstant;
        case WGPUBlendFactor_Src1:
            return BlendFactor::Src1;
        case WGPUBlendFactor_OneMinusSrc1:
            return BlendFactor::OneMinusSrc1;
        case WGPUBlendFactor_Src1Alpha:
            return BlendFactor::Src1Alpha;
        case WGPUBlendFactor_OneMinusSrc1Alpha:
            return BlendFactor::OneMinusSrc1Alpha;
        default:
            return BlendFactor::Undefined;
    }
}

[[nodiscard]] inline WGPUBlendOperation ToWgpu(BlendOperation value) noexcept {
    switch (value) {
        case BlendOperation::Undefined:
            return WGPUBlendOperation_Undefined;
        case BlendOperation::Add:
            return WGPUBlendOperation_Add;
        case BlendOperation::Subtract:
            return WGPUBlendOperation_Subtract;
        case BlendOperation::ReverseSubtract:
            return WGPUBlendOperation_ReverseSubtract;
        case BlendOperation::Min:
            return WGPUBlendOperation_Min;
        case BlendOperation::Max:
            return WGPUBlendOperation_Max;
    }
    return WGPUBlendOperation_Undefined;
}

[[nodiscard]] inline BlendOperation FromWgpu(WGPUBlendOperation value) noexcept {
    switch (value) {
        case WGPUBlendOperation_Undefined:
            return BlendOperation::Undefined;
        case WGPUBlendOperation_Add:
            return BlendOperation::Add;
        case WGPUBlendOperation_Subtract:
            return BlendOperation::Subtract;
        case WGPUBlendOperation_ReverseSubtract:
            return BlendOperation::ReverseSubtract;
        case WGPUBlendOperation_Min:
            return BlendOperation::Min;
        case WGPUBlendOperation_Max:
            return BlendOperation::Max;
        default:
            return BlendOperation::Undefined;
    }
}

[[nodiscard]] inline WGPUBufferBindingType ToWgpu(BufferBindingType value) noexcept {
    switch (value) {
        case BufferBindingType::BindingNotUsed:
            return WGPUBufferBindingType_BindingNotUsed;
        case BufferBindingType::Undefined:
            return WGPUBufferBindingType_Undefined;
        case BufferBindingType::Uniform:
            return WGPUBufferBindingType_Uniform;
        case BufferBindingType::Storage:
            return WGPUBufferBindingType_Storage;
        case BufferBindingType::ReadOnlyStorage:
            return WGPUBufferBindingType_ReadOnlyStorage;
    }
    return WGPUBufferBindingType_Undefined;
}

[[nodiscard]] inline BufferBindingType FromWgpu(WGPUBufferBindingType value) noexcept {
    switch (value) {
        case WGPUBufferBindingType_BindingNotUsed:
            return BufferBindingType::BindingNotUsed;
        case WGPUBufferBindingType_Undefined:
            return BufferBindingType::Undefined;
        case WGPUBufferBindingType_Uniform:
            return BufferBindingType::Uniform;
        case WGPUBufferBindingType_Storage:
            return BufferBindingType::Storage;
        case WGPUBufferBindingType_ReadOnlyStorage:
            return BufferBindingType::ReadOnlyStorage;
        default:
            return BufferBindingType::Undefined;
    }
}

[[nodiscard]] inline WGPUBufferMapState ToWgpu(BufferMapState value) noexcept {
    switch (value) {
        case BufferMapState::Unmapped:
            return WGPUBufferMapState_Unmapped;
        case BufferMapState::Pending:
            return WGPUBufferMapState_Pending;
        case BufferMapState::Mapped:
            return WGPUBufferMapState_Mapped;
    }
    return WGPUBufferMapState_Unmapped;
}

[[nodiscard]] inline BufferMapState FromWgpu(WGPUBufferMapState value) noexcept {
    switch (value) {
        case WGPUBufferMapState_Unmapped:
            return BufferMapState::Unmapped;
        case WGPUBufferMapState_Pending:
            return BufferMapState::Pending;
        case WGPUBufferMapState_Mapped:
            return BufferMapState::Mapped;
        default:
            return BufferMapState::Unmapped;
    }
}

[[nodiscard]] inline WGPUCallbackMode ToWgpu(CallbackMode value) noexcept {
    switch (value) {
        case CallbackMode::WaitAnyOnly:
            return WGPUCallbackMode_WaitAnyOnly;
        case CallbackMode::AllowProcessEvents:
            return WGPUCallbackMode_AllowProcessEvents;
        case CallbackMode::AllowSpontaneous:
            return WGPUCallbackMode_AllowSpontaneous;
    }
    return WGPUCallbackMode_WaitAnyOnly;
}

[[nodiscard]] inline CallbackMode FromWgpu(WGPUCallbackMode value) noexcept {
    switch (value) {
        case WGPUCallbackMode_WaitAnyOnly:
            return CallbackMode::WaitAnyOnly;
        case WGPUCallbackMode_AllowProcessEvents:
            return CallbackMode::AllowProcessEvents;
        case WGPUCallbackMode_AllowSpontaneous:
            return CallbackMode::AllowSpontaneous;
        default:
            return CallbackMode::WaitAnyOnly;
    }
}

[[nodiscard]] inline WGPUColorSpacePrimariesDawn ToWgpu(ColorSpacePrimariesDawn value) noexcept {
    switch (value) {
        case ColorSpacePrimariesDawn::SRGB:
            return WGPUColorSpacePrimariesDawn_SRGB;
        case ColorSpacePrimariesDawn::Rec709:
            return WGPUColorSpacePrimariesDawn_Rec709;
        case ColorSpacePrimariesDawn::Rec601:
            return WGPUColorSpacePrimariesDawn_Rec601;
        case ColorSpacePrimariesDawn::Rec2020:
            return WGPUColorSpacePrimariesDawn_Rec2020;
        case ColorSpacePrimariesDawn::DisplayP3:
            return WGPUColorSpacePrimariesDawn_DisplayP3;
    }
    return WGPUColorSpacePrimariesDawn_SRGB;
}

[[nodiscard]] inline ColorSpacePrimariesDawn FromWgpu(WGPUColorSpacePrimariesDawn value) noexcept {
    // SRGB and Rec709 share the same native enum value in webgpu.h.
    if (value == WGPUColorSpacePrimariesDawn_SRGB) {
        return ColorSpacePrimariesDawn::SRGB;
    }

    switch (value) {
        case WGPUColorSpacePrimariesDawn_Rec601:
            return ColorSpacePrimariesDawn::Rec601;
        case WGPUColorSpacePrimariesDawn_Rec2020:
            return ColorSpacePrimariesDawn::Rec2020;
        case WGPUColorSpacePrimariesDawn_DisplayP3:
            return ColorSpacePrimariesDawn::DisplayP3;
        default:
            return ColorSpacePrimariesDawn::SRGB;
    }
}

[[nodiscard]] inline WGPUColorSpaceTransferDawn ToWgpu(ColorSpaceTransferDawn value) noexcept {
    switch (value) {
        case ColorSpaceTransferDawn::Identity:
            return WGPUColorSpaceTransferDawn_Identity;
        case ColorSpaceTransferDawn::SRGB:
            return WGPUColorSpaceTransferDawn_SRGB;
        case ColorSpaceTransferDawn::DisplayP3:
            return WGPUColorSpaceTransferDawn_DisplayP3;
        case ColorSpaceTransferDawn::SMPTE_170M:
            return WGPUColorSpaceTransferDawn_SMPTE_170M;
        case ColorSpaceTransferDawn::HLG:
            return WGPUColorSpaceTransferDawn_HLG;
        case ColorSpaceTransferDawn::PQ:
            return WGPUColorSpaceTransferDawn_PQ;
    }
    return WGPUColorSpaceTransferDawn_Identity;
}

[[nodiscard]] inline ColorSpaceTransferDawn FromWgpu(WGPUColorSpaceTransferDawn value) noexcept {
    switch (value) {
        case WGPUColorSpaceTransferDawn_Identity:
            return ColorSpaceTransferDawn::Identity;
        case WGPUColorSpaceTransferDawn_SRGB:
            return ColorSpaceTransferDawn::SRGB;
        case WGPUColorSpaceTransferDawn_DisplayP3:
            return ColorSpaceTransferDawn::DisplayP3;
        case WGPUColorSpaceTransferDawn_SMPTE_170M:
            return ColorSpaceTransferDawn::SMPTE_170M;
        case WGPUColorSpaceTransferDawn_HLG:
            return ColorSpaceTransferDawn::HLG;
        case WGPUColorSpaceTransferDawn_PQ:
            return ColorSpaceTransferDawn::PQ;
        default:
            return ColorSpaceTransferDawn::Identity;
    }
}

[[nodiscard]] inline WGPUColorSpaceYCbCrMatrixDawn ToWgpu(ColorSpaceYCbCrMatrixDawn value) noexcept {
    switch (value) {
        case ColorSpaceYCbCrMatrixDawn::Identity:
            return WGPUColorSpaceYCbCrMatrixDawn_Identity;
        case ColorSpaceYCbCrMatrixDawn::Rec601:
            return WGPUColorSpaceYCbCrMatrixDawn_Rec601;
        case ColorSpaceYCbCrMatrixDawn::Rec709:
            return WGPUColorSpaceYCbCrMatrixDawn_Rec709;
        case ColorSpaceYCbCrMatrixDawn::Rec2020:
            return WGPUColorSpaceYCbCrMatrixDawn_Rec2020;
    }
    return WGPUColorSpaceYCbCrMatrixDawn_Identity;
}

[[nodiscard]] inline ColorSpaceYCbCrMatrixDawn FromWgpu(WGPUColorSpaceYCbCrMatrixDawn value) noexcept {
    switch (value) {
        case WGPUColorSpaceYCbCrMatrixDawn_Identity:
            return ColorSpaceYCbCrMatrixDawn::Identity;
        case WGPUColorSpaceYCbCrMatrixDawn_Rec601:
            return ColorSpaceYCbCrMatrixDawn::Rec601;
        case WGPUColorSpaceYCbCrMatrixDawn_Rec709:
            return ColorSpaceYCbCrMatrixDawn::Rec709;
        case WGPUColorSpaceYCbCrMatrixDawn_Rec2020:
            return ColorSpaceYCbCrMatrixDawn::Rec2020;
        default:
            return ColorSpaceYCbCrMatrixDawn::Identity;
    }
}

[[nodiscard]] inline WGPUColorSpaceYCbCrRangeDawn ToWgpu(ColorSpaceYCbCrRangeDawn value) noexcept {
    switch (value) {
        case ColorSpaceYCbCrRangeDawn::Identity:
            return WGPUColorSpaceYCbCrRangeDawn_Identity;
        case ColorSpaceYCbCrRangeDawn::Narrow:
            return WGPUColorSpaceYCbCrRangeDawn_Narrow;
        case ColorSpaceYCbCrRangeDawn::Full:
            return WGPUColorSpaceYCbCrRangeDawn_Full;
    }
    return WGPUColorSpaceYCbCrRangeDawn_Identity;
}

[[nodiscard]] inline ColorSpaceYCbCrRangeDawn FromWgpu(WGPUColorSpaceYCbCrRangeDawn value) noexcept {
    switch (value) {
        case WGPUColorSpaceYCbCrRangeDawn_Identity:
            return ColorSpaceYCbCrRangeDawn::Identity;
        case WGPUColorSpaceYCbCrRangeDawn_Narrow:
            return ColorSpaceYCbCrRangeDawn::Narrow;
        case WGPUColorSpaceYCbCrRangeDawn_Full:
            return ColorSpaceYCbCrRangeDawn::Full;
        default:
            return ColorSpaceYCbCrRangeDawn::Identity;
    }
}

[[nodiscard]] inline WGPUCompareFunction ToWgpu(CompareFunction value) noexcept {
    switch (value) {
        case CompareFunction::Undefined:
            return WGPUCompareFunction_Undefined;
        case CompareFunction::Never:
            return WGPUCompareFunction_Never;
        case CompareFunction::Less:
            return WGPUCompareFunction_Less;
        case CompareFunction::Equal:
            return WGPUCompareFunction_Equal;
        case CompareFunction::LessEqual:
            return WGPUCompareFunction_LessEqual;
        case CompareFunction::Greater:
            return WGPUCompareFunction_Greater;
        case CompareFunction::NotEqual:
            return WGPUCompareFunction_NotEqual;
        case CompareFunction::GreaterEqual:
            return WGPUCompareFunction_GreaterEqual;
        case CompareFunction::Always:
            return WGPUCompareFunction_Always;
    }
    return WGPUCompareFunction_Undefined;
}

[[nodiscard]] inline CompareFunction FromWgpu(WGPUCompareFunction value) noexcept {
    switch (value) {
        case WGPUCompareFunction_Undefined:
            return CompareFunction::Undefined;
        case WGPUCompareFunction_Never:
            return CompareFunction::Never;
        case WGPUCompareFunction_Less:
            return CompareFunction::Less;
        case WGPUCompareFunction_Equal:
            return CompareFunction::Equal;
        case WGPUCompareFunction_LessEqual:
            return CompareFunction::LessEqual;
        case WGPUCompareFunction_Greater:
            return CompareFunction::Greater;
        case WGPUCompareFunction_NotEqual:
            return CompareFunction::NotEqual;
        case WGPUCompareFunction_GreaterEqual:
            return CompareFunction::GreaterEqual;
        case WGPUCompareFunction_Always:
            return CompareFunction::Always;
        default:
            return CompareFunction::Undefined;
    }
}

[[nodiscard]] inline WGPUCompilationInfoRequestStatus ToWgpu(CompilationInfoRequestStatus value) noexcept {
    switch (value) {
        case CompilationInfoRequestStatus::Success:
            return WGPUCompilationInfoRequestStatus_Success;
        case CompilationInfoRequestStatus::CallbackCancelled:
            return WGPUCompilationInfoRequestStatus_CallbackCancelled;
    }
    return WGPUCompilationInfoRequestStatus_CallbackCancelled;
}

[[nodiscard]] inline CompilationInfoRequestStatus FromWgpu(WGPUCompilationInfoRequestStatus value) noexcept {
    switch (value) {
        case WGPUCompilationInfoRequestStatus_Success:
            return CompilationInfoRequestStatus::Success;
        case WGPUCompilationInfoRequestStatus_CallbackCancelled:
            return CompilationInfoRequestStatus::CallbackCancelled;
        default:
            return CompilationInfoRequestStatus::CallbackCancelled;
    }
}

[[nodiscard]] inline WGPUCompilationMessageType ToWgpu(CompilationMessageType value) noexcept {
    switch (value) {
        case CompilationMessageType::Error:
            return WGPUCompilationMessageType_Error;
        case CompilationMessageType::Warning:
            return WGPUCompilationMessageType_Warning;
        case CompilationMessageType::Info:
            return WGPUCompilationMessageType_Info;
    }
    return WGPUCompilationMessageType_Error;
}

[[nodiscard]] inline CompilationMessageType FromWgpu(WGPUCompilationMessageType value) noexcept {
    switch (value) {
        case WGPUCompilationMessageType_Error:
            return CompilationMessageType::Error;
        case WGPUCompilationMessageType_Warning:
            return CompilationMessageType::Warning;
        case WGPUCompilationMessageType_Info:
            return CompilationMessageType::Info;
        default:
            return CompilationMessageType::Error;
    }
}

[[nodiscard]] inline WGPUComponentSwizzle ToWgpu(ComponentSwizzle value) noexcept {
    switch (value) {
        case ComponentSwizzle::Undefined:
            return WGPUComponentSwizzle_Undefined;
        case ComponentSwizzle::Zero:
            return WGPUComponentSwizzle_Zero;
        case ComponentSwizzle::One:
            return WGPUComponentSwizzle_One;
        case ComponentSwizzle::R:
            return WGPUComponentSwizzle_R;
        case ComponentSwizzle::G:
            return WGPUComponentSwizzle_G;
        case ComponentSwizzle::B:
            return WGPUComponentSwizzle_B;
        case ComponentSwizzle::A:
            return WGPUComponentSwizzle_A;
    }
    return WGPUComponentSwizzle_Undefined;
}

[[nodiscard]] inline ComponentSwizzle FromWgpu(WGPUComponentSwizzle value) noexcept {
    switch (value) {
        case WGPUComponentSwizzle_Undefined:
            return ComponentSwizzle::Undefined;
        case WGPUComponentSwizzle_Zero:
            return ComponentSwizzle::Zero;
        case WGPUComponentSwizzle_One:
            return ComponentSwizzle::One;
        case WGPUComponentSwizzle_R:
            return ComponentSwizzle::R;
        case WGPUComponentSwizzle_G:
            return ComponentSwizzle::G;
        case WGPUComponentSwizzle_B:
            return ComponentSwizzle::B;
        case WGPUComponentSwizzle_A:
            return ComponentSwizzle::A;
        default:
            return ComponentSwizzle::Undefined;
    }
}

[[nodiscard]] inline WGPUCompositeAlphaMode ToWgpu(CompositeAlphaMode value) noexcept {
    switch (value) {
        case CompositeAlphaMode::Auto:
            return WGPUCompositeAlphaMode_Auto;
        case CompositeAlphaMode::Opaque:
            return WGPUCompositeAlphaMode_Opaque;
        case CompositeAlphaMode::Premultiplied:
            return WGPUCompositeAlphaMode_Premultiplied;
        case CompositeAlphaMode::Unpremultiplied:
            return WGPUCompositeAlphaMode_Unpremultiplied;
        case CompositeAlphaMode::Inherit:
            return WGPUCompositeAlphaMode_Inherit;
    }
    return WGPUCompositeAlphaMode_Auto;
}

[[nodiscard]] inline CompositeAlphaMode FromWgpu(WGPUCompositeAlphaMode value) noexcept {
    switch (value) {
        case WGPUCompositeAlphaMode_Auto:
            return CompositeAlphaMode::Auto;
        case WGPUCompositeAlphaMode_Opaque:
            return CompositeAlphaMode::Opaque;
        case WGPUCompositeAlphaMode_Premultiplied:
            return CompositeAlphaMode::Premultiplied;
        case WGPUCompositeAlphaMode_Unpremultiplied:
            return CompositeAlphaMode::Unpremultiplied;
        case WGPUCompositeAlphaMode_Inherit:
            return CompositeAlphaMode::Inherit;
        default:
            return CompositeAlphaMode::Auto;
    }
}

[[nodiscard]] inline WGPUCreatePipelineAsyncStatus ToWgpu(CreatePipelineAsyncStatus value) noexcept {
    switch (value) {
        case CreatePipelineAsyncStatus::Success:
            return WGPUCreatePipelineAsyncStatus_Success;
        case CreatePipelineAsyncStatus::CallbackCancelled:
            return WGPUCreatePipelineAsyncStatus_CallbackCancelled;
        case CreatePipelineAsyncStatus::ValidationError:
            return WGPUCreatePipelineAsyncStatus_ValidationError;
        case CreatePipelineAsyncStatus::InternalError:
            return WGPUCreatePipelineAsyncStatus_InternalError;
    }
    return WGPUCreatePipelineAsyncStatus_InternalError;
}

[[nodiscard]] inline CreatePipelineAsyncStatus FromWgpu(WGPUCreatePipelineAsyncStatus value) noexcept {
    switch (value) {
        case WGPUCreatePipelineAsyncStatus_Success:
            return CreatePipelineAsyncStatus::Success;
        case WGPUCreatePipelineAsyncStatus_CallbackCancelled:
            return CreatePipelineAsyncStatus::CallbackCancelled;
        case WGPUCreatePipelineAsyncStatus_ValidationError:
            return CreatePipelineAsyncStatus::ValidationError;
        case WGPUCreatePipelineAsyncStatus_InternalError:
            return CreatePipelineAsyncStatus::InternalError;
        default:
            return CreatePipelineAsyncStatus::InternalError;
    }
}

[[nodiscard]] inline WGPUCullMode ToWgpu(CullMode value) noexcept {
    switch (value) {
        case CullMode::Undefined:
            return WGPUCullMode_Undefined;
        case CullMode::None:
            return WGPUCullMode_None;
        case CullMode::Front:
            return WGPUCullMode_Front;
        case CullMode::Back:
            return WGPUCullMode_Back;
    }
    return WGPUCullMode_Undefined;
}

[[nodiscard]] inline CullMode FromWgpu(WGPUCullMode value) noexcept {
    switch (value) {
        case WGPUCullMode_Undefined:
            return CullMode::Undefined;
        case WGPUCullMode_None:
            return CullMode::None;
        case WGPUCullMode_Front:
            return CullMode::Front;
        case WGPUCullMode_Back:
            return CullMode::Back;
        default:
            return CullMode::Undefined;
    }
}

[[nodiscard]] inline WGPUDeviceLostReason ToWgpu(DeviceLostReason value) noexcept {
    switch (value) {
        case DeviceLostReason::Unknown:
            return WGPUDeviceLostReason_Unknown;
        case DeviceLostReason::Destroyed:
            return WGPUDeviceLostReason_Destroyed;
        case DeviceLostReason::CallbackCancelled:
            return WGPUDeviceLostReason_CallbackCancelled;
        case DeviceLostReason::FailedCreation:
            return WGPUDeviceLostReason_FailedCreation;
    }
    return WGPUDeviceLostReason_Unknown;
}

[[nodiscard]] inline DeviceLostReason FromWgpu(WGPUDeviceLostReason value) noexcept {
    switch (value) {
        case WGPUDeviceLostReason_Unknown:
            return DeviceLostReason::Unknown;
        case WGPUDeviceLostReason_Destroyed:
            return DeviceLostReason::Destroyed;
        case WGPUDeviceLostReason_CallbackCancelled:
            return DeviceLostReason::CallbackCancelled;
        case WGPUDeviceLostReason_FailedCreation:
            return DeviceLostReason::FailedCreation;
        default:
            return DeviceLostReason::Unknown;
    }
}

[[nodiscard]] inline WGPUErrorFilter ToWgpu(ErrorFilter value) noexcept {
    switch (value) {
        case ErrorFilter::Validation:
            return WGPUErrorFilter_Validation;
        case ErrorFilter::OutOfMemory:
            return WGPUErrorFilter_OutOfMemory;
        case ErrorFilter::Internal:
            return WGPUErrorFilter_Internal;
    }
    return WGPUErrorFilter_Validation;
}

[[nodiscard]] inline ErrorFilter FromWgpu(WGPUErrorFilter value) noexcept {
    switch (value) {
        case WGPUErrorFilter_Validation:
            return ErrorFilter::Validation;
        case WGPUErrorFilter_OutOfMemory:
            return ErrorFilter::OutOfMemory;
        case WGPUErrorFilter_Internal:
            return ErrorFilter::Internal;
        default:
            return ErrorFilter::Validation;
    }
}

[[nodiscard]] inline WGPUErrorType ToWgpu(ErrorType value) noexcept {
    switch (value) {
        case ErrorType::NoError:
            return WGPUErrorType_NoError;
        case ErrorType::Validation:
            return WGPUErrorType_Validation;
        case ErrorType::OutOfMemory:
            return WGPUErrorType_OutOfMemory;
        case ErrorType::Internal:
            return WGPUErrorType_Internal;
        case ErrorType::Unknown:
            return WGPUErrorType_Unknown;
    }
    return WGPUErrorType_Unknown;
}

[[nodiscard]] inline ErrorType FromWgpu(WGPUErrorType value) noexcept {
    switch (value) {
        case WGPUErrorType_NoError:
            return ErrorType::NoError;
        case WGPUErrorType_Validation:
            return ErrorType::Validation;
        case WGPUErrorType_OutOfMemory:
            return ErrorType::OutOfMemory;
        case WGPUErrorType_Internal:
            return ErrorType::Internal;
        case WGPUErrorType_Unknown:
            return ErrorType::Unknown;
        default:
            return ErrorType::Unknown;
    }
}

[[nodiscard]] inline WGPUExternalTextureRotation ToWgpu(ExternalTextureRotation value) noexcept {
    switch (value) {
        case ExternalTextureRotation::Rotate0Degrees:
            return WGPUExternalTextureRotation_Rotate0Degrees;
        case ExternalTextureRotation::Rotate90Degrees:
            return WGPUExternalTextureRotation_Rotate90Degrees;
        case ExternalTextureRotation::Rotate180Degrees:
            return WGPUExternalTextureRotation_Rotate180Degrees;
        case ExternalTextureRotation::Rotate270Degrees:
            return WGPUExternalTextureRotation_Rotate270Degrees;
    }
    return WGPUExternalTextureRotation_Rotate0Degrees;
}

[[nodiscard]] inline ExternalTextureRotation FromWgpu(WGPUExternalTextureRotation value) noexcept {
    switch (value) {
        case WGPUExternalTextureRotation_Rotate0Degrees:
            return ExternalTextureRotation::Rotate0Degrees;
        case WGPUExternalTextureRotation_Rotate90Degrees:
            return ExternalTextureRotation::Rotate90Degrees;
        case WGPUExternalTextureRotation_Rotate180Degrees:
            return ExternalTextureRotation::Rotate180Degrees;
        case WGPUExternalTextureRotation_Rotate270Degrees:
            return ExternalTextureRotation::Rotate270Degrees;
        default:
            return ExternalTextureRotation::Rotate0Degrees;
    }
}

[[nodiscard]] inline WGPUFeatureLevel ToWgpu(FeatureLevel value) noexcept {
    switch (value) {
        case FeatureLevel::Undefined:
            return WGPUFeatureLevel_Undefined;
        case FeatureLevel::Compatibility:
            return WGPUFeatureLevel_Compatibility;
        case FeatureLevel::Core:
            return WGPUFeatureLevel_Core;
    }
    return WGPUFeatureLevel_Undefined;
}

[[nodiscard]] inline FeatureLevel FromWgpu(WGPUFeatureLevel value) noexcept {
    switch (value) {
        case WGPUFeatureLevel_Undefined:
            return FeatureLevel::Undefined;
        case WGPUFeatureLevel_Compatibility:
            return FeatureLevel::Compatibility;
        case WGPUFeatureLevel_Core:
            return FeatureLevel::Core;
        default:
            return FeatureLevel::Undefined;
    }
}

[[nodiscard]] inline WGPUFeatureName ToWgpu(FeatureName value) noexcept {
    switch (value) {
        case FeatureName::CoreFeaturesAndLimits:
            return WGPUFeatureName_CoreFeaturesAndLimits;
        case FeatureName::DepthClipControl:
            return WGPUFeatureName_DepthClipControl;
        case FeatureName::Depth32FloatStencil8:
            return WGPUFeatureName_Depth32FloatStencil8;
        case FeatureName::TextureCompressionBC:
            return WGPUFeatureName_TextureCompressionBC;
        case FeatureName::TextureCompressionBCSliced3D:
            return WGPUFeatureName_TextureCompressionBCSliced3D;
        case FeatureName::TextureCompressionETC2:
            return WGPUFeatureName_TextureCompressionETC2;
        case FeatureName::TextureCompressionASTC:
            return WGPUFeatureName_TextureCompressionASTC;
        case FeatureName::TextureCompressionASTCSliced3D:
            return WGPUFeatureName_TextureCompressionASTCSliced3D;
        case FeatureName::TimestampQuery:
            return WGPUFeatureName_TimestampQuery;
        case FeatureName::IndirectFirstInstance:
            return WGPUFeatureName_IndirectFirstInstance;
        case FeatureName::ShaderF16:
            return WGPUFeatureName_ShaderF16;
        case FeatureName::RG11B10UfloatRenderable:
            return WGPUFeatureName_RG11B10UfloatRenderable;
        case FeatureName::BGRA8UnormStorage:
            return WGPUFeatureName_BGRA8UnormStorage;
        case FeatureName::Float32Filterable:
            return WGPUFeatureName_Float32Filterable;
        case FeatureName::Float32Blendable:
            return WGPUFeatureName_Float32Blendable;
        case FeatureName::ClipDistances:
            return WGPUFeatureName_ClipDistances;
        case FeatureName::DualSourceBlending:
            return WGPUFeatureName_DualSourceBlending;
        case FeatureName::Subgroups:
            return WGPUFeatureName_Subgroups;
        case FeatureName::TextureFormatsTier1:
            return WGPUFeatureName_TextureFormatsTier1;
        case FeatureName::TextureFormatsTier2:
            return WGPUFeatureName_TextureFormatsTier2;
        case FeatureName::PrimitiveIndex:
            return WGPUFeatureName_PrimitiveIndex;
        case FeatureName::TextureComponentSwizzle:
            return WGPUFeatureName_TextureComponentSwizzle;
        case FeatureName::DawnInternalUsages:
            return WGPUFeatureName_DawnInternalUsages;
        case FeatureName::DawnMultiPlanarFormats:
            return WGPUFeatureName_DawnMultiPlanarFormats;
        case FeatureName::DawnNative:
            return WGPUFeatureName_DawnNative;
        case FeatureName::ChromiumExperimentalTimestampQueryInsidePasses:
            return WGPUFeatureName_ChromiumExperimentalTimestampQueryInsidePasses;
        case FeatureName::ImplicitDeviceSynchronization:
            return WGPUFeatureName_ImplicitDeviceSynchronization;
        case FeatureName::TransientAttachments:
            return WGPUFeatureName_TransientAttachments;
        case FeatureName::MSAARenderToSingleSampled:
            return WGPUFeatureName_MSAARenderToSingleSampled;
        case FeatureName::D3D11MultithreadProtected:
            return WGPUFeatureName_D3D11MultithreadProtected;
        case FeatureName::ANGLETextureSharing:
            return WGPUFeatureName_ANGLETextureSharing;
        case FeatureName::PixelLocalStorageCoherent:
            return WGPUFeatureName_PixelLocalStorageCoherent;
        case FeatureName::PixelLocalStorageNonCoherent:
            return WGPUFeatureName_PixelLocalStorageNonCoherent;
        case FeatureName::Unorm16TextureFormats:
            return WGPUFeatureName_Unorm16TextureFormats;
        case FeatureName::MultiPlanarFormatExtendedUsages:
            return WGPUFeatureName_MultiPlanarFormatExtendedUsages;
        case FeatureName::MultiPlanarFormatP010:
            return WGPUFeatureName_MultiPlanarFormatP010;
        case FeatureName::HostMappedPointer:
            return WGPUFeatureName_HostMappedPointer;
        case FeatureName::MultiPlanarRenderTargets:
            return WGPUFeatureName_MultiPlanarRenderTargets;
        case FeatureName::MultiPlanarFormatNv12a:
            return WGPUFeatureName_MultiPlanarFormatNv12a;
        case FeatureName::FramebufferFetch:
            return WGPUFeatureName_FramebufferFetch;
        case FeatureName::BufferMapExtendedUsages:
            return WGPUFeatureName_BufferMapExtendedUsages;
        case FeatureName::AdapterPropertiesMemoryHeaps:
            return WGPUFeatureName_AdapterPropertiesMemoryHeaps;
        case FeatureName::AdapterPropertiesD3D:
            return WGPUFeatureName_AdapterPropertiesD3D;
        case FeatureName::AdapterPropertiesVk:
            return WGPUFeatureName_AdapterPropertiesVk;
        case FeatureName::DawnFormatCapabilities:
            return WGPUFeatureName_DawnFormatCapabilities;
        case FeatureName::DawnDrmFormatCapabilities:
            return WGPUFeatureName_DawnDrmFormatCapabilities;
        case FeatureName::MultiPlanarFormatNv16:
            return WGPUFeatureName_MultiPlanarFormatNv16;
        case FeatureName::MultiPlanarFormatNv24:
            return WGPUFeatureName_MultiPlanarFormatNv24;
        case FeatureName::MultiPlanarFormatP210:
            return WGPUFeatureName_MultiPlanarFormatP210;
        case FeatureName::MultiPlanarFormatP410:
            return WGPUFeatureName_MultiPlanarFormatP410;
        case FeatureName::SharedTextureMemoryVkDedicatedAllocation:
            return WGPUFeatureName_SharedTextureMemoryVkDedicatedAllocation;
        case FeatureName::SharedTextureMemoryAHardwareBuffer:
            return WGPUFeatureName_SharedTextureMemoryAHardwareBuffer;
        case FeatureName::SharedTextureMemoryDmaBuf:
            return WGPUFeatureName_SharedTextureMemoryDmaBuf;
        case FeatureName::SharedTextureMemoryOpaqueFD:
            return WGPUFeatureName_SharedTextureMemoryOpaqueFD;
        case FeatureName::SharedTextureMemoryZirconHandle:
            return WGPUFeatureName_SharedTextureMemoryZirconHandle;
        case FeatureName::SharedTextureMemoryDXGISharedHandle:
            return WGPUFeatureName_SharedTextureMemoryDXGISharedHandle;
        case FeatureName::SharedTextureMemoryD3D11Texture2D:
            return WGPUFeatureName_SharedTextureMemoryD3D11Texture2D;
        case FeatureName::SharedTextureMemoryIOSurface:
            return WGPUFeatureName_SharedTextureMemoryIOSurface;
        case FeatureName::SharedTextureMemoryEGLImage:
            return WGPUFeatureName_SharedTextureMemoryEGLImage;
        case FeatureName::SharedFenceVkSemaphoreOpaqueFD:
            return WGPUFeatureName_SharedFenceVkSemaphoreOpaqueFD;
        case FeatureName::SharedFenceSyncFD:
            return WGPUFeatureName_SharedFenceSyncFD;
        case FeatureName::SharedFenceVkSemaphoreZirconHandle:
            return WGPUFeatureName_SharedFenceVkSemaphoreZirconHandle;
        case FeatureName::SharedFenceDXGISharedHandle:
            return WGPUFeatureName_SharedFenceDXGISharedHandle;
        case FeatureName::SharedFenceMTLSharedEvent:
            return WGPUFeatureName_SharedFenceMTLSharedEvent;
        case FeatureName::SharedBufferMemoryD3D12Resource:
            return WGPUFeatureName_SharedBufferMemoryD3D12Resource;
        case FeatureName::StaticSamplers:
            return WGPUFeatureName_StaticSamplers;
        case FeatureName::YCbCrVulkanSamplers:
            return WGPUFeatureName_YCbCrVulkanSamplers;
        case FeatureName::ShaderModuleCompilationOptions:
            return WGPUFeatureName_ShaderModuleCompilationOptions;
        case FeatureName::DawnLoadResolveTexture:
            return WGPUFeatureName_DawnLoadResolveTexture;
        case FeatureName::DawnPartialLoadResolveTexture:
            return WGPUFeatureName_DawnPartialLoadResolveTexture;
        case FeatureName::MultiDrawIndirect:
            return WGPUFeatureName_MultiDrawIndirect;
        case FeatureName::DawnTexelCopyBufferRowAlignment:
            return WGPUFeatureName_DawnTexelCopyBufferRowAlignment;
        case FeatureName::FlexibleTextureViews:
            return WGPUFeatureName_FlexibleTextureViews;
        case FeatureName::ChromiumExperimentalSubgroupMatrix:
            return WGPUFeatureName_ChromiumExperimentalSubgroupMatrix;
        case FeatureName::SharedFenceEGLSync:
            return WGPUFeatureName_SharedFenceEGLSync;
        case FeatureName::DawnDeviceAllocatorControl:
            return WGPUFeatureName_DawnDeviceAllocatorControl;
        case FeatureName::AdapterPropertiesWGPU:
            return WGPUFeatureName_AdapterPropertiesWGPU;
        case FeatureName::SharedBufferMemoryD3D12SharedMemoryFileMappingHandle:
#ifdef WOKI_WGPU_HAS_D3D12_FILE_MAPPING_FEATURE
            return WGPUFeatureName_SharedBufferMemoryD3D12SharedMemoryFileMappingHandle;
#else
            return WGPUFeatureName_Force32;
#endif
        case FeatureName::SharedTextureMemoryD3D12Resource:
            return WGPUFeatureName_SharedTextureMemoryD3D12Resource;
        case FeatureName::ChromiumExperimentalSamplingResourceTable:
            return WGPUFeatureName_ChromiumExperimentalSamplingResourceTable;
        case FeatureName::SubgroupSizeControl:
            return WGPUFeatureName_SubgroupSizeControl;
        case FeatureName::AtomicVec2uMinMax:
            return WGPUFeatureName_AtomicVec2uMinMax;
        case FeatureName::Unorm16FormatsForExternalTexture:
            return WGPUFeatureName_Unorm16FormatsForExternalTexture;
        case FeatureName::OpaqueYCbCrAndroidForExternalTexture:
            return WGPUFeatureName_OpaqueYCbCrAndroidForExternalTexture;
        case FeatureName::Unorm16Filterable:
            return WGPUFeatureName_Unorm16Filterable;
        case FeatureName::RenderPassRenderArea:
            return WGPUFeatureName_RenderPassRenderArea;
        case FeatureName::AdapterPropertiesDrm:
            return WGPUFeatureName_AdapterPropertiesDrm;
    }
    return WGPUFeatureName_CoreFeaturesAndLimits;
}

[[nodiscard]] inline FeatureName FromWgpu(WGPUFeatureName value) noexcept {
    switch (value) {
        case WGPUFeatureName_CoreFeaturesAndLimits:
            return FeatureName::CoreFeaturesAndLimits;
        case WGPUFeatureName_DepthClipControl:
            return FeatureName::DepthClipControl;
        case WGPUFeatureName_Depth32FloatStencil8:
            return FeatureName::Depth32FloatStencil8;
        case WGPUFeatureName_TextureCompressionBC:
            return FeatureName::TextureCompressionBC;
        case WGPUFeatureName_TextureCompressionBCSliced3D:
            return FeatureName::TextureCompressionBCSliced3D;
        case WGPUFeatureName_TextureCompressionETC2:
            return FeatureName::TextureCompressionETC2;
        case WGPUFeatureName_TextureCompressionASTC:
            return FeatureName::TextureCompressionASTC;
        case WGPUFeatureName_TextureCompressionASTCSliced3D:
            return FeatureName::TextureCompressionASTCSliced3D;
        case WGPUFeatureName_TimestampQuery:
            return FeatureName::TimestampQuery;
        case WGPUFeatureName_IndirectFirstInstance:
            return FeatureName::IndirectFirstInstance;
        case WGPUFeatureName_ShaderF16:
            return FeatureName::ShaderF16;
        case WGPUFeatureName_RG11B10UfloatRenderable:
            return FeatureName::RG11B10UfloatRenderable;
        case WGPUFeatureName_BGRA8UnormStorage:
            return FeatureName::BGRA8UnormStorage;
        case WGPUFeatureName_Float32Filterable:
            return FeatureName::Float32Filterable;
        case WGPUFeatureName_Float32Blendable:
            return FeatureName::Float32Blendable;
        case WGPUFeatureName_ClipDistances:
            return FeatureName::ClipDistances;
        case WGPUFeatureName_DualSourceBlending:
            return FeatureName::DualSourceBlending;
        case WGPUFeatureName_Subgroups:
            return FeatureName::Subgroups;
        case WGPUFeatureName_TextureFormatsTier1:
            return FeatureName::TextureFormatsTier1;
        case WGPUFeatureName_TextureFormatsTier2:
            return FeatureName::TextureFormatsTier2;
        case WGPUFeatureName_PrimitiveIndex:
            return FeatureName::PrimitiveIndex;
        case WGPUFeatureName_TextureComponentSwizzle:
            return FeatureName::TextureComponentSwizzle;
        case WGPUFeatureName_DawnInternalUsages:
            return FeatureName::DawnInternalUsages;
        case WGPUFeatureName_DawnMultiPlanarFormats:
            return FeatureName::DawnMultiPlanarFormats;
        case WGPUFeatureName_DawnNative:
            return FeatureName::DawnNative;
        case WGPUFeatureName_ChromiumExperimentalTimestampQueryInsidePasses:
            return FeatureName::ChromiumExperimentalTimestampQueryInsidePasses;
        case WGPUFeatureName_ImplicitDeviceSynchronization:
            return FeatureName::ImplicitDeviceSynchronization;
        case WGPUFeatureName_TransientAttachments:
            return FeatureName::TransientAttachments;
        case WGPUFeatureName_MSAARenderToSingleSampled:
            return FeatureName::MSAARenderToSingleSampled;
        case WGPUFeatureName_D3D11MultithreadProtected:
            return FeatureName::D3D11MultithreadProtected;
        case WGPUFeatureName_ANGLETextureSharing:
            return FeatureName::ANGLETextureSharing;
        case WGPUFeatureName_PixelLocalStorageCoherent:
            return FeatureName::PixelLocalStorageCoherent;
        case WGPUFeatureName_PixelLocalStorageNonCoherent:
            return FeatureName::PixelLocalStorageNonCoherent;
        case WGPUFeatureName_Unorm16TextureFormats:
            return FeatureName::Unorm16TextureFormats;
        case WGPUFeatureName_MultiPlanarFormatExtendedUsages:
            return FeatureName::MultiPlanarFormatExtendedUsages;
        case WGPUFeatureName_MultiPlanarFormatP010:
            return FeatureName::MultiPlanarFormatP010;
        case WGPUFeatureName_HostMappedPointer:
            return FeatureName::HostMappedPointer;
        case WGPUFeatureName_MultiPlanarRenderTargets:
            return FeatureName::MultiPlanarRenderTargets;
        case WGPUFeatureName_MultiPlanarFormatNv12a:
            return FeatureName::MultiPlanarFormatNv12a;
        case WGPUFeatureName_FramebufferFetch:
            return FeatureName::FramebufferFetch;
        case WGPUFeatureName_BufferMapExtendedUsages:
            return FeatureName::BufferMapExtendedUsages;
        case WGPUFeatureName_AdapterPropertiesMemoryHeaps:
            return FeatureName::AdapterPropertiesMemoryHeaps;
        case WGPUFeatureName_AdapterPropertiesD3D:
            return FeatureName::AdapterPropertiesD3D;
        case WGPUFeatureName_AdapterPropertiesVk:
            return FeatureName::AdapterPropertiesVk;
        case WGPUFeatureName_DawnFormatCapabilities:
            return FeatureName::DawnFormatCapabilities;
        case WGPUFeatureName_DawnDrmFormatCapabilities:
            return FeatureName::DawnDrmFormatCapabilities;
        case WGPUFeatureName_MultiPlanarFormatNv16:
            return FeatureName::MultiPlanarFormatNv16;
        case WGPUFeatureName_MultiPlanarFormatNv24:
            return FeatureName::MultiPlanarFormatNv24;
        case WGPUFeatureName_MultiPlanarFormatP210:
            return FeatureName::MultiPlanarFormatP210;
        case WGPUFeatureName_MultiPlanarFormatP410:
            return FeatureName::MultiPlanarFormatP410;
        case WGPUFeatureName_SharedTextureMemoryVkDedicatedAllocation:
            return FeatureName::SharedTextureMemoryVkDedicatedAllocation;
        case WGPUFeatureName_SharedTextureMemoryAHardwareBuffer:
            return FeatureName::SharedTextureMemoryAHardwareBuffer;
        case WGPUFeatureName_SharedTextureMemoryDmaBuf:
            return FeatureName::SharedTextureMemoryDmaBuf;
        case WGPUFeatureName_SharedTextureMemoryOpaqueFD:
            return FeatureName::SharedTextureMemoryOpaqueFD;
        case WGPUFeatureName_SharedTextureMemoryZirconHandle:
            return FeatureName::SharedTextureMemoryZirconHandle;
        case WGPUFeatureName_SharedTextureMemoryDXGISharedHandle:
            return FeatureName::SharedTextureMemoryDXGISharedHandle;
        case WGPUFeatureName_SharedTextureMemoryD3D11Texture2D:
            return FeatureName::SharedTextureMemoryD3D11Texture2D;
        case WGPUFeatureName_SharedTextureMemoryIOSurface:
            return FeatureName::SharedTextureMemoryIOSurface;
        case WGPUFeatureName_SharedTextureMemoryEGLImage:
            return FeatureName::SharedTextureMemoryEGLImage;
        case WGPUFeatureName_SharedFenceVkSemaphoreOpaqueFD:
            return FeatureName::SharedFenceVkSemaphoreOpaqueFD;
        case WGPUFeatureName_SharedFenceSyncFD:
            return FeatureName::SharedFenceSyncFD;
        case WGPUFeatureName_SharedFenceVkSemaphoreZirconHandle:
            return FeatureName::SharedFenceVkSemaphoreZirconHandle;
        case WGPUFeatureName_SharedFenceDXGISharedHandle:
            return FeatureName::SharedFenceDXGISharedHandle;
        case WGPUFeatureName_SharedFenceMTLSharedEvent:
            return FeatureName::SharedFenceMTLSharedEvent;
        case WGPUFeatureName_SharedBufferMemoryD3D12Resource:
            return FeatureName::SharedBufferMemoryD3D12Resource;
        case WGPUFeatureName_StaticSamplers:
            return FeatureName::StaticSamplers;
        case WGPUFeatureName_YCbCrVulkanSamplers:
            return FeatureName::YCbCrVulkanSamplers;
        case WGPUFeatureName_ShaderModuleCompilationOptions:
            return FeatureName::ShaderModuleCompilationOptions;
        case WGPUFeatureName_DawnLoadResolveTexture:
            return FeatureName::DawnLoadResolveTexture;
        case WGPUFeatureName_DawnPartialLoadResolveTexture:
            return FeatureName::DawnPartialLoadResolveTexture;
        case WGPUFeatureName_MultiDrawIndirect:
            return FeatureName::MultiDrawIndirect;
        case WGPUFeatureName_DawnTexelCopyBufferRowAlignment:
            return FeatureName::DawnTexelCopyBufferRowAlignment;
        case WGPUFeatureName_FlexibleTextureViews:
            return FeatureName::FlexibleTextureViews;
        case WGPUFeatureName_ChromiumExperimentalSubgroupMatrix:
            return FeatureName::ChromiumExperimentalSubgroupMatrix;
        case WGPUFeatureName_SharedFenceEGLSync:
            return FeatureName::SharedFenceEGLSync;
        case WGPUFeatureName_DawnDeviceAllocatorControl:
            return FeatureName::DawnDeviceAllocatorControl;
        case WGPUFeatureName_AdapterPropertiesWGPU:
            return FeatureName::AdapterPropertiesWGPU;
#ifdef WOKI_WGPU_HAS_D3D12_FILE_MAPPING_FEATURE
        case WGPUFeatureName_SharedBufferMemoryD3D12SharedMemoryFileMappingHandle:
            return FeatureName::SharedBufferMemoryD3D12SharedMemoryFileMappingHandle;
#endif
        case WGPUFeatureName_SharedTextureMemoryD3D12Resource:
            return FeatureName::SharedTextureMemoryD3D12Resource;
        case WGPUFeatureName_ChromiumExperimentalSamplingResourceTable:
            return FeatureName::ChromiumExperimentalSamplingResourceTable;
        case WGPUFeatureName_SubgroupSizeControl:
            return FeatureName::SubgroupSizeControl;
        case WGPUFeatureName_AtomicVec2uMinMax:
            return FeatureName::AtomicVec2uMinMax;
        case WGPUFeatureName_Unorm16FormatsForExternalTexture:
            return FeatureName::Unorm16FormatsForExternalTexture;
        case WGPUFeatureName_OpaqueYCbCrAndroidForExternalTexture:
            return FeatureName::OpaqueYCbCrAndroidForExternalTexture;
        case WGPUFeatureName_Unorm16Filterable:
            return FeatureName::Unorm16Filterable;
        case WGPUFeatureName_RenderPassRenderArea:
            return FeatureName::RenderPassRenderArea;
        case WGPUFeatureName_AdapterPropertiesDrm:
            return FeatureName::AdapterPropertiesDrm;
        default:
            return FeatureName::CoreFeaturesAndLimits;
    }
}

[[nodiscard]] inline WGPUFilterMode ToWgpu(FilterMode value) noexcept {
    switch (value) {
        case FilterMode::Undefined:
            return WGPUFilterMode_Undefined;
        case FilterMode::Nearest:
            return WGPUFilterMode_Nearest;
        case FilterMode::Linear:
            return WGPUFilterMode_Linear;
    }
    return WGPUFilterMode_Undefined;
}

[[nodiscard]] inline FilterMode FromWgpu(WGPUFilterMode value) noexcept {
    switch (value) {
        case WGPUFilterMode_Undefined:
            return FilterMode::Undefined;
        case WGPUFilterMode_Nearest:
            return FilterMode::Nearest;
        case WGPUFilterMode_Linear:
            return FilterMode::Linear;
        default:
            return FilterMode::Undefined;
    }
}

[[nodiscard]] inline WGPUFrontFace ToWgpu(FrontFace value) noexcept {
    switch (value) {
        case FrontFace::Undefined:
            return WGPUFrontFace_Undefined;
        case FrontFace::CCW:
            return WGPUFrontFace_CCW;
        case FrontFace::CW:
            return WGPUFrontFace_CW;
    }
    return WGPUFrontFace_Undefined;
}

[[nodiscard]] inline FrontFace FromWgpu(WGPUFrontFace value) noexcept {
    switch (value) {
        case WGPUFrontFace_Undefined:
            return FrontFace::Undefined;
        case WGPUFrontFace_CCW:
            return FrontFace::CCW;
        case WGPUFrontFace_CW:
            return FrontFace::CW;
        default:
            return FrontFace::Undefined;
    }
}

[[nodiscard]] inline WGPUIndexFormat ToWgpu(IndexFormat value) noexcept {
    switch (value) {
        case IndexFormat::Undefined:
            return WGPUIndexFormat_Undefined;
        case IndexFormat::Uint16:
            return WGPUIndexFormat_Uint16;
        case IndexFormat::Uint32:
            return WGPUIndexFormat_Uint32;
    }
    return WGPUIndexFormat_Undefined;
}

[[nodiscard]] inline IndexFormat FromWgpu(WGPUIndexFormat value) noexcept {
    switch (value) {
        case WGPUIndexFormat_Undefined:
            return IndexFormat::Undefined;
        case WGPUIndexFormat_Uint16:
            return IndexFormat::Uint16;
        case WGPUIndexFormat_Uint32:
            return IndexFormat::Uint32;
        default:
            return IndexFormat::Undefined;
    }
}

[[nodiscard]] inline WGPUInstanceFeatureName ToWgpu(InstanceFeatureName value) noexcept {
    switch (value) {
        case InstanceFeatureName::TimedWaitAny:
            return WGPUInstanceFeatureName_TimedWaitAny;
        case InstanceFeatureName::ShaderSourceSPIRV:
            return WGPUInstanceFeatureName_ShaderSourceSPIRV;
        case InstanceFeatureName::MultipleDevicesPerAdapter:
            return WGPUInstanceFeatureName_MultipleDevicesPerAdapter;
    }
    return WGPUInstanceFeatureName_TimedWaitAny;
}

[[nodiscard]] inline InstanceFeatureName FromWgpu(WGPUInstanceFeatureName value) noexcept {
    switch (value) {
        case WGPUInstanceFeatureName_TimedWaitAny:
            return InstanceFeatureName::TimedWaitAny;
        case WGPUInstanceFeatureName_ShaderSourceSPIRV:
            return InstanceFeatureName::ShaderSourceSPIRV;
        case WGPUInstanceFeatureName_MultipleDevicesPerAdapter:
            return InstanceFeatureName::MultipleDevicesPerAdapter;
        default:
            return InstanceFeatureName::TimedWaitAny;
    }
}

[[nodiscard]] inline WGPULoadOp ToWgpu(LoadOp value) noexcept {
    switch (value) {
        case LoadOp::Undefined:
            return WGPULoadOp_Undefined;
        case LoadOp::Load:
            return WGPULoadOp_Load;
        case LoadOp::Clear:
            return WGPULoadOp_Clear;
        case LoadOp::ExpandResolveTexture:
            return WGPULoadOp_ExpandResolveTexture;
    }
    return WGPULoadOp_Undefined;
}

[[nodiscard]] inline LoadOp FromWgpu(WGPULoadOp value) noexcept {
    switch (value) {
        case WGPULoadOp_Undefined:
            return LoadOp::Undefined;
        case WGPULoadOp_Load:
            return LoadOp::Load;
        case WGPULoadOp_Clear:
            return LoadOp::Clear;
        case WGPULoadOp_ExpandResolveTexture:
            return LoadOp::ExpandResolveTexture;
        default:
            return LoadOp::Undefined;
    }
}

[[nodiscard]] inline WGPULoggingType ToWgpu(LoggingType value) noexcept {
    switch (value) {
        case LoggingType::Verbose:
            return WGPULoggingType_Verbose;
        case LoggingType::Info:
            return WGPULoggingType_Info;
        case LoggingType::Warning:
            return WGPULoggingType_Warning;
        case LoggingType::Error:
            return WGPULoggingType_Error;
    }
    return WGPULoggingType_Verbose;
}

[[nodiscard]] inline LoggingType FromWgpu(WGPULoggingType value) noexcept {
    switch (value) {
        case WGPULoggingType_Verbose:
            return LoggingType::Verbose;
        case WGPULoggingType_Info:
            return LoggingType::Info;
        case WGPULoggingType_Warning:
            return LoggingType::Warning;
        case WGPULoggingType_Error:
            return LoggingType::Error;
        default:
            return LoggingType::Verbose;
    }
}

[[nodiscard]] inline WGPUMapAsyncStatus ToWgpu(MapAsyncStatus value) noexcept {
    switch (value) {
        case MapAsyncStatus::Success:
            return WGPUMapAsyncStatus_Success;
        case MapAsyncStatus::CallbackCancelled:
            return WGPUMapAsyncStatus_CallbackCancelled;
        case MapAsyncStatus::Error:
            return WGPUMapAsyncStatus_Error;
        case MapAsyncStatus::Aborted:
            return WGPUMapAsyncStatus_Aborted;
    }
    return WGPUMapAsyncStatus_Error;
}

[[nodiscard]] inline MapAsyncStatus FromWgpu(WGPUMapAsyncStatus value) noexcept {
    switch (value) {
        case WGPUMapAsyncStatus_Success:
            return MapAsyncStatus::Success;
        case WGPUMapAsyncStatus_CallbackCancelled:
            return MapAsyncStatus::CallbackCancelled;
        case WGPUMapAsyncStatus_Error:
            return MapAsyncStatus::Error;
        case WGPUMapAsyncStatus_Aborted:
            return MapAsyncStatus::Aborted;
        default:
            return MapAsyncStatus::Error;
    }
}

[[nodiscard]] inline WGPUMipmapFilterMode ToWgpu(MipmapFilterMode value) noexcept {
    switch (value) {
        case MipmapFilterMode::Undefined:
            return WGPUMipmapFilterMode_Undefined;
        case MipmapFilterMode::Nearest:
            return WGPUMipmapFilterMode_Nearest;
        case MipmapFilterMode::Linear:
            return WGPUMipmapFilterMode_Linear;
    }
    return WGPUMipmapFilterMode_Undefined;
}

[[nodiscard]] inline MipmapFilterMode FromWgpu(WGPUMipmapFilterMode value) noexcept {
    switch (value) {
        case WGPUMipmapFilterMode_Undefined:
            return MipmapFilterMode::Undefined;
        case WGPUMipmapFilterMode_Nearest:
            return MipmapFilterMode::Nearest;
        case WGPUMipmapFilterMode_Linear:
            return MipmapFilterMode::Linear;
        default:
            return MipmapFilterMode::Undefined;
    }
}

[[nodiscard]] inline WGPUPopErrorScopeStatus ToWgpu(PopErrorScopeStatus value) noexcept {
    switch (value) {
        case PopErrorScopeStatus::Success:
            return WGPUPopErrorScopeStatus_Success;
        case PopErrorScopeStatus::CallbackCancelled:
            return WGPUPopErrorScopeStatus_CallbackCancelled;
        case PopErrorScopeStatus::Error:
            return WGPUPopErrorScopeStatus_Error;
    }
    return WGPUPopErrorScopeStatus_Error;
}

[[nodiscard]] inline PopErrorScopeStatus FromWgpu(WGPUPopErrorScopeStatus value) noexcept {
    switch (value) {
        case WGPUPopErrorScopeStatus_Success:
            return PopErrorScopeStatus::Success;
        case WGPUPopErrorScopeStatus_CallbackCancelled:
            return PopErrorScopeStatus::CallbackCancelled;
        case WGPUPopErrorScopeStatus_Error:
            return PopErrorScopeStatus::Error;
        default:
            return PopErrorScopeStatus::Error;
    }
}

[[nodiscard]] inline WGPUPowerPreference ToWgpu(PowerPreference value) noexcept {
    switch (value) {
        case PowerPreference::Undefined:
            return WGPUPowerPreference_Undefined;
        case PowerPreference::LowPower:
            return WGPUPowerPreference_LowPower;
        case PowerPreference::HighPerformance:
            return WGPUPowerPreference_HighPerformance;
    }
    return WGPUPowerPreference_Undefined;
}

[[nodiscard]] inline PowerPreference FromWgpu(WGPUPowerPreference value) noexcept {
    switch (value) {
        case WGPUPowerPreference_Undefined:
            return PowerPreference::Undefined;
        case WGPUPowerPreference_LowPower:
            return PowerPreference::LowPower;
        case WGPUPowerPreference_HighPerformance:
            return PowerPreference::HighPerformance;
        default:
            return PowerPreference::Undefined;
    }
}

[[nodiscard]] inline WGPUPredefinedColorSpace ToWgpu(PredefinedColorSpace value) noexcept {
    switch (value) {
        case PredefinedColorSpace::SRGB:
            return WGPUPredefinedColorSpace_SRGB;
        case PredefinedColorSpace::DisplayP3:
            return WGPUPredefinedColorSpace_DisplayP3;
        case PredefinedColorSpace::SRGBLinear:
            return WGPUPredefinedColorSpace_SRGBLinear;
        case PredefinedColorSpace::DisplayP3Linear:
            return WGPUPredefinedColorSpace_DisplayP3Linear;
        case PredefinedColorSpace::Rec2020Linear:
            return WGPUPredefinedColorSpace_Rec2020Linear;
    }
    return WGPUPredefinedColorSpace_SRGB;
}

[[nodiscard]] inline PredefinedColorSpace FromWgpu(WGPUPredefinedColorSpace value) noexcept {
    switch (value) {
        case WGPUPredefinedColorSpace_SRGB:
            return PredefinedColorSpace::SRGB;
        case WGPUPredefinedColorSpace_DisplayP3:
            return PredefinedColorSpace::DisplayP3;
        case WGPUPredefinedColorSpace_SRGBLinear:
            return PredefinedColorSpace::SRGBLinear;
        case WGPUPredefinedColorSpace_DisplayP3Linear:
            return PredefinedColorSpace::DisplayP3Linear;
        case WGPUPredefinedColorSpace_Rec2020Linear:
            return PredefinedColorSpace::Rec2020Linear;
        default:
            return PredefinedColorSpace::SRGB;
    }
}

[[nodiscard]] inline WGPUPresentMode ToWgpu(PresentMode value) noexcept {
    switch (value) {
        case PresentMode::Undefined:
            return WGPUPresentMode_Undefined;
        case PresentMode::Fifo:
            return WGPUPresentMode_Fifo;
        case PresentMode::FifoRelaxed:
            return WGPUPresentMode_FifoRelaxed;
        case PresentMode::Immediate:
            return WGPUPresentMode_Immediate;
        case PresentMode::Mailbox:
            return WGPUPresentMode_Mailbox;
    }
    return WGPUPresentMode_Undefined;
}

[[nodiscard]] inline PresentMode FromWgpu(WGPUPresentMode value) noexcept {
    switch (value) {
        case WGPUPresentMode_Undefined:
            return PresentMode::Undefined;
        case WGPUPresentMode_Fifo:
            return PresentMode::Fifo;
        case WGPUPresentMode_FifoRelaxed:
            return PresentMode::FifoRelaxed;
        case WGPUPresentMode_Immediate:
            return PresentMode::Immediate;
        case WGPUPresentMode_Mailbox:
            return PresentMode::Mailbox;
        default:
            return PresentMode::Undefined;
    }
}

[[nodiscard]] inline WGPUPrimitiveTopology ToWgpu(PrimitiveTopology value) noexcept {
    switch (value) {
        case PrimitiveTopology::Undefined:
            return WGPUPrimitiveTopology_Undefined;
        case PrimitiveTopology::PointList:
            return WGPUPrimitiveTopology_PointList;
        case PrimitiveTopology::LineList:
            return WGPUPrimitiveTopology_LineList;
        case PrimitiveTopology::LineStrip:
            return WGPUPrimitiveTopology_LineStrip;
        case PrimitiveTopology::TriangleList:
            return WGPUPrimitiveTopology_TriangleList;
        case PrimitiveTopology::TriangleStrip:
            return WGPUPrimitiveTopology_TriangleStrip;
    }
    return WGPUPrimitiveTopology_Undefined;
}

[[nodiscard]] inline PrimitiveTopology FromWgpu(WGPUPrimitiveTopology value) noexcept {
    switch (value) {
        case WGPUPrimitiveTopology_Undefined:
            return PrimitiveTopology::Undefined;
        case WGPUPrimitiveTopology_PointList:
            return PrimitiveTopology::PointList;
        case WGPUPrimitiveTopology_LineList:
            return PrimitiveTopology::LineList;
        case WGPUPrimitiveTopology_LineStrip:
            return PrimitiveTopology::LineStrip;
        case WGPUPrimitiveTopology_TriangleList:
            return PrimitiveTopology::TriangleList;
        case WGPUPrimitiveTopology_TriangleStrip:
            return PrimitiveTopology::TriangleStrip;
        default:
            return PrimitiveTopology::Undefined;
    }
}

[[nodiscard]] inline WGPUQueryType ToWgpu(QueryType value) noexcept {
    switch (value) {
        case QueryType::Occlusion:
            return WGPUQueryType_Occlusion;
        case QueryType::Timestamp:
            return WGPUQueryType_Timestamp;
    }
    return WGPUQueryType_Occlusion;
}

[[nodiscard]] inline QueryType FromWgpu(WGPUQueryType value) noexcept {
    switch (value) {
        case WGPUQueryType_Occlusion:
            return QueryType::Occlusion;
        case WGPUQueryType_Timestamp:
            return QueryType::Timestamp;
        default:
            return QueryType::Occlusion;
    }
}

[[nodiscard]] inline WGPUQueueWorkDoneStatus ToWgpu(QueueWorkDoneStatus value) noexcept {
    switch (value) {
        case QueueWorkDoneStatus::Success:
            return WGPUQueueWorkDoneStatus_Success;
        case QueueWorkDoneStatus::CallbackCancelled:
            return WGPUQueueWorkDoneStatus_CallbackCancelled;
        case QueueWorkDoneStatus::Error:
            return WGPUQueueWorkDoneStatus_Error;
    }
    return WGPUQueueWorkDoneStatus_Error;
}

[[nodiscard]] inline QueueWorkDoneStatus FromWgpu(WGPUQueueWorkDoneStatus value) noexcept {
    switch (value) {
        case WGPUQueueWorkDoneStatus_Success:
            return QueueWorkDoneStatus::Success;
        case WGPUQueueWorkDoneStatus_CallbackCancelled:
            return QueueWorkDoneStatus::CallbackCancelled;
        case WGPUQueueWorkDoneStatus_Error:
            return QueueWorkDoneStatus::Error;
        default:
            return QueueWorkDoneStatus::Error;
    }
}

[[nodiscard]] inline WGPURequestAdapterStatus ToWgpu(RequestAdapterStatus value) noexcept {
    switch (value) {
        case RequestAdapterStatus::Success:
            return WGPURequestAdapterStatus_Success;
        case RequestAdapterStatus::CallbackCancelled:
            return WGPURequestAdapterStatus_CallbackCancelled;
        case RequestAdapterStatus::Unavailable:
            return WGPURequestAdapterStatus_Unavailable;
        case RequestAdapterStatus::Error:
            return WGPURequestAdapterStatus_Error;
    }
    return WGPURequestAdapterStatus_Error;
}

[[nodiscard]] inline RequestAdapterStatus FromWgpu(WGPURequestAdapterStatus value) noexcept {
    switch (value) {
        case WGPURequestAdapterStatus_Success:
            return RequestAdapterStatus::Success;
        case WGPURequestAdapterStatus_CallbackCancelled:
            return RequestAdapterStatus::CallbackCancelled;
        case WGPURequestAdapterStatus_Unavailable:
            return RequestAdapterStatus::Unavailable;
        case WGPURequestAdapterStatus_Error:
            return RequestAdapterStatus::Error;
        default:
            return RequestAdapterStatus::Error;
    }
}

[[nodiscard]] inline WGPURequestDeviceStatus ToWgpu(RequestDeviceStatus value) noexcept {
    switch (value) {
        case RequestDeviceStatus::Success:
            return WGPURequestDeviceStatus_Success;
        case RequestDeviceStatus::CallbackCancelled:
            return WGPURequestDeviceStatus_CallbackCancelled;
        case RequestDeviceStatus::Error:
            return WGPURequestDeviceStatus_Error;
    }
    return WGPURequestDeviceStatus_Error;
}

[[nodiscard]] inline RequestDeviceStatus FromWgpu(WGPURequestDeviceStatus value) noexcept {
    switch (value) {
        case WGPURequestDeviceStatus_Success:
            return RequestDeviceStatus::Success;
        case WGPURequestDeviceStatus_CallbackCancelled:
            return RequestDeviceStatus::CallbackCancelled;
        case WGPURequestDeviceStatus_Error:
            return RequestDeviceStatus::Error;
        default:
            return RequestDeviceStatus::Error;
    }
}

[[nodiscard]] inline WGPUSamplerBindingType ToWgpu(SamplerBindingType value) noexcept {
    switch (value) {
        case SamplerBindingType::BindingNotUsed:
            return WGPUSamplerBindingType_BindingNotUsed;
        case SamplerBindingType::Undefined:
            return WGPUSamplerBindingType_Undefined;
        case SamplerBindingType::Filtering:
            return WGPUSamplerBindingType_Filtering;
        case SamplerBindingType::NonFiltering:
            return WGPUSamplerBindingType_NonFiltering;
        case SamplerBindingType::Comparison:
            return WGPUSamplerBindingType_Comparison;
    }
    return WGPUSamplerBindingType_Undefined;
}

[[nodiscard]] inline SamplerBindingType FromWgpu(WGPUSamplerBindingType value) noexcept {
    switch (value) {
        case WGPUSamplerBindingType_BindingNotUsed:
            return SamplerBindingType::BindingNotUsed;
        case WGPUSamplerBindingType_Undefined:
            return SamplerBindingType::Undefined;
        case WGPUSamplerBindingType_Filtering:
            return SamplerBindingType::Filtering;
        case WGPUSamplerBindingType_NonFiltering:
            return SamplerBindingType::NonFiltering;
        case WGPUSamplerBindingType_Comparison:
            return SamplerBindingType::Comparison;
        default:
            return SamplerBindingType::Undefined;
    }
}

[[nodiscard]] inline WGPUSharedFenceType ToWgpu(SharedFenceType value) noexcept {
    switch (value) {
        case SharedFenceType::VkSemaphoreOpaqueFD:
            return WGPUSharedFenceType_VkSemaphoreOpaqueFD;
        case SharedFenceType::SyncFD:
            return WGPUSharedFenceType_SyncFD;
        case SharedFenceType::VkSemaphoreZirconHandle:
            return WGPUSharedFenceType_VkSemaphoreZirconHandle;
        case SharedFenceType::DXGISharedHandle:
            return WGPUSharedFenceType_DXGISharedHandle;
        case SharedFenceType::MTLSharedEvent:
            return WGPUSharedFenceType_MTLSharedEvent;
        case SharedFenceType::EGLSync:
            return WGPUSharedFenceType_EGLSync;
    }
    return WGPUSharedFenceType_VkSemaphoreOpaqueFD;
}

[[nodiscard]] inline SharedFenceType FromWgpu(WGPUSharedFenceType value) noexcept {
    switch (value) {
        case WGPUSharedFenceType_VkSemaphoreOpaqueFD:
            return SharedFenceType::VkSemaphoreOpaqueFD;
        case WGPUSharedFenceType_SyncFD:
            return SharedFenceType::SyncFD;
        case WGPUSharedFenceType_VkSemaphoreZirconHandle:
            return SharedFenceType::VkSemaphoreZirconHandle;
        case WGPUSharedFenceType_DXGISharedHandle:
            return SharedFenceType::DXGISharedHandle;
        case WGPUSharedFenceType_MTLSharedEvent:
            return SharedFenceType::MTLSharedEvent;
        case WGPUSharedFenceType_EGLSync:
            return SharedFenceType::EGLSync;
        default:
            return SharedFenceType::VkSemaphoreOpaqueFD;
    }
}

[[nodiscard]] inline WGPUStatus ToWgpu(Status value) noexcept {
    switch (value) {
        case Status::Success:
            return WGPUStatus_Success;
        case Status::Error:
            return WGPUStatus_Error;
    }
    return WGPUStatus_Error;
}

[[nodiscard]] inline Status FromWgpu(WGPUStatus value) noexcept {
    switch (value) {
        case WGPUStatus_Success:
            return Status::Success;
        case WGPUStatus_Error:
            return Status::Error;
        default:
            return Status::Error;
    }
}

[[nodiscard]] inline WGPUStencilOperation ToWgpu(StencilOperation value) noexcept {
    switch (value) {
        case StencilOperation::Undefined:
            return WGPUStencilOperation_Undefined;
        case StencilOperation::Keep:
            return WGPUStencilOperation_Keep;
        case StencilOperation::Zero:
            return WGPUStencilOperation_Zero;
        case StencilOperation::Replace:
            return WGPUStencilOperation_Replace;
        case StencilOperation::Invert:
            return WGPUStencilOperation_Invert;
        case StencilOperation::IncrementClamp:
            return WGPUStencilOperation_IncrementClamp;
        case StencilOperation::DecrementClamp:
            return WGPUStencilOperation_DecrementClamp;
        case StencilOperation::IncrementWrap:
            return WGPUStencilOperation_IncrementWrap;
        case StencilOperation::DecrementWrap:
            return WGPUStencilOperation_DecrementWrap;
    }
    return WGPUStencilOperation_Undefined;
}

[[nodiscard]] inline StencilOperation FromWgpu(WGPUStencilOperation value) noexcept {
    switch (value) {
        case WGPUStencilOperation_Undefined:
            return StencilOperation::Undefined;
        case WGPUStencilOperation_Keep:
            return StencilOperation::Keep;
        case WGPUStencilOperation_Zero:
            return StencilOperation::Zero;
        case WGPUStencilOperation_Replace:
            return StencilOperation::Replace;
        case WGPUStencilOperation_Invert:
            return StencilOperation::Invert;
        case WGPUStencilOperation_IncrementClamp:
            return StencilOperation::IncrementClamp;
        case WGPUStencilOperation_DecrementClamp:
            return StencilOperation::DecrementClamp;
        case WGPUStencilOperation_IncrementWrap:
            return StencilOperation::IncrementWrap;
        case WGPUStencilOperation_DecrementWrap:
            return StencilOperation::DecrementWrap;
        default:
            return StencilOperation::Undefined;
    }
}

[[nodiscard]] inline WGPUStorageTextureAccess ToWgpu(StorageTextureAccess value) noexcept {
    switch (value) {
        case StorageTextureAccess::BindingNotUsed:
            return WGPUStorageTextureAccess_BindingNotUsed;
        case StorageTextureAccess::Undefined:
            return WGPUStorageTextureAccess_Undefined;
        case StorageTextureAccess::WriteOnly:
            return WGPUStorageTextureAccess_WriteOnly;
        case StorageTextureAccess::ReadOnly:
            return WGPUStorageTextureAccess_ReadOnly;
        case StorageTextureAccess::ReadWrite:
            return WGPUStorageTextureAccess_ReadWrite;
    }
    return WGPUStorageTextureAccess_Undefined;
}

[[nodiscard]] inline StorageTextureAccess FromWgpu(WGPUStorageTextureAccess value) noexcept {
    switch (value) {
        case WGPUStorageTextureAccess_BindingNotUsed:
            return StorageTextureAccess::BindingNotUsed;
        case WGPUStorageTextureAccess_Undefined:
            return StorageTextureAccess::Undefined;
        case WGPUStorageTextureAccess_WriteOnly:
            return StorageTextureAccess::WriteOnly;
        case WGPUStorageTextureAccess_ReadOnly:
            return StorageTextureAccess::ReadOnly;
        case WGPUStorageTextureAccess_ReadWrite:
            return StorageTextureAccess::ReadWrite;
        default:
            return StorageTextureAccess::Undefined;
    }
}

[[nodiscard]] inline WGPUStoreOp ToWgpu(StoreOp value) noexcept {
    switch (value) {
        case StoreOp::Undefined:
            return WGPUStoreOp_Undefined;
        case StoreOp::Store:
            return WGPUStoreOp_Store;
        case StoreOp::Discard:
            return WGPUStoreOp_Discard;
    }
    return WGPUStoreOp_Undefined;
}

[[nodiscard]] inline StoreOp FromWgpu(WGPUStoreOp value) noexcept {
    switch (value) {
        case WGPUStoreOp_Undefined:
            return StoreOp::Undefined;
        case WGPUStoreOp_Store:
            return StoreOp::Store;
        case WGPUStoreOp_Discard:
            return StoreOp::Discard;
        default:
            return StoreOp::Undefined;
    }
}

[[nodiscard]] inline WGPUSType ToWgpu(SType value) noexcept {
    switch (value) {
        case SType::ShaderSourceSPIRV:
            return WGPUSType_ShaderSourceSPIRV;
        case SType::ShaderSourceWGSL:
            return WGPUSType_ShaderSourceWGSL;
        case SType::RenderPassMaxDrawCount:
            return WGPUSType_RenderPassMaxDrawCount;
        case SType::SurfaceSourceMetalLayer:
            return WGPUSType_SurfaceSourceMetalLayer;
        case SType::SurfaceSourceWindowsHWND:
            return WGPUSType_SurfaceSourceWindowsHWND;
        case SType::SurfaceSourceXlibWindow:
            return WGPUSType_SurfaceSourceXlibWindow;
        case SType::SurfaceSourceWaylandSurface:
            return WGPUSType_SurfaceSourceWaylandSurface;
        case SType::SurfaceSourceAndroidNativeWindow:
            return WGPUSType_SurfaceSourceAndroidNativeWindow;
        case SType::SurfaceSourceXCBWindow:
            return WGPUSType_SurfaceSourceXCBWindow;
        case SType::SurfaceColorManagement:
            return WGPUSType_SurfaceColorManagement;
        case SType::RequestAdapterWebXROptions:
            return WGPUSType_RequestAdapterWebXROptions;
        case SType::TextureComponentSwizzleDescriptor:
            return WGPUSType_TextureComponentSwizzleDescriptor;
        case SType::ExternalTextureBindingLayout:
            return WGPUSType_ExternalTextureBindingLayout;
        case SType::ExternalTextureBindingEntry:
            return WGPUSType_ExternalTextureBindingEntry;
        case SType::CompatibilityModeLimits:
            return WGPUSType_CompatibilityModeLimits;
        case SType::TextureBindingViewDimension:
            return WGPUSType_TextureBindingViewDimension;
        case SType::EmscriptenSurfaceSourceCanvasHTMLSelector:
            return WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
        case SType::SurfaceDescriptorFromWindowsCoreWindow:
            return WGPUSType_SurfaceDescriptorFromWindowsCoreWindow;
        case SType::SurfaceDescriptorFromWindowsUWPSwapChainPanel:
            return WGPUSType_SurfaceDescriptorFromWindowsUWPSwapChainPanel;
        case SType::DawnTextureInternalUsageDescriptor:
            return WGPUSType_DawnTextureInternalUsageDescriptor;
        case SType::DawnEncoderInternalUsageDescriptor:
            return WGPUSType_DawnEncoderInternalUsageDescriptor;
        case SType::DawnInstanceDescriptor:
            return WGPUSType_DawnInstanceDescriptor;
        case SType::DawnCacheDeviceDescriptor:
            return WGPUSType_DawnCacheDeviceDescriptor;
        case SType::DawnAdapterPropertiesPowerPreference:
            return WGPUSType_DawnAdapterPropertiesPowerPreference;
        case SType::DawnBufferDescriptorErrorInfoFromWireClient:
            return WGPUSType_DawnBufferDescriptorErrorInfoFromWireClient;
        case SType::DawnTogglesDescriptor:
            return WGPUSType_DawnTogglesDescriptor;
        case SType::DawnShaderModuleSPIRVOptionsDescriptor:
            return WGPUSType_DawnShaderModuleSPIRVOptionsDescriptor;
        case SType::RequestAdapterOptionsLUID:
            return WGPUSType_RequestAdapterOptionsLUID;
        case SType::RequestAdapterOptionsGetGLProc:
            return WGPUSType_RequestAdapterOptionsGetGLProc;
        case SType::RequestAdapterOptionsD3D11Device:
            return WGPUSType_RequestAdapterOptionsD3D11Device;
        case SType::DawnRenderPassSampleCount:
            return WGPUSType_DawnRenderPassSampleCount;
        case SType::RenderPassPixelLocalStorage:
            return WGPUSType_RenderPassPixelLocalStorage;
        case SType::PipelineLayoutPixelLocalStorage:
            return WGPUSType_PipelineLayoutPixelLocalStorage;
        case SType::BufferHostMappedPointer:
            return WGPUSType_BufferHostMappedPointer;
        case SType::AdapterPropertiesMemoryHeaps:
            return WGPUSType_AdapterPropertiesMemoryHeaps;
        case SType::AdapterPropertiesD3D:
            return WGPUSType_AdapterPropertiesD3D;
        case SType::AdapterPropertiesVk:
            return WGPUSType_AdapterPropertiesVk;
        case SType::DawnWireWGSLControl:
            return WGPUSType_DawnWireWGSLControl;
        case SType::DawnWGSLBlocklist:
            return WGPUSType_DawnWGSLBlocklist;
        case SType::DawnDrmFormatCapabilities:
            return WGPUSType_DawnDrmFormatCapabilities;
        case SType::ShaderModuleCompilationOptions:
            return WGPUSType_ShaderModuleCompilationOptions;
        case SType::ColorTargetStateExpandResolveTextureDawn:
            return WGPUSType_ColorTargetStateExpandResolveTextureDawn;
        case SType::RenderPassRenderAreaRect:
            return WGPUSType_RenderPassRenderAreaRect;
        case SType::SharedTextureMemoryVkDedicatedAllocationDescriptor:
            return WGPUSType_SharedTextureMemoryVkDedicatedAllocationDescriptor;
        case SType::SharedTextureMemoryAHardwareBufferDescriptor:
            return WGPUSType_SharedTextureMemoryAHardwareBufferDescriptor;
        case SType::SharedTextureMemoryDmaBufDescriptor:
            return WGPUSType_SharedTextureMemoryDmaBufDescriptor;
        case SType::SharedTextureMemoryOpaqueFDDescriptor:
            return WGPUSType_SharedTextureMemoryOpaqueFDDescriptor;
        case SType::SharedTextureMemoryZirconHandleDescriptor:
            return WGPUSType_SharedTextureMemoryZirconHandleDescriptor;
        case SType::SharedTextureMemoryDXGISharedHandleDescriptor:
            return WGPUSType_SharedTextureMemoryDXGISharedHandleDescriptor;
        case SType::SharedTextureMemoryD3D11Texture2DDescriptor:
            return WGPUSType_SharedTextureMemoryD3D11Texture2DDescriptor;
        case SType::SharedTextureMemoryIOSurfaceDescriptor:
            return WGPUSType_SharedTextureMemoryIOSurfaceDescriptor;
        case SType::SharedTextureMemoryEGLImageDescriptor:
            return WGPUSType_SharedTextureMemoryEGLImageDescriptor;
        case SType::SharedTextureMemoryInitializedBeginState:
            return WGPUSType_SharedTextureMemoryInitializedBeginState;
        case SType::SharedTextureMemoryInitializedEndState:
            return WGPUSType_SharedTextureMemoryInitializedEndState;
        case SType::SharedTextureMemoryVkImageLayoutBeginState:
            return WGPUSType_SharedTextureMemoryVkImageLayoutBeginState;
        case SType::SharedTextureMemoryVkImageLayoutEndState:
            return WGPUSType_SharedTextureMemoryVkImageLayoutEndState;
        case SType::SharedTextureMemoryD3DSwapchainBeginState:
            return WGPUSType_SharedTextureMemoryD3DSwapchainBeginState;
        case SType::SharedFenceVkSemaphoreOpaqueFDDescriptor:
            return WGPUSType_SharedFenceVkSemaphoreOpaqueFDDescriptor;
        case SType::SharedFenceVkSemaphoreOpaqueFDExportInfo:
            return WGPUSType_SharedFenceVkSemaphoreOpaqueFDExportInfo;
        case SType::SharedFenceSyncFDDescriptor:
            return WGPUSType_SharedFenceSyncFDDescriptor;
        case SType::SharedFenceSyncFDExportInfo:
            return WGPUSType_SharedFenceSyncFDExportInfo;
        case SType::SharedFenceVkSemaphoreZirconHandleDescriptor:
            return WGPUSType_SharedFenceVkSemaphoreZirconHandleDescriptor;
        case SType::SharedFenceVkSemaphoreZirconHandleExportInfo:
            return WGPUSType_SharedFenceVkSemaphoreZirconHandleExportInfo;
        case SType::SharedFenceDXGISharedHandleDescriptor:
            return WGPUSType_SharedFenceDXGISharedHandleDescriptor;
        case SType::SharedFenceDXGISharedHandleExportInfo:
            return WGPUSType_SharedFenceDXGISharedHandleExportInfo;
        case SType::SharedFenceMTLSharedEventDescriptor:
            return WGPUSType_SharedFenceMTLSharedEventDescriptor;
        case SType::SharedFenceMTLSharedEventExportInfo:
            return WGPUSType_SharedFenceMTLSharedEventExportInfo;
        case SType::SharedBufferMemoryD3D12ResourceDescriptor:
            return WGPUSType_SharedBufferMemoryD3D12ResourceDescriptor;
        case SType::StaticSamplerBindingLayout:
            return WGPUSType_StaticSamplerBindingLayout;
        case SType::YCbCrVkDescriptor:
            return WGPUSType_YCbCrVkDescriptor;
        case SType::SharedTextureMemoryAHardwareBufferProperties:
            return WGPUSType_SharedTextureMemoryAHardwareBufferProperties;
        case SType::AHardwareBufferProperties:
            return WGPUSType_AHardwareBufferProperties;
        case SType::DawnTexelCopyBufferRowAlignmentLimits:
            return WGPUSType_DawnTexelCopyBufferRowAlignmentLimits;
        case SType::AdapterPropertiesSubgroupMatrixConfigs:
            return WGPUSType_AdapterPropertiesSubgroupMatrixConfigs;
        case SType::SharedFenceEGLSyncDescriptor:
            return WGPUSType_SharedFenceEGLSyncDescriptor;
        case SType::SharedFenceEGLSyncExportInfo:
            return WGPUSType_SharedFenceEGLSyncExportInfo;
        case SType::DawnInjectedInvalidSType:
            return WGPUSType_DawnInjectedInvalidSType;
        case SType::DawnCompilationMessageUtf16:
            return WGPUSType_DawnCompilationMessageUtf16;
        case SType::DawnFakeBufferOOMForTesting:
            return WGPUSType_DawnFakeBufferOOMForTesting;
        case SType::SurfaceDescriptorFromWindowsWinUISwapChainPanel:
            return WGPUSType_SurfaceDescriptorFromWindowsWinUISwapChainPanel;
        case SType::DawnDeviceAllocatorControl:
            return WGPUSType_DawnDeviceAllocatorControl;
        case SType::DawnHostMappedPointerLimits:
            return WGPUSType_DawnHostMappedPointerLimits;
        case SType::RenderPassDescriptorResolveRect:
            return WGPUSType_RenderPassDescriptorResolveRect;
        case SType::RequestAdapterWebGPUBackendOptions:
            return WGPUSType_RequestAdapterWebGPUBackendOptions;
        case SType::DawnFakeDeviceInitializeErrorForTesting:
            return WGPUSType_DawnFakeDeviceInitializeErrorForTesting;
        case SType::SharedTextureMemoryD3D11BeginState:
            return WGPUSType_SharedTextureMemoryD3D11BeginState;
        case SType::DawnConsumeAdapterDescriptor:
            return WGPUSType_DawnConsumeAdapterDescriptor;
        case SType::TexelBufferBindingEntry:
            return WGPUSType_TexelBufferBindingEntry;
        case SType::TexelBufferBindingLayout:
            return WGPUSType_TexelBufferBindingLayout;
        case SType::SharedTextureMemoryMetalEndAccessState:
            return WGPUSType_SharedTextureMemoryMetalEndAccessState;
        case SType::AdapterPropertiesWGPU:
            return WGPUSType_AdapterPropertiesWGPU;
        case SType::SharedBufferMemoryD3D12SharedMemoryFileMappingHandleDescriptor:
            return WGPUSType_ShaderSourceSPIRV;
        case SType::SharedTextureMemoryD3D12ResourceDescriptor:
            return WGPUSType_SharedTextureMemoryD3D12ResourceDescriptor;
        case SType::RequestAdapterOptionsAngleVirtualizationGroup:
            return WGPUSType_RequestAdapterOptionsAngleVirtualizationGroup;
        case SType::PipelineLayoutResourceTable:
            return WGPUSType_PipelineLayoutResourceTable;
        case SType::AdapterPropertiesDrm:
            return WGPUSType_AdapterPropertiesDrm;
    }
    return WGPUSType_ShaderSourceSPIRV;
}

[[nodiscard]] inline SType FromWgpu(WGPUSType value) noexcept {
    switch (value) {
        case WGPUSType_ShaderSourceSPIRV:
            return SType::ShaderSourceSPIRV;
        case WGPUSType_ShaderSourceWGSL:
            return SType::ShaderSourceWGSL;
        case WGPUSType_RenderPassMaxDrawCount:
            return SType::RenderPassMaxDrawCount;
        case WGPUSType_SurfaceSourceMetalLayer:
            return SType::SurfaceSourceMetalLayer;
        case WGPUSType_SurfaceSourceWindowsHWND:
            return SType::SurfaceSourceWindowsHWND;
        case WGPUSType_SurfaceSourceXlibWindow:
            return SType::SurfaceSourceXlibWindow;
        case WGPUSType_SurfaceSourceWaylandSurface:
            return SType::SurfaceSourceWaylandSurface;
        case WGPUSType_SurfaceSourceAndroidNativeWindow:
            return SType::SurfaceSourceAndroidNativeWindow;
        case WGPUSType_SurfaceSourceXCBWindow:
            return SType::SurfaceSourceXCBWindow;
        case WGPUSType_SurfaceColorManagement:
            return SType::SurfaceColorManagement;
        case WGPUSType_RequestAdapterWebXROptions:
            return SType::RequestAdapterWebXROptions;
        case WGPUSType_TextureComponentSwizzleDescriptor:
            return SType::TextureComponentSwizzleDescriptor;
        case WGPUSType_ExternalTextureBindingLayout:
            return SType::ExternalTextureBindingLayout;
        case WGPUSType_ExternalTextureBindingEntry:
            return SType::ExternalTextureBindingEntry;
        case WGPUSType_CompatibilityModeLimits:
            return SType::CompatibilityModeLimits;
        case WGPUSType_TextureBindingViewDimension:
            return SType::TextureBindingViewDimension;
        case WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector:
            return SType::EmscriptenSurfaceSourceCanvasHTMLSelector;
        case WGPUSType_SurfaceDescriptorFromWindowsCoreWindow:
            return SType::SurfaceDescriptorFromWindowsCoreWindow;
        case WGPUSType_SurfaceDescriptorFromWindowsUWPSwapChainPanel:
            return SType::SurfaceDescriptorFromWindowsUWPSwapChainPanel;
        case WGPUSType_DawnTextureInternalUsageDescriptor:
            return SType::DawnTextureInternalUsageDescriptor;
        case WGPUSType_DawnEncoderInternalUsageDescriptor:
            return SType::DawnEncoderInternalUsageDescriptor;
        case WGPUSType_DawnInstanceDescriptor:
            return SType::DawnInstanceDescriptor;
        case WGPUSType_DawnCacheDeviceDescriptor:
            return SType::DawnCacheDeviceDescriptor;
        case WGPUSType_DawnAdapterPropertiesPowerPreference:
            return SType::DawnAdapterPropertiesPowerPreference;
        case WGPUSType_DawnBufferDescriptorErrorInfoFromWireClient:
            return SType::DawnBufferDescriptorErrorInfoFromWireClient;
        case WGPUSType_DawnTogglesDescriptor:
            return SType::DawnTogglesDescriptor;
        case WGPUSType_DawnShaderModuleSPIRVOptionsDescriptor:
            return SType::DawnShaderModuleSPIRVOptionsDescriptor;
        case WGPUSType_RequestAdapterOptionsLUID:
            return SType::RequestAdapterOptionsLUID;
        case WGPUSType_RequestAdapterOptionsGetGLProc:
            return SType::RequestAdapterOptionsGetGLProc;
        case WGPUSType_RequestAdapterOptionsD3D11Device:
            return SType::RequestAdapterOptionsD3D11Device;
        case WGPUSType_DawnRenderPassSampleCount:
            return SType::DawnRenderPassSampleCount;
        case WGPUSType_RenderPassPixelLocalStorage:
            return SType::RenderPassPixelLocalStorage;
        case WGPUSType_PipelineLayoutPixelLocalStorage:
            return SType::PipelineLayoutPixelLocalStorage;
        case WGPUSType_BufferHostMappedPointer:
            return SType::BufferHostMappedPointer;
        case WGPUSType_AdapterPropertiesMemoryHeaps:
            return SType::AdapterPropertiesMemoryHeaps;
        case WGPUSType_AdapterPropertiesD3D:
            return SType::AdapterPropertiesD3D;
        case WGPUSType_AdapterPropertiesVk:
            return SType::AdapterPropertiesVk;
        case WGPUSType_DawnWireWGSLControl:
            return SType::DawnWireWGSLControl;
        case WGPUSType_DawnWGSLBlocklist:
            return SType::DawnWGSLBlocklist;
        case WGPUSType_DawnDrmFormatCapabilities:
            return SType::DawnDrmFormatCapabilities;
        case WGPUSType_ShaderModuleCompilationOptions:
            return SType::ShaderModuleCompilationOptions;
        case WGPUSType_ColorTargetStateExpandResolveTextureDawn:
            return SType::ColorTargetStateExpandResolveTextureDawn;
        case WGPUSType_RenderPassRenderAreaRect:
            return SType::RenderPassRenderAreaRect;
        case WGPUSType_SharedTextureMemoryVkDedicatedAllocationDescriptor:
            return SType::SharedTextureMemoryVkDedicatedAllocationDescriptor;
        case WGPUSType_SharedTextureMemoryAHardwareBufferDescriptor:
            return SType::SharedTextureMemoryAHardwareBufferDescriptor;
        case WGPUSType_SharedTextureMemoryDmaBufDescriptor:
            return SType::SharedTextureMemoryDmaBufDescriptor;
        case WGPUSType_SharedTextureMemoryOpaqueFDDescriptor:
            return SType::SharedTextureMemoryOpaqueFDDescriptor;
        case WGPUSType_SharedTextureMemoryZirconHandleDescriptor:
            return SType::SharedTextureMemoryZirconHandleDescriptor;
        case WGPUSType_SharedTextureMemoryDXGISharedHandleDescriptor:
            return SType::SharedTextureMemoryDXGISharedHandleDescriptor;
        case WGPUSType_SharedTextureMemoryD3D11Texture2DDescriptor:
            return SType::SharedTextureMemoryD3D11Texture2DDescriptor;
        case WGPUSType_SharedTextureMemoryIOSurfaceDescriptor:
            return SType::SharedTextureMemoryIOSurfaceDescriptor;
        case WGPUSType_SharedTextureMemoryEGLImageDescriptor:
            return SType::SharedTextureMemoryEGLImageDescriptor;
        case WGPUSType_SharedTextureMemoryInitializedBeginState:
            return SType::SharedTextureMemoryInitializedBeginState;
        case WGPUSType_SharedTextureMemoryInitializedEndState:
            return SType::SharedTextureMemoryInitializedEndState;
        case WGPUSType_SharedTextureMemoryVkImageLayoutBeginState:
            return SType::SharedTextureMemoryVkImageLayoutBeginState;
        case WGPUSType_SharedTextureMemoryVkImageLayoutEndState:
            return SType::SharedTextureMemoryVkImageLayoutEndState;
        case WGPUSType_SharedTextureMemoryD3DSwapchainBeginState:
            return SType::SharedTextureMemoryD3DSwapchainBeginState;
        case WGPUSType_SharedFenceVkSemaphoreOpaqueFDDescriptor:
            return SType::SharedFenceVkSemaphoreOpaqueFDDescriptor;
        case WGPUSType_SharedFenceVkSemaphoreOpaqueFDExportInfo:
            return SType::SharedFenceVkSemaphoreOpaqueFDExportInfo;
        case WGPUSType_SharedFenceSyncFDDescriptor:
            return SType::SharedFenceSyncFDDescriptor;
        case WGPUSType_SharedFenceSyncFDExportInfo:
            return SType::SharedFenceSyncFDExportInfo;
        case WGPUSType_SharedFenceVkSemaphoreZirconHandleDescriptor:
            return SType::SharedFenceVkSemaphoreZirconHandleDescriptor;
        case WGPUSType_SharedFenceVkSemaphoreZirconHandleExportInfo:
            return SType::SharedFenceVkSemaphoreZirconHandleExportInfo;
        case WGPUSType_SharedFenceDXGISharedHandleDescriptor:
            return SType::SharedFenceDXGISharedHandleDescriptor;
        case WGPUSType_SharedFenceDXGISharedHandleExportInfo:
            return SType::SharedFenceDXGISharedHandleExportInfo;
        case WGPUSType_SharedFenceMTLSharedEventDescriptor:
            return SType::SharedFenceMTLSharedEventDescriptor;
        case WGPUSType_SharedFenceMTLSharedEventExportInfo:
            return SType::SharedFenceMTLSharedEventExportInfo;
        case WGPUSType_SharedBufferMemoryD3D12ResourceDescriptor:
            return SType::SharedBufferMemoryD3D12ResourceDescriptor;
        case WGPUSType_StaticSamplerBindingLayout:
            return SType::StaticSamplerBindingLayout;
        case WGPUSType_YCbCrVkDescriptor:
            return SType::YCbCrVkDescriptor;
        case WGPUSType_SharedTextureMemoryAHardwareBufferProperties:
            return SType::SharedTextureMemoryAHardwareBufferProperties;
        case WGPUSType_AHardwareBufferProperties:
            return SType::AHardwareBufferProperties;
        case WGPUSType_DawnTexelCopyBufferRowAlignmentLimits:
            return SType::DawnTexelCopyBufferRowAlignmentLimits;
        case WGPUSType_AdapterPropertiesSubgroupMatrixConfigs:
            return SType::AdapterPropertiesSubgroupMatrixConfigs;
        case WGPUSType_SharedFenceEGLSyncDescriptor:
            return SType::SharedFenceEGLSyncDescriptor;
        case WGPUSType_SharedFenceEGLSyncExportInfo:
            return SType::SharedFenceEGLSyncExportInfo;
        case WGPUSType_DawnInjectedInvalidSType:
            return SType::DawnInjectedInvalidSType;
        case WGPUSType_DawnCompilationMessageUtf16:
            return SType::DawnCompilationMessageUtf16;
        case WGPUSType_DawnFakeBufferOOMForTesting:
            return SType::DawnFakeBufferOOMForTesting;
        case WGPUSType_SurfaceDescriptorFromWindowsWinUISwapChainPanel:
            return SType::SurfaceDescriptorFromWindowsWinUISwapChainPanel;
        case WGPUSType_DawnDeviceAllocatorControl:
            return SType::DawnDeviceAllocatorControl;
        case WGPUSType_DawnHostMappedPointerLimits:
            return SType::DawnHostMappedPointerLimits;
        case WGPUSType_RenderPassDescriptorResolveRect:
            return SType::RenderPassDescriptorResolveRect;
        case WGPUSType_RequestAdapterWebGPUBackendOptions:
            return SType::RequestAdapterWebGPUBackendOptions;
        case WGPUSType_DawnFakeDeviceInitializeErrorForTesting:
            return SType::DawnFakeDeviceInitializeErrorForTesting;
        case WGPUSType_SharedTextureMemoryD3D11BeginState:
            return SType::SharedTextureMemoryD3D11BeginState;
        case WGPUSType_DawnConsumeAdapterDescriptor:
            return SType::DawnConsumeAdapterDescriptor;
        case WGPUSType_TexelBufferBindingEntry:
            return SType::TexelBufferBindingEntry;
        case WGPUSType_TexelBufferBindingLayout:
            return SType::TexelBufferBindingLayout;
        case WGPUSType_SharedTextureMemoryMetalEndAccessState:
            return SType::SharedTextureMemoryMetalEndAccessState;
        case WGPUSType_AdapterPropertiesWGPU:
            return SType::AdapterPropertiesWGPU;
        case WGPUSType_SharedTextureMemoryD3D12ResourceDescriptor:
            return SType::SharedTextureMemoryD3D12ResourceDescriptor;
        case WGPUSType_RequestAdapterOptionsAngleVirtualizationGroup:
            return SType::RequestAdapterOptionsAngleVirtualizationGroup;
        case WGPUSType_PipelineLayoutResourceTable:
            return SType::PipelineLayoutResourceTable;
        case WGPUSType_AdapterPropertiesDrm:
            return SType::AdapterPropertiesDrm;
        default:
            return SType::ShaderSourceSPIRV;
    }
}

[[nodiscard]] inline WGPUSubgroupMatrixComponentType ToWgpu(SubgroupMatrixComponentType value) noexcept {
    switch (value) {
        case SubgroupMatrixComponentType::F32:
            return WGPUSubgroupMatrixComponentType_F32;
        case SubgroupMatrixComponentType::F16:
            return WGPUSubgroupMatrixComponentType_F16;
        case SubgroupMatrixComponentType::U32:
            return WGPUSubgroupMatrixComponentType_U32;
        case SubgroupMatrixComponentType::I32:
            return WGPUSubgroupMatrixComponentType_I32;
        case SubgroupMatrixComponentType::U8:
            return WGPUSubgroupMatrixComponentType_U8;
        case SubgroupMatrixComponentType::I8:
            return WGPUSubgroupMatrixComponentType_I8;
    }
    return WGPUSubgroupMatrixComponentType_F32;
}

[[nodiscard]] inline SubgroupMatrixComponentType FromWgpu(WGPUSubgroupMatrixComponentType value) noexcept {
    switch (value) {
        case WGPUSubgroupMatrixComponentType_F32:
            return SubgroupMatrixComponentType::F32;
        case WGPUSubgroupMatrixComponentType_F16:
            return SubgroupMatrixComponentType::F16;
        case WGPUSubgroupMatrixComponentType_U32:
            return SubgroupMatrixComponentType::U32;
        case WGPUSubgroupMatrixComponentType_I32:
            return SubgroupMatrixComponentType::I32;
        case WGPUSubgroupMatrixComponentType_U8:
            return SubgroupMatrixComponentType::U8;
        case WGPUSubgroupMatrixComponentType_I8:
            return SubgroupMatrixComponentType::I8;
        default:
            return SubgroupMatrixComponentType::F32;
    }
}

[[nodiscard]] inline WGPUSurfaceGetCurrentTextureStatus ToWgpu(SurfaceGetCurrentTextureStatus value) noexcept {
    switch (value) {
        case SurfaceGetCurrentTextureStatus::SuccessOptimal:
            return WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal;
        case SurfaceGetCurrentTextureStatus::SuccessSuboptimal:
            return WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal;
        case SurfaceGetCurrentTextureStatus::Timeout:
            return WGPUSurfaceGetCurrentTextureStatus_Timeout;
        case SurfaceGetCurrentTextureStatus::Outdated:
            return WGPUSurfaceGetCurrentTextureStatus_Outdated;
        case SurfaceGetCurrentTextureStatus::Lost:
            return WGPUSurfaceGetCurrentTextureStatus_Lost;
        case SurfaceGetCurrentTextureStatus::Error:
            return WGPUSurfaceGetCurrentTextureStatus_Error;
    }
    return WGPUSurfaceGetCurrentTextureStatus_Error;
}

[[nodiscard]] inline SurfaceGetCurrentTextureStatus FromWgpu(WGPUSurfaceGetCurrentTextureStatus value) noexcept {
    switch (value) {
        case WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal:
            return SurfaceGetCurrentTextureStatus::SuccessOptimal;
        case WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal:
            return SurfaceGetCurrentTextureStatus::SuccessSuboptimal;
        case WGPUSurfaceGetCurrentTextureStatus_Timeout:
            return SurfaceGetCurrentTextureStatus::Timeout;
        case WGPUSurfaceGetCurrentTextureStatus_Outdated:
            return SurfaceGetCurrentTextureStatus::Outdated;
        case WGPUSurfaceGetCurrentTextureStatus_Lost:
            return SurfaceGetCurrentTextureStatus::Lost;
        case WGPUSurfaceGetCurrentTextureStatus_Error:
            return SurfaceGetCurrentTextureStatus::Error;
        default:
            return SurfaceGetCurrentTextureStatus::Error;
    }
}

[[nodiscard]] inline WGPUTexelBufferAccess ToWgpu(TexelBufferAccess value) noexcept {
    switch (value) {
        case TexelBufferAccess::Undefined:
            return WGPUTexelBufferAccess_Undefined;
        case TexelBufferAccess::ReadOnly:
            return WGPUTexelBufferAccess_ReadOnly;
        case TexelBufferAccess::ReadWrite:
            return WGPUTexelBufferAccess_ReadWrite;
    }
    return WGPUTexelBufferAccess_Undefined;
}

[[nodiscard]] inline TexelBufferAccess FromWgpu(WGPUTexelBufferAccess value) noexcept {
    switch (value) {
        case WGPUTexelBufferAccess_Undefined:
            return TexelBufferAccess::Undefined;
        case WGPUTexelBufferAccess_ReadOnly:
            return TexelBufferAccess::ReadOnly;
        case WGPUTexelBufferAccess_ReadWrite:
            return TexelBufferAccess::ReadWrite;
        default:
            return TexelBufferAccess::Undefined;
    }
}

[[nodiscard]] inline WGPUTextureAspect ToWgpu(TextureAspect value) noexcept {
    switch (value) {
        case TextureAspect::Undefined:
            return WGPUTextureAspect_Undefined;
        case TextureAspect::All:
            return WGPUTextureAspect_All;
        case TextureAspect::StencilOnly:
            return WGPUTextureAspect_StencilOnly;
        case TextureAspect::DepthOnly:
            return WGPUTextureAspect_DepthOnly;
        case TextureAspect::Plane0Only:
            return WGPUTextureAspect_Plane0Only;
        case TextureAspect::Plane1Only:
            return WGPUTextureAspect_Plane1Only;
        case TextureAspect::Plane2Only:
            return WGPUTextureAspect_Plane2Only;
    }
    return WGPUTextureAspect_Undefined;
}

[[nodiscard]] inline TextureAspect FromWgpu(WGPUTextureAspect value) noexcept {
    switch (value) {
        case WGPUTextureAspect_Undefined:
            return TextureAspect::Undefined;
        case WGPUTextureAspect_All:
            return TextureAspect::All;
        case WGPUTextureAspect_StencilOnly:
            return TextureAspect::StencilOnly;
        case WGPUTextureAspect_DepthOnly:
            return TextureAspect::DepthOnly;
        case WGPUTextureAspect_Plane0Only:
            return TextureAspect::Plane0Only;
        case WGPUTextureAspect_Plane1Only:
            return TextureAspect::Plane1Only;
        case WGPUTextureAspect_Plane2Only:
            return TextureAspect::Plane2Only;
        default:
            return TextureAspect::Undefined;
    }
}

[[nodiscard]] inline WGPUTextureDimension ToWgpu(TextureDimension value) noexcept {
    switch (value) {
        case TextureDimension::Undefined:
            return WGPUTextureDimension_Undefined;
        case TextureDimension::e1D:
            return WGPUTextureDimension_1D;
        case TextureDimension::e2D:
            return WGPUTextureDimension_2D;
        case TextureDimension::e3D:
            return WGPUTextureDimension_3D;
    }
    return WGPUTextureDimension_Undefined;
}

[[nodiscard]] inline TextureDimension FromWgpu(WGPUTextureDimension value) noexcept {
    switch (value) {
        case WGPUTextureDimension_Undefined:
            return TextureDimension::Undefined;
        case WGPUTextureDimension_1D:
            return TextureDimension::e1D;
        case WGPUTextureDimension_2D:
            return TextureDimension::e2D;
        case WGPUTextureDimension_3D:
            return TextureDimension::e3D;
        default:
            return TextureDimension::Undefined;
    }
}

[[nodiscard]] inline WGPUTextureFormat ToWgpu(TextureFormat value) noexcept {
    switch (value) {
        case TextureFormat::Undefined:
            return WGPUTextureFormat_Undefined;
        case TextureFormat::R8Unorm:
            return WGPUTextureFormat_R8Unorm;
        case TextureFormat::R8Snorm:
            return WGPUTextureFormat_R8Snorm;
        case TextureFormat::R8Uint:
            return WGPUTextureFormat_R8Uint;
        case TextureFormat::R8Sint:
            return WGPUTextureFormat_R8Sint;
        case TextureFormat::R16Unorm:
            return WGPUTextureFormat_R16Unorm;
        case TextureFormat::R16Snorm:
            return WGPUTextureFormat_R16Snorm;
        case TextureFormat::R16Uint:
            return WGPUTextureFormat_R16Uint;
        case TextureFormat::R16Sint:
            return WGPUTextureFormat_R16Sint;
        case TextureFormat::R16Float:
            return WGPUTextureFormat_R16Float;
        case TextureFormat::RG8Unorm:
            return WGPUTextureFormat_RG8Unorm;
        case TextureFormat::RG8Snorm:
            return WGPUTextureFormat_RG8Snorm;
        case TextureFormat::RG8Uint:
            return WGPUTextureFormat_RG8Uint;
        case TextureFormat::RG8Sint:
            return WGPUTextureFormat_RG8Sint;
        case TextureFormat::R32Float:
            return WGPUTextureFormat_R32Float;
        case TextureFormat::R32Uint:
            return WGPUTextureFormat_R32Uint;
        case TextureFormat::R32Sint:
            return WGPUTextureFormat_R32Sint;
        case TextureFormat::RG16Unorm:
            return WGPUTextureFormat_RG16Unorm;
        case TextureFormat::RG16Snorm:
            return WGPUTextureFormat_RG16Snorm;
        case TextureFormat::RG16Uint:
            return WGPUTextureFormat_RG16Uint;
        case TextureFormat::RG16Sint:
            return WGPUTextureFormat_RG16Sint;
        case TextureFormat::RG16Float:
            return WGPUTextureFormat_RG16Float;
        case TextureFormat::RGBA8Unorm:
            return WGPUTextureFormat_RGBA8Unorm;
        case TextureFormat::RGBA8UnormSrgb:
            return WGPUTextureFormat_RGBA8UnormSrgb;
        case TextureFormat::RGBA8Snorm:
            return WGPUTextureFormat_RGBA8Snorm;
        case TextureFormat::RGBA8Uint:
            return WGPUTextureFormat_RGBA8Uint;
        case TextureFormat::RGBA8Sint:
            return WGPUTextureFormat_RGBA8Sint;
        case TextureFormat::BGRA8Unorm:
            return WGPUTextureFormat_BGRA8Unorm;
        case TextureFormat::BGRA8UnormSrgb:
            return WGPUTextureFormat_BGRA8UnormSrgb;
        case TextureFormat::RGB10A2Uint:
            return WGPUTextureFormat_RGB10A2Uint;
        case TextureFormat::RGB10A2Unorm:
            return WGPUTextureFormat_RGB10A2Unorm;
        case TextureFormat::RG11B10Ufloat:
            return WGPUTextureFormat_RG11B10Ufloat;
        case TextureFormat::RGB9E5Ufloat:
            return WGPUTextureFormat_RGB9E5Ufloat;
        case TextureFormat::RG32Float:
            return WGPUTextureFormat_RG32Float;
        case TextureFormat::RG32Uint:
            return WGPUTextureFormat_RG32Uint;
        case TextureFormat::RG32Sint:
            return WGPUTextureFormat_RG32Sint;
        case TextureFormat::RGBA16Unorm:
            return WGPUTextureFormat_RGBA16Unorm;
        case TextureFormat::RGBA16Snorm:
            return WGPUTextureFormat_RGBA16Snorm;
        case TextureFormat::RGBA16Uint:
            return WGPUTextureFormat_RGBA16Uint;
        case TextureFormat::RGBA16Sint:
            return WGPUTextureFormat_RGBA16Sint;
        case TextureFormat::RGBA16Float:
            return WGPUTextureFormat_RGBA16Float;
        case TextureFormat::RGBA32Float:
            return WGPUTextureFormat_RGBA32Float;
        case TextureFormat::RGBA32Uint:
            return WGPUTextureFormat_RGBA32Uint;
        case TextureFormat::RGBA32Sint:
            return WGPUTextureFormat_RGBA32Sint;
        case TextureFormat::Stencil8:
            return WGPUTextureFormat_Stencil8;
        case TextureFormat::Depth16Unorm:
            return WGPUTextureFormat_Depth16Unorm;
        case TextureFormat::Depth24Plus:
            return WGPUTextureFormat_Depth24Plus;
        case TextureFormat::Depth24PlusStencil8:
            return WGPUTextureFormat_Depth24PlusStencil8;
        case TextureFormat::Depth32Float:
            return WGPUTextureFormat_Depth32Float;
        case TextureFormat::Depth32FloatStencil8:
            return WGPUTextureFormat_Depth32FloatStencil8;
        case TextureFormat::BC1RGBAUnorm:
            return WGPUTextureFormat_BC1RGBAUnorm;
        case TextureFormat::BC1RGBAUnormSrgb:
            return WGPUTextureFormat_BC1RGBAUnormSrgb;
        case TextureFormat::BC2RGBAUnorm:
            return WGPUTextureFormat_BC2RGBAUnorm;
        case TextureFormat::BC2RGBAUnormSrgb:
            return WGPUTextureFormat_BC2RGBAUnormSrgb;
        case TextureFormat::BC3RGBAUnorm:
            return WGPUTextureFormat_BC3RGBAUnorm;
        case TextureFormat::BC3RGBAUnormSrgb:
            return WGPUTextureFormat_BC3RGBAUnormSrgb;
        case TextureFormat::BC4RUnorm:
            return WGPUTextureFormat_BC4RUnorm;
        case TextureFormat::BC4RSnorm:
            return WGPUTextureFormat_BC4RSnorm;
        case TextureFormat::BC5RGUnorm:
            return WGPUTextureFormat_BC5RGUnorm;
        case TextureFormat::BC5RGSnorm:
            return WGPUTextureFormat_BC5RGSnorm;
        case TextureFormat::BC6HRGBUfloat:
            return WGPUTextureFormat_BC6HRGBUfloat;
        case TextureFormat::BC6HRGBFloat:
            return WGPUTextureFormat_BC6HRGBFloat;
        case TextureFormat::BC7RGBAUnorm:
            return WGPUTextureFormat_BC7RGBAUnorm;
        case TextureFormat::BC7RGBAUnormSrgb:
            return WGPUTextureFormat_BC7RGBAUnormSrgb;
        case TextureFormat::ETC2RGB8Unorm:
            return WGPUTextureFormat_ETC2RGB8Unorm;
        case TextureFormat::ETC2RGB8UnormSrgb:
            return WGPUTextureFormat_ETC2RGB8UnormSrgb;
        case TextureFormat::ETC2RGB8A1Unorm:
            return WGPUTextureFormat_ETC2RGB8A1Unorm;
        case TextureFormat::ETC2RGB8A1UnormSrgb:
            return WGPUTextureFormat_ETC2RGB8A1UnormSrgb;
        case TextureFormat::ETC2RGBA8Unorm:
            return WGPUTextureFormat_ETC2RGBA8Unorm;
        case TextureFormat::ETC2RGBA8UnormSrgb:
            return WGPUTextureFormat_ETC2RGBA8UnormSrgb;
        case TextureFormat::EACR11Unorm:
            return WGPUTextureFormat_EACR11Unorm;
        case TextureFormat::EACR11Snorm:
            return WGPUTextureFormat_EACR11Snorm;
        case TextureFormat::EACRG11Unorm:
            return WGPUTextureFormat_EACRG11Unorm;
        case TextureFormat::EACRG11Snorm:
            return WGPUTextureFormat_EACRG11Snorm;
        case TextureFormat::ASTC4x4Unorm:
            return WGPUTextureFormat_ASTC4x4Unorm;
        case TextureFormat::ASTC4x4UnormSrgb:
            return WGPUTextureFormat_ASTC4x4UnormSrgb;
        case TextureFormat::ASTC5x4Unorm:
            return WGPUTextureFormat_ASTC5x4Unorm;
        case TextureFormat::ASTC5x4UnormSrgb:
            return WGPUTextureFormat_ASTC5x4UnormSrgb;
        case TextureFormat::ASTC5x5Unorm:
            return WGPUTextureFormat_ASTC5x5Unorm;
        case TextureFormat::ASTC5x5UnormSrgb:
            return WGPUTextureFormat_ASTC5x5UnormSrgb;
        case TextureFormat::ASTC6x5Unorm:
            return WGPUTextureFormat_ASTC6x5Unorm;
        case TextureFormat::ASTC6x5UnormSrgb:
            return WGPUTextureFormat_ASTC6x5UnormSrgb;
        case TextureFormat::ASTC6x6Unorm:
            return WGPUTextureFormat_ASTC6x6Unorm;
        case TextureFormat::ASTC6x6UnormSrgb:
            return WGPUTextureFormat_ASTC6x6UnormSrgb;
        case TextureFormat::ASTC8x5Unorm:
            return WGPUTextureFormat_ASTC8x5Unorm;
        case TextureFormat::ASTC8x5UnormSrgb:
            return WGPUTextureFormat_ASTC8x5UnormSrgb;
        case TextureFormat::ASTC8x6Unorm:
            return WGPUTextureFormat_ASTC8x6Unorm;
        case TextureFormat::ASTC8x6UnormSrgb:
            return WGPUTextureFormat_ASTC8x6UnormSrgb;
        case TextureFormat::ASTC8x8Unorm:
            return WGPUTextureFormat_ASTC8x8Unorm;
        case TextureFormat::ASTC8x8UnormSrgb:
            return WGPUTextureFormat_ASTC8x8UnormSrgb;
        case TextureFormat::ASTC10x5Unorm:
            return WGPUTextureFormat_ASTC10x5Unorm;
        case TextureFormat::ASTC10x5UnormSrgb:
            return WGPUTextureFormat_ASTC10x5UnormSrgb;
        case TextureFormat::ASTC10x6Unorm:
            return WGPUTextureFormat_ASTC10x6Unorm;
        case TextureFormat::ASTC10x6UnormSrgb:
            return WGPUTextureFormat_ASTC10x6UnormSrgb;
        case TextureFormat::ASTC10x8Unorm:
            return WGPUTextureFormat_ASTC10x8Unorm;
        case TextureFormat::ASTC10x8UnormSrgb:
            return WGPUTextureFormat_ASTC10x8UnormSrgb;
        case TextureFormat::ASTC10x10Unorm:
            return WGPUTextureFormat_ASTC10x10Unorm;
        case TextureFormat::ASTC10x10UnormSrgb:
            return WGPUTextureFormat_ASTC10x10UnormSrgb;
        case TextureFormat::ASTC12x10Unorm:
            return WGPUTextureFormat_ASTC12x10Unorm;
        case TextureFormat::ASTC12x10UnormSrgb:
            return WGPUTextureFormat_ASTC12x10UnormSrgb;
        case TextureFormat::ASTC12x12Unorm:
            return WGPUTextureFormat_ASTC12x12Unorm;
        case TextureFormat::ASTC12x12UnormSrgb:
            return WGPUTextureFormat_ASTC12x12UnormSrgb;
        case TextureFormat::R8BG8Biplanar420Unorm:
            return WGPUTextureFormat_R8BG8Biplanar420Unorm;
        case TextureFormat::R10X6BG10X6Biplanar420Unorm:
            return WGPUTextureFormat_R10X6BG10X6Biplanar420Unorm;
        case TextureFormat::R8BG8A8Triplanar420Unorm:
            return WGPUTextureFormat_R8BG8A8Triplanar420Unorm;
        case TextureFormat::R8BG8Biplanar422Unorm:
            return WGPUTextureFormat_R8BG8Biplanar422Unorm;
        case TextureFormat::R8BG8Biplanar444Unorm:
            return WGPUTextureFormat_R8BG8Biplanar444Unorm;
        case TextureFormat::R10X6BG10X6Biplanar422Unorm:
            return WGPUTextureFormat_R10X6BG10X6Biplanar422Unorm;
        case TextureFormat::R10X6BG10X6Biplanar444Unorm:
            return WGPUTextureFormat_R10X6BG10X6Biplanar444Unorm;
        case TextureFormat::OpaqueYCbCrAndroid:
            return WGPUTextureFormat_OpaqueYCbCrAndroid;
    }
    return WGPUTextureFormat_Undefined;
}

[[nodiscard]] inline TextureFormat FromWgpu(WGPUTextureFormat value) noexcept {
    switch (value) {
        case WGPUTextureFormat_Undefined:
            return TextureFormat::Undefined;
        case WGPUTextureFormat_R8Unorm:
            return TextureFormat::R8Unorm;
        case WGPUTextureFormat_R8Snorm:
            return TextureFormat::R8Snorm;
        case WGPUTextureFormat_R8Uint:
            return TextureFormat::R8Uint;
        case WGPUTextureFormat_R8Sint:
            return TextureFormat::R8Sint;
        case WGPUTextureFormat_R16Unorm:
            return TextureFormat::R16Unorm;
        case WGPUTextureFormat_R16Snorm:
            return TextureFormat::R16Snorm;
        case WGPUTextureFormat_R16Uint:
            return TextureFormat::R16Uint;
        case WGPUTextureFormat_R16Sint:
            return TextureFormat::R16Sint;
        case WGPUTextureFormat_R16Float:
            return TextureFormat::R16Float;
        case WGPUTextureFormat_RG8Unorm:
            return TextureFormat::RG8Unorm;
        case WGPUTextureFormat_RG8Snorm:
            return TextureFormat::RG8Snorm;
        case WGPUTextureFormat_RG8Uint:
            return TextureFormat::RG8Uint;
        case WGPUTextureFormat_RG8Sint:
            return TextureFormat::RG8Sint;
        case WGPUTextureFormat_R32Float:
            return TextureFormat::R32Float;
        case WGPUTextureFormat_R32Uint:
            return TextureFormat::R32Uint;
        case WGPUTextureFormat_R32Sint:
            return TextureFormat::R32Sint;
        case WGPUTextureFormat_RG16Unorm:
            return TextureFormat::RG16Unorm;
        case WGPUTextureFormat_RG16Snorm:
            return TextureFormat::RG16Snorm;
        case WGPUTextureFormat_RG16Uint:
            return TextureFormat::RG16Uint;
        case WGPUTextureFormat_RG16Sint:
            return TextureFormat::RG16Sint;
        case WGPUTextureFormat_RG16Float:
            return TextureFormat::RG16Float;
        case WGPUTextureFormat_RGBA8Unorm:
            return TextureFormat::RGBA8Unorm;
        case WGPUTextureFormat_RGBA8UnormSrgb:
            return TextureFormat::RGBA8UnormSrgb;
        case WGPUTextureFormat_RGBA8Snorm:
            return TextureFormat::RGBA8Snorm;
        case WGPUTextureFormat_RGBA8Uint:
            return TextureFormat::RGBA8Uint;
        case WGPUTextureFormat_RGBA8Sint:
            return TextureFormat::RGBA8Sint;
        case WGPUTextureFormat_BGRA8Unorm:
            return TextureFormat::BGRA8Unorm;
        case WGPUTextureFormat_BGRA8UnormSrgb:
            return TextureFormat::BGRA8UnormSrgb;
        case WGPUTextureFormat_RGB10A2Uint:
            return TextureFormat::RGB10A2Uint;
        case WGPUTextureFormat_RGB10A2Unorm:
            return TextureFormat::RGB10A2Unorm;
        case WGPUTextureFormat_RG11B10Ufloat:
            return TextureFormat::RG11B10Ufloat;
        case WGPUTextureFormat_RGB9E5Ufloat:
            return TextureFormat::RGB9E5Ufloat;
        case WGPUTextureFormat_RG32Float:
            return TextureFormat::RG32Float;
        case WGPUTextureFormat_RG32Uint:
            return TextureFormat::RG32Uint;
        case WGPUTextureFormat_RG32Sint:
            return TextureFormat::RG32Sint;
        case WGPUTextureFormat_RGBA16Unorm:
            return TextureFormat::RGBA16Unorm;
        case WGPUTextureFormat_RGBA16Snorm:
            return TextureFormat::RGBA16Snorm;
        case WGPUTextureFormat_RGBA16Uint:
            return TextureFormat::RGBA16Uint;
        case WGPUTextureFormat_RGBA16Sint:
            return TextureFormat::RGBA16Sint;
        case WGPUTextureFormat_RGBA16Float:
            return TextureFormat::RGBA16Float;
        case WGPUTextureFormat_RGBA32Float:
            return TextureFormat::RGBA32Float;
        case WGPUTextureFormat_RGBA32Uint:
            return TextureFormat::RGBA32Uint;
        case WGPUTextureFormat_RGBA32Sint:
            return TextureFormat::RGBA32Sint;
        case WGPUTextureFormat_Stencil8:
            return TextureFormat::Stencil8;
        case WGPUTextureFormat_Depth16Unorm:
            return TextureFormat::Depth16Unorm;
        case WGPUTextureFormat_Depth24Plus:
            return TextureFormat::Depth24Plus;
        case WGPUTextureFormat_Depth24PlusStencil8:
            return TextureFormat::Depth24PlusStencil8;
        case WGPUTextureFormat_Depth32Float:
            return TextureFormat::Depth32Float;
        case WGPUTextureFormat_Depth32FloatStencil8:
            return TextureFormat::Depth32FloatStencil8;
        case WGPUTextureFormat_BC1RGBAUnorm:
            return TextureFormat::BC1RGBAUnorm;
        case WGPUTextureFormat_BC1RGBAUnormSrgb:
            return TextureFormat::BC1RGBAUnormSrgb;
        case WGPUTextureFormat_BC2RGBAUnorm:
            return TextureFormat::BC2RGBAUnorm;
        case WGPUTextureFormat_BC2RGBAUnormSrgb:
            return TextureFormat::BC2RGBAUnormSrgb;
        case WGPUTextureFormat_BC3RGBAUnorm:
            return TextureFormat::BC3RGBAUnorm;
        case WGPUTextureFormat_BC3RGBAUnormSrgb:
            return TextureFormat::BC3RGBAUnormSrgb;
        case WGPUTextureFormat_BC4RUnorm:
            return TextureFormat::BC4RUnorm;
        case WGPUTextureFormat_BC4RSnorm:
            return TextureFormat::BC4RSnorm;
        case WGPUTextureFormat_BC5RGUnorm:
            return TextureFormat::BC5RGUnorm;
        case WGPUTextureFormat_BC5RGSnorm:
            return TextureFormat::BC5RGSnorm;
        case WGPUTextureFormat_BC6HRGBUfloat:
            return TextureFormat::BC6HRGBUfloat;
        case WGPUTextureFormat_BC6HRGBFloat:
            return TextureFormat::BC6HRGBFloat;
        case WGPUTextureFormat_BC7RGBAUnorm:
            return TextureFormat::BC7RGBAUnorm;
        case WGPUTextureFormat_BC7RGBAUnormSrgb:
            return TextureFormat::BC7RGBAUnormSrgb;
        case WGPUTextureFormat_ETC2RGB8Unorm:
            return TextureFormat::ETC2RGB8Unorm;
        case WGPUTextureFormat_ETC2RGB8UnormSrgb:
            return TextureFormat::ETC2RGB8UnormSrgb;
        case WGPUTextureFormat_ETC2RGB8A1Unorm:
            return TextureFormat::ETC2RGB8A1Unorm;
        case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
            return TextureFormat::ETC2RGB8A1UnormSrgb;
        case WGPUTextureFormat_ETC2RGBA8Unorm:
            return TextureFormat::ETC2RGBA8Unorm;
        case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
            return TextureFormat::ETC2RGBA8UnormSrgb;
        case WGPUTextureFormat_EACR11Unorm:
            return TextureFormat::EACR11Unorm;
        case WGPUTextureFormat_EACR11Snorm:
            return TextureFormat::EACR11Snorm;
        case WGPUTextureFormat_EACRG11Unorm:
            return TextureFormat::EACRG11Unorm;
        case WGPUTextureFormat_EACRG11Snorm:
            return TextureFormat::EACRG11Snorm;
        case WGPUTextureFormat_ASTC4x4Unorm:
            return TextureFormat::ASTC4x4Unorm;
        case WGPUTextureFormat_ASTC4x4UnormSrgb:
            return TextureFormat::ASTC4x4UnormSrgb;
        case WGPUTextureFormat_ASTC5x4Unorm:
            return TextureFormat::ASTC5x4Unorm;
        case WGPUTextureFormat_ASTC5x4UnormSrgb:
            return TextureFormat::ASTC5x4UnormSrgb;
        case WGPUTextureFormat_ASTC5x5Unorm:
            return TextureFormat::ASTC5x5Unorm;
        case WGPUTextureFormat_ASTC5x5UnormSrgb:
            return TextureFormat::ASTC5x5UnormSrgb;
        case WGPUTextureFormat_ASTC6x5Unorm:
            return TextureFormat::ASTC6x5Unorm;
        case WGPUTextureFormat_ASTC6x5UnormSrgb:
            return TextureFormat::ASTC6x5UnormSrgb;
        case WGPUTextureFormat_ASTC6x6Unorm:
            return TextureFormat::ASTC6x6Unorm;
        case WGPUTextureFormat_ASTC6x6UnormSrgb:
            return TextureFormat::ASTC6x6UnormSrgb;
        case WGPUTextureFormat_ASTC8x5Unorm:
            return TextureFormat::ASTC8x5Unorm;
        case WGPUTextureFormat_ASTC8x5UnormSrgb:
            return TextureFormat::ASTC8x5UnormSrgb;
        case WGPUTextureFormat_ASTC8x6Unorm:
            return TextureFormat::ASTC8x6Unorm;
        case WGPUTextureFormat_ASTC8x6UnormSrgb:
            return TextureFormat::ASTC8x6UnormSrgb;
        case WGPUTextureFormat_ASTC8x8Unorm:
            return TextureFormat::ASTC8x8Unorm;
        case WGPUTextureFormat_ASTC8x8UnormSrgb:
            return TextureFormat::ASTC8x8UnormSrgb;
        case WGPUTextureFormat_ASTC10x5Unorm:
            return TextureFormat::ASTC10x5Unorm;
        case WGPUTextureFormat_ASTC10x5UnormSrgb:
            return TextureFormat::ASTC10x5UnormSrgb;
        case WGPUTextureFormat_ASTC10x6Unorm:
            return TextureFormat::ASTC10x6Unorm;
        case WGPUTextureFormat_ASTC10x6UnormSrgb:
            return TextureFormat::ASTC10x6UnormSrgb;
        case WGPUTextureFormat_ASTC10x8Unorm:
            return TextureFormat::ASTC10x8Unorm;
        case WGPUTextureFormat_ASTC10x8UnormSrgb:
            return TextureFormat::ASTC10x8UnormSrgb;
        case WGPUTextureFormat_ASTC10x10Unorm:
            return TextureFormat::ASTC10x10Unorm;
        case WGPUTextureFormat_ASTC10x10UnormSrgb:
            return TextureFormat::ASTC10x10UnormSrgb;
        case WGPUTextureFormat_ASTC12x10Unorm:
            return TextureFormat::ASTC12x10Unorm;
        case WGPUTextureFormat_ASTC12x10UnormSrgb:
            return TextureFormat::ASTC12x10UnormSrgb;
        case WGPUTextureFormat_ASTC12x12Unorm:
            return TextureFormat::ASTC12x12Unorm;
        case WGPUTextureFormat_ASTC12x12UnormSrgb:
            return TextureFormat::ASTC12x12UnormSrgb;
        case WGPUTextureFormat_R8BG8Biplanar420Unorm:
            return TextureFormat::R8BG8Biplanar420Unorm;
        case WGPUTextureFormat_R10X6BG10X6Biplanar420Unorm:
            return TextureFormat::R10X6BG10X6Biplanar420Unorm;
        case WGPUTextureFormat_R8BG8A8Triplanar420Unorm:
            return TextureFormat::R8BG8A8Triplanar420Unorm;
        case WGPUTextureFormat_R8BG8Biplanar422Unorm:
            return TextureFormat::R8BG8Biplanar422Unorm;
        case WGPUTextureFormat_R8BG8Biplanar444Unorm:
            return TextureFormat::R8BG8Biplanar444Unorm;
        case WGPUTextureFormat_R10X6BG10X6Biplanar422Unorm:
            return TextureFormat::R10X6BG10X6Biplanar422Unorm;
        case WGPUTextureFormat_R10X6BG10X6Biplanar444Unorm:
            return TextureFormat::R10X6BG10X6Biplanar444Unorm;
        case WGPUTextureFormat_OpaqueYCbCrAndroid:
            return TextureFormat::OpaqueYCbCrAndroid;
        default:
            return TextureFormat::Undefined;
    }
}

[[nodiscard]] inline WGPUTextureSampleType ToWgpu(TextureSampleType value) noexcept {
    switch (value) {
        case TextureSampleType::BindingNotUsed:
            return WGPUTextureSampleType_BindingNotUsed;
        case TextureSampleType::Undefined:
            return WGPUTextureSampleType_Undefined;
        case TextureSampleType::Float:
            return WGPUTextureSampleType_Float;
        case TextureSampleType::UnfilterableFloat:
            return WGPUTextureSampleType_UnfilterableFloat;
        case TextureSampleType::Depth:
            return WGPUTextureSampleType_Depth;
        case TextureSampleType::Sint:
            return WGPUTextureSampleType_Sint;
        case TextureSampleType::Uint:
            return WGPUTextureSampleType_Uint;
    }
    return WGPUTextureSampleType_Undefined;
}

[[nodiscard]] inline TextureSampleType FromWgpu(WGPUTextureSampleType value) noexcept {
    switch (value) {
        case WGPUTextureSampleType_BindingNotUsed:
            return TextureSampleType::BindingNotUsed;
        case WGPUTextureSampleType_Undefined:
            return TextureSampleType::Undefined;
        case WGPUTextureSampleType_Float:
            return TextureSampleType::Float;
        case WGPUTextureSampleType_UnfilterableFloat:
            return TextureSampleType::UnfilterableFloat;
        case WGPUTextureSampleType_Depth:
            return TextureSampleType::Depth;
        case WGPUTextureSampleType_Sint:
            return TextureSampleType::Sint;
        case WGPUTextureSampleType_Uint:
            return TextureSampleType::Uint;
        default:
            return TextureSampleType::Undefined;
    }
}

[[nodiscard]] inline WGPUTextureViewDimension ToWgpu(TextureViewDimension value) noexcept {
    switch (value) {
        case TextureViewDimension::Undefined:
            return WGPUTextureViewDimension_Undefined;
        case TextureViewDimension::e1D:
            return WGPUTextureViewDimension_1D;
        case TextureViewDimension::e2D:
            return WGPUTextureViewDimension_2D;
        case TextureViewDimension::e2DArray:
            return WGPUTextureViewDimension_2DArray;
        case TextureViewDimension::Cube:
            return WGPUTextureViewDimension_Cube;
        case TextureViewDimension::CubeArray:
            return WGPUTextureViewDimension_CubeArray;
        case TextureViewDimension::e3D:
            return WGPUTextureViewDimension_3D;
    }
    return WGPUTextureViewDimension_Undefined;
}

[[nodiscard]] inline TextureViewDimension FromWgpu(WGPUTextureViewDimension value) noexcept {
    switch (value) {
        case WGPUTextureViewDimension_Undefined:
            return TextureViewDimension::Undefined;
        case WGPUTextureViewDimension_1D:
            return TextureViewDimension::e1D;
        case WGPUTextureViewDimension_2D:
            return TextureViewDimension::e2D;
        case WGPUTextureViewDimension_2DArray:
            return TextureViewDimension::e2DArray;
        case WGPUTextureViewDimension_Cube:
            return TextureViewDimension::Cube;
        case WGPUTextureViewDimension_CubeArray:
            return TextureViewDimension::CubeArray;
        case WGPUTextureViewDimension_3D:
            return TextureViewDimension::e3D;
        default:
            return TextureViewDimension::Undefined;
    }
}

[[nodiscard]] inline WGPUToneMappingMode ToWgpu(ToneMappingMode value) noexcept {
    switch (value) {
        case ToneMappingMode::Standard:
            return WGPUToneMappingMode_Standard;
        case ToneMappingMode::Extended:
            return WGPUToneMappingMode_Extended;
    }
    return WGPUToneMappingMode_Standard;
}

[[nodiscard]] inline ToneMappingMode FromWgpu(WGPUToneMappingMode value) noexcept {
    switch (value) {
        case WGPUToneMappingMode_Standard:
            return ToneMappingMode::Standard;
        case WGPUToneMappingMode_Extended:
            return ToneMappingMode::Extended;
        default:
            return ToneMappingMode::Standard;
    }
}

[[nodiscard]] inline WGPUVertexFormat ToWgpu(VertexFormat value) noexcept {
    switch (value) {
        case VertexFormat::Uint8:
            return WGPUVertexFormat_Uint8;
        case VertexFormat::Uint8x2:
            return WGPUVertexFormat_Uint8x2;
        case VertexFormat::Uint8x4:
            return WGPUVertexFormat_Uint8x4;
        case VertexFormat::Sint8:
            return WGPUVertexFormat_Sint8;
        case VertexFormat::Sint8x2:
            return WGPUVertexFormat_Sint8x2;
        case VertexFormat::Sint8x4:
            return WGPUVertexFormat_Sint8x4;
        case VertexFormat::Unorm8:
            return WGPUVertexFormat_Unorm8;
        case VertexFormat::Unorm8x2:
            return WGPUVertexFormat_Unorm8x2;
        case VertexFormat::Unorm8x4:
            return WGPUVertexFormat_Unorm8x4;
        case VertexFormat::Snorm8:
            return WGPUVertexFormat_Snorm8;
        case VertexFormat::Snorm8x2:
            return WGPUVertexFormat_Snorm8x2;
        case VertexFormat::Snorm8x4:
            return WGPUVertexFormat_Snorm8x4;
        case VertexFormat::Uint16:
            return WGPUVertexFormat_Uint16;
        case VertexFormat::Uint16x2:
            return WGPUVertexFormat_Uint16x2;
        case VertexFormat::Uint16x4:
            return WGPUVertexFormat_Uint16x4;
        case VertexFormat::Sint16:
            return WGPUVertexFormat_Sint16;
        case VertexFormat::Sint16x2:
            return WGPUVertexFormat_Sint16x2;
        case VertexFormat::Sint16x4:
            return WGPUVertexFormat_Sint16x4;
        case VertexFormat::Unorm16:
            return WGPUVertexFormat_Unorm16;
        case VertexFormat::Unorm16x2:
            return WGPUVertexFormat_Unorm16x2;
        case VertexFormat::Unorm16x4:
            return WGPUVertexFormat_Unorm16x4;
        case VertexFormat::Snorm16:
            return WGPUVertexFormat_Snorm16;
        case VertexFormat::Snorm16x2:
            return WGPUVertexFormat_Snorm16x2;
        case VertexFormat::Snorm16x4:
            return WGPUVertexFormat_Snorm16x4;
        case VertexFormat::Float16:
            return WGPUVertexFormat_Float16;
        case VertexFormat::Float16x2:
            return WGPUVertexFormat_Float16x2;
        case VertexFormat::Float16x4:
            return WGPUVertexFormat_Float16x4;
        case VertexFormat::Float32:
            return WGPUVertexFormat_Float32;
        case VertexFormat::Float32x2:
            return WGPUVertexFormat_Float32x2;
        case VertexFormat::Float32x3:
            return WGPUVertexFormat_Float32x3;
        case VertexFormat::Float32x4:
            return WGPUVertexFormat_Float32x4;
        case VertexFormat::Uint32:
            return WGPUVertexFormat_Uint32;
        case VertexFormat::Uint32x2:
            return WGPUVertexFormat_Uint32x2;
        case VertexFormat::Uint32x3:
            return WGPUVertexFormat_Uint32x3;
        case VertexFormat::Uint32x4:
            return WGPUVertexFormat_Uint32x4;
        case VertexFormat::Sint32:
            return WGPUVertexFormat_Sint32;
        case VertexFormat::Sint32x2:
            return WGPUVertexFormat_Sint32x2;
        case VertexFormat::Sint32x3:
            return WGPUVertexFormat_Sint32x3;
        case VertexFormat::Sint32x4:
            return WGPUVertexFormat_Sint32x4;
        case VertexFormat::Unorm10_10_10_2:
            return WGPUVertexFormat_Unorm10_10_10_2;
        case VertexFormat::Unorm8x4BGRA:
            return WGPUVertexFormat_Unorm8x4BGRA;
    }
    return WGPUVertexFormat_Uint8;
}

[[nodiscard]] inline VertexFormat FromWgpu(WGPUVertexFormat value) noexcept {
    switch (value) {
        case WGPUVertexFormat_Uint8:
            return VertexFormat::Uint8;
        case WGPUVertexFormat_Uint8x2:
            return VertexFormat::Uint8x2;
        case WGPUVertexFormat_Uint8x4:
            return VertexFormat::Uint8x4;
        case WGPUVertexFormat_Sint8:
            return VertexFormat::Sint8;
        case WGPUVertexFormat_Sint8x2:
            return VertexFormat::Sint8x2;
        case WGPUVertexFormat_Sint8x4:
            return VertexFormat::Sint8x4;
        case WGPUVertexFormat_Unorm8:
            return VertexFormat::Unorm8;
        case WGPUVertexFormat_Unorm8x2:
            return VertexFormat::Unorm8x2;
        case WGPUVertexFormat_Unorm8x4:
            return VertexFormat::Unorm8x4;
        case WGPUVertexFormat_Snorm8:
            return VertexFormat::Snorm8;
        case WGPUVertexFormat_Snorm8x2:
            return VertexFormat::Snorm8x2;
        case WGPUVertexFormat_Snorm8x4:
            return VertexFormat::Snorm8x4;
        case WGPUVertexFormat_Uint16:
            return VertexFormat::Uint16;
        case WGPUVertexFormat_Uint16x2:
            return VertexFormat::Uint16x2;
        case WGPUVertexFormat_Uint16x4:
            return VertexFormat::Uint16x4;
        case WGPUVertexFormat_Sint16:
            return VertexFormat::Sint16;
        case WGPUVertexFormat_Sint16x2:
            return VertexFormat::Sint16x2;
        case WGPUVertexFormat_Sint16x4:
            return VertexFormat::Sint16x4;
        case WGPUVertexFormat_Unorm16:
            return VertexFormat::Unorm16;
        case WGPUVertexFormat_Unorm16x2:
            return VertexFormat::Unorm16x2;
        case WGPUVertexFormat_Unorm16x4:
            return VertexFormat::Unorm16x4;
        case WGPUVertexFormat_Snorm16:
            return VertexFormat::Snorm16;
        case WGPUVertexFormat_Snorm16x2:
            return VertexFormat::Snorm16x2;
        case WGPUVertexFormat_Snorm16x4:
            return VertexFormat::Snorm16x4;
        case WGPUVertexFormat_Float16:
            return VertexFormat::Float16;
        case WGPUVertexFormat_Float16x2:
            return VertexFormat::Float16x2;
        case WGPUVertexFormat_Float16x4:
            return VertexFormat::Float16x4;
        case WGPUVertexFormat_Float32:
            return VertexFormat::Float32;
        case WGPUVertexFormat_Float32x2:
            return VertexFormat::Float32x2;
        case WGPUVertexFormat_Float32x3:
            return VertexFormat::Float32x3;
        case WGPUVertexFormat_Float32x4:
            return VertexFormat::Float32x4;
        case WGPUVertexFormat_Uint32:
            return VertexFormat::Uint32;
        case WGPUVertexFormat_Uint32x2:
            return VertexFormat::Uint32x2;
        case WGPUVertexFormat_Uint32x3:
            return VertexFormat::Uint32x3;
        case WGPUVertexFormat_Uint32x4:
            return VertexFormat::Uint32x4;
        case WGPUVertexFormat_Sint32:
            return VertexFormat::Sint32;
        case WGPUVertexFormat_Sint32x2:
            return VertexFormat::Sint32x2;
        case WGPUVertexFormat_Sint32x3:
            return VertexFormat::Sint32x3;
        case WGPUVertexFormat_Sint32x4:
            return VertexFormat::Sint32x4;
        case WGPUVertexFormat_Unorm10_10_10_2:
            return VertexFormat::Unorm10_10_10_2;
        case WGPUVertexFormat_Unorm8x4BGRA:
            return VertexFormat::Unorm8x4BGRA;
        default:
            return VertexFormat::Uint8;
    }
}

[[nodiscard]] inline WGPUVertexStepMode ToWgpu(VertexStepMode value) noexcept {
    switch (value) {
        case VertexStepMode::Undefined:
            return WGPUVertexStepMode_Undefined;
        case VertexStepMode::Vertex:
            return WGPUVertexStepMode_Vertex;
        case VertexStepMode::Instance:
            return WGPUVertexStepMode_Instance;
    }
    return WGPUVertexStepMode_Undefined;
}

[[nodiscard]] inline VertexStepMode FromWgpu(WGPUVertexStepMode value) noexcept {
    switch (value) {
        case WGPUVertexStepMode_Undefined:
            return VertexStepMode::Undefined;
        case WGPUVertexStepMode_Vertex:
            return VertexStepMode::Vertex;
        case WGPUVertexStepMode_Instance:
            return VertexStepMode::Instance;
        default:
            return VertexStepMode::Undefined;
    }
}

[[nodiscard]] inline WGPUWaitStatus ToWgpu(WaitStatus value) noexcept {
    switch (value) {
        case WaitStatus::Success:
            return WGPUWaitStatus_Success;
        case WaitStatus::TimedOut:
            return WGPUWaitStatus_TimedOut;
        case WaitStatus::Error:
            return WGPUWaitStatus_Error;
    }
    return WGPUWaitStatus_Error;
}

[[nodiscard]] inline WaitStatus FromWgpu(WGPUWaitStatus value) noexcept {
    switch (value) {
        case WGPUWaitStatus_Success:
            return WaitStatus::Success;
        case WGPUWaitStatus_TimedOut:
            return WaitStatus::TimedOut;
        case WGPUWaitStatus_Error:
            return WaitStatus::Error;
        default:
            return WaitStatus::Error;
    }
}

[[nodiscard]] inline WGPUWGSLLanguageFeatureName ToWgpu(WGSLLanguageFeatureName value) noexcept {
    switch (value) {
        case WGSLLanguageFeatureName::ReadonlyAndReadwriteStorageTextures:
            return WGPUWGSLLanguageFeatureName_ReadonlyAndReadwriteStorageTextures;
        case WGSLLanguageFeatureName::Packed4x8IntegerDotProduct:
            return WGPUWGSLLanguageFeatureName_Packed4x8IntegerDotProduct;
        case WGSLLanguageFeatureName::UnrestrictedPointerParameters:
            return WGPUWGSLLanguageFeatureName_UnrestrictedPointerParameters;
        case WGSLLanguageFeatureName::PointerCompositeAccess:
            return WGPUWGSLLanguageFeatureName_PointerCompositeAccess;
        case WGSLLanguageFeatureName::UniformBufferStandardLayout:
            return WGPUWGSLLanguageFeatureName_UniformBufferStandardLayout;
        case WGSLLanguageFeatureName::SubgroupId:
            return WGPUWGSLLanguageFeatureName_SubgroupId;
        case WGSLLanguageFeatureName::TextureAndSamplerLet:
            return WGPUWGSLLanguageFeatureName_TextureAndSamplerLet;
        case WGSLLanguageFeatureName::SubgroupUniformity:
            return WGPUWGSLLanguageFeatureName_SubgroupUniformity;
        case WGSLLanguageFeatureName::TextureFormatsTier1:
            return WGPUWGSLLanguageFeatureName_TextureFormatsTier1;
        case WGSLLanguageFeatureName::LinearIndexing:
            return WGPUWGSLLanguageFeatureName_LinearIndexing;
        case WGSLLanguageFeatureName::ImmediateAddressSpace:
            return WGPUWGSLLanguageFeatureName_ImmediateAddressSpace;
        case WGSLLanguageFeatureName::ChromiumTestingUnimplemented:
            return WGPUWGSLLanguageFeatureName_ChromiumTestingUnimplemented;
        case WGSLLanguageFeatureName::ChromiumTestingUnsafeExperimental:
            return WGPUWGSLLanguageFeatureName_ChromiumTestingUnsafeExperimental;
        case WGSLLanguageFeatureName::ChromiumTestingExperimental:
            return WGPUWGSLLanguageFeatureName_ChromiumTestingExperimental;
        case WGSLLanguageFeatureName::ChromiumTestingShippedWithKillswitch:
            return WGPUWGSLLanguageFeatureName_ChromiumTestingShippedWithKillswitch;
        case WGSLLanguageFeatureName::ChromiumTestingShipped:
            return WGPUWGSLLanguageFeatureName_ChromiumTestingShipped;
        case WGSLLanguageFeatureName::SizedBindingArray:
            return WGPUWGSLLanguageFeatureName_SizedBindingArray;
        case WGSLLanguageFeatureName::TexelBuffers:
            return WGPUWGSLLanguageFeatureName_TexelBuffers;
        case WGSLLanguageFeatureName::ChromiumPrint:
            return WGPUWGSLLanguageFeatureName_ChromiumPrint;
        case WGSLLanguageFeatureName::FragmentDepth:
            return WGPUWGSLLanguageFeatureName_FragmentDepth;
        case WGSLLanguageFeatureName::BufferView:
            return WGPUWGSLLanguageFeatureName_BufferView;
        case WGSLLanguageFeatureName::SwizzleAssignment:
            return WGPUWGSLLanguageFeatureName_SwizzleAssignment;
    }
    return WGPUWGSLLanguageFeatureName_ReadonlyAndReadwriteStorageTextures;
}

[[nodiscard]] inline WGSLLanguageFeatureName FromWgpu(WGPUWGSLLanguageFeatureName value) noexcept {
    switch (value) {
        case WGPUWGSLLanguageFeatureName_ReadonlyAndReadwriteStorageTextures:
            return WGSLLanguageFeatureName::ReadonlyAndReadwriteStorageTextures;
        case WGPUWGSLLanguageFeatureName_Packed4x8IntegerDotProduct:
            return WGSLLanguageFeatureName::Packed4x8IntegerDotProduct;
        case WGPUWGSLLanguageFeatureName_UnrestrictedPointerParameters:
            return WGSLLanguageFeatureName::UnrestrictedPointerParameters;
        case WGPUWGSLLanguageFeatureName_PointerCompositeAccess:
            return WGSLLanguageFeatureName::PointerCompositeAccess;
        case WGPUWGSLLanguageFeatureName_UniformBufferStandardLayout:
            return WGSLLanguageFeatureName::UniformBufferStandardLayout;
        case WGPUWGSLLanguageFeatureName_SubgroupId:
            return WGSLLanguageFeatureName::SubgroupId;
        case WGPUWGSLLanguageFeatureName_TextureAndSamplerLet:
            return WGSLLanguageFeatureName::TextureAndSamplerLet;
        case WGPUWGSLLanguageFeatureName_SubgroupUniformity:
            return WGSLLanguageFeatureName::SubgroupUniformity;
        case WGPUWGSLLanguageFeatureName_TextureFormatsTier1:
            return WGSLLanguageFeatureName::TextureFormatsTier1;
        case WGPUWGSLLanguageFeatureName_LinearIndexing:
            return WGSLLanguageFeatureName::LinearIndexing;
        case WGPUWGSLLanguageFeatureName_ImmediateAddressSpace:
            return WGSLLanguageFeatureName::ImmediateAddressSpace;
        case WGPUWGSLLanguageFeatureName_ChromiumTestingUnimplemented:
            return WGSLLanguageFeatureName::ChromiumTestingUnimplemented;
        case WGPUWGSLLanguageFeatureName_ChromiumTestingUnsafeExperimental:
            return WGSLLanguageFeatureName::ChromiumTestingUnsafeExperimental;
        case WGPUWGSLLanguageFeatureName_ChromiumTestingExperimental:
            return WGSLLanguageFeatureName::ChromiumTestingExperimental;
        case WGPUWGSLLanguageFeatureName_ChromiumTestingShippedWithKillswitch:
            return WGSLLanguageFeatureName::ChromiumTestingShippedWithKillswitch;
        case WGPUWGSLLanguageFeatureName_ChromiumTestingShipped:
            return WGSLLanguageFeatureName::ChromiumTestingShipped;
        case WGPUWGSLLanguageFeatureName_SizedBindingArray:
            return WGSLLanguageFeatureName::SizedBindingArray;
        case WGPUWGSLLanguageFeatureName_TexelBuffers:
            return WGSLLanguageFeatureName::TexelBuffers;
        case WGPUWGSLLanguageFeatureName_ChromiumPrint:
            return WGSLLanguageFeatureName::ChromiumPrint;
        case WGPUWGSLLanguageFeatureName_FragmentDepth:
            return WGSLLanguageFeatureName::FragmentDepth;
        case WGPUWGSLLanguageFeatureName_BufferView:
            return WGSLLanguageFeatureName::BufferView;
        case WGPUWGSLLanguageFeatureName_SwizzleAssignment:
            return WGSLLanguageFeatureName::SwizzleAssignment;
        default:
            return WGSLLanguageFeatureName::ReadonlyAndReadwriteStorageTextures;
    }
}

[[nodiscard]] inline WGPUBufferUsage ToWgpuBufferUsage(BufferUsage value) noexcept {
    return static_cast<WGPUBufferUsage>(static_cast<u64>(value));
}

[[nodiscard]] inline BufferUsage FromWgpuBufferUsage(WGPUBufferUsage value) noexcept {
    return static_cast<BufferUsage>(static_cast<u64>(value));
}

[[nodiscard]] inline WGPUColorWriteMask ToWgpuColorWriteMask(ColorWriteMask value) noexcept {
    return static_cast<WGPUColorWriteMask>(static_cast<u64>(value));
}

[[nodiscard]] inline ColorWriteMask FromWgpuColorWriteMask(WGPUColorWriteMask value) noexcept {
    return static_cast<ColorWriteMask>(static_cast<u64>(value));
}

[[nodiscard]] inline WGPUHeapProperty ToWgpuHeapProperty(HeapProperty value) noexcept {
    return static_cast<WGPUHeapProperty>(static_cast<u64>(value));
}

[[nodiscard]] inline HeapProperty FromWgpuHeapProperty(WGPUHeapProperty value) noexcept {
    return static_cast<HeapProperty>(static_cast<u64>(value));
}

[[nodiscard]] inline WGPUMapMode ToWgpuMapMode(MapMode value) noexcept {
    return static_cast<WGPUMapMode>(static_cast<u64>(value));
}

[[nodiscard]] inline MapMode FromWgpuMapMode(WGPUMapMode value) noexcept {
    return static_cast<MapMode>(static_cast<u64>(value));
}

[[nodiscard]] inline WGPUShaderStage ToWgpuShaderStage(ShaderStage value) noexcept {
    return static_cast<WGPUShaderStage>(static_cast<u64>(value));
}

[[nodiscard]] inline ShaderStage FromWgpuShaderStage(WGPUShaderStage value) noexcept {
    return static_cast<ShaderStage>(static_cast<u64>(value));
}

[[nodiscard]] inline WGPUTextureUsage ToWgpuTextureUsage(TextureUsage value) noexcept {
    return static_cast<WGPUTextureUsage>(static_cast<u64>(value));
}

[[nodiscard]] inline TextureUsage FromWgpuTextureUsage(WGPUTextureUsage value) noexcept {
    return static_cast<TextureUsage>(static_cast<u64>(value));
}

} // namespace woki::rhi::wgpu::convert
