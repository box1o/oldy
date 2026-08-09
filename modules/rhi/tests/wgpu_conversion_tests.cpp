#include <catch2/catch_test_macros.hpp>

#include "wgpu_enums.hpp"
#include "detail/device_descriptor.hpp"

using namespace woki::rhi;

TEST_CASE("WebGPU flag conversions preserve combined masks") {
    using namespace woki::rhi::wgpu::convert;

    const auto buffer_flags = WGPUBufferUsage_CopySrc | WGPUBufferUsage_Vertex | WGPUBufferUsage_Storage;
    REQUIRE(FromWgpuBufferUsage(buffer_flags) == (BufferUsage::CopySrc | BufferUsage::Vertex | BufferUsage::Storage));

    const auto texture_flags = WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_RenderAttachment;
    REQUIRE(FromWgpuTextureUsage(texture_flags) == (TextureUsage::CopyDst | TextureUsage::TextureBinding | TextureUsage::RenderAttachment));

    const auto shader_stages = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    REQUIRE(FromWgpuShaderStage(shader_stages) == (ShaderStage::Vertex | ShaderStage::Fragment));

    const auto color_mask = static_cast<ColorWriteMask>(static_cast<woki::u64>(ColorWriteMask::Red) | static_cast<woki::u64>(ColorWriteMask::Blue));
    REQUIRE(ToWgpuColorWriteMask(color_mask) == (WGPUColorWriteMask_Red | WGPUColorWriteMask_Blue));
}

TEST_CASE("WebGPU device descriptor storage owns its native pointer targets") {
    DeviceDesc desc{};
    desc.required_features = {FeatureName::TimestampQuery, FeatureName::ShaderF16};
    desc.required_limits.emplace();

    woki::rhi::wgpu::detail::DeviceDescriptorStorage storage(desc);

    REQUIRE(storage.native_desc.requiredFeatures == storage.required_features.data());
    REQUIRE(storage.native_desc.requiredLimits == &storage.required_limits);
    REQUIRE(storage.native_desc.requiredFeatureCount == 2);
}

TEST_CASE("WebGPU feature conversion maps D3D12 file mapping handles") {
    REQUIRE(woki::rhi::wgpu::convert::ToWgpu(FeatureName::SharedBufferMemoryD3D12SharedMemoryFileMappingHandle) == WGPUFeatureName_SharedBufferMemoryD3D12SharedMemoryFileMappingHandle);
    REQUIRE(woki::rhi::wgpu::convert::FromWgpu(WGPUFeatureName_SharedBufferMemoryD3D12SharedMemoryFileMappingHandle) == FeatureName::SharedBufferMemoryD3D12SharedMemoryFileMappingHandle);
}

TEST_CASE("Unknown WebGPU completion statuses do not report success") {
    using namespace woki::rhi::wgpu::convert;

    REQUIRE(FromWgpu(WGPUStatus_Success) == Status::Success);
    REQUIRE(FromWgpu(WGPUWaitStatus_Success) == WaitStatus::Success);
    REQUIRE(FromWgpu(WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal) == SurfaceGetCurrentTextureStatus::SuccessOptimal);
    REQUIRE(FromWgpu(WGPUStatus_Force32) == Status::Error);
    REQUIRE(FromWgpu(WGPUWaitStatus_Force32) == WaitStatus::Error);
    REQUIRE(FromWgpu(WGPUSurfaceGetCurrentTextureStatus_Force32) == SurfaceGetCurrentTextureStatus::Error);
}
