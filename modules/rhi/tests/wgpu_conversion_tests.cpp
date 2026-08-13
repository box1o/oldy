#include <catch2/catch_test_macros.hpp>

#include "wgpu_enums.hpp"
#include "detail/device_descriptor.hpp"
#include "detail/pipeline_descriptor.hpp"
#include "detail/resource_descriptor.hpp"

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

TEST_CASE("WebGPU pipeline stage storage owns specialization constant keys") {
    std::vector<ConstantEntryDesc> constants{{.key = "mode", .value = 2.0}, {.key = "scale", .value = 0.5}};
    VertexStateDesc desc{.constants = constants};
    woki::rhi::wgpu::detail::VertexStateStorage storage(desc);

    constants[0].key = "changed";
    REQUIRE(storage.native.constantCount == 2);
    CHECK(woki::rhi::wgpu::detail::StringFromView(storage.native.constants[0].key) == "mode");
    CHECK(storage.native.constants[0].value == 2.0);

    ComputeStateDesc compute_desc{.constants = constants};
    woki::rhi::wgpu::detail::ComputeStateStorage compute(compute_desc);
    constants[1].key = "also changed";
    CHECK(woki::rhi::wgpu::detail::StringFromView(compute.native.constants[1].key) == "scale");
}

TEST_CASE("WebGPU resource descriptor storage owns copied strings and arrays") {
    ShaderModuleDesc shader_desc{.code = "@compute @workgroup_size(1) fn main() {}", .label = "owned shader"};
    woki::rhi::wgpu::detail::ShaderModuleDescriptorStorage shader(shader_desc);
    shader_desc.code = "changed";
    CHECK(woki::rhi::wgpu::detail::StringFromView(shader.wgsl_source.code) == "@compute @workgroup_size(1) fn main() {}");

    TextureDesc texture_desc{
        .size = {1, 1, 1},
        .format = TextureFormat::RGBA8Unorm,
        .usage = TextureUsage::TextureBinding,
        .view_formats = {TextureFormat::RGBA8UnormSrgb, TextureFormat::BGRA8Unorm},
    };
    woki::rhi::wgpu::detail::TextureDescriptorStorage texture(texture_desc);
    texture_desc.view_formats.clear();
    REQUIRE(texture.native.viewFormatCount == 2);
    REQUIRE(texture.native.viewFormats == texture.view_formats.data());
    CHECK(texture.native.viewFormats[0] == WGPUTextureFormat_RGBA8UnormSrgb);
    CHECK(texture.native.viewFormats[1] == WGPUTextureFormat_BGRA8Unorm);
}

TEST_CASE("WebGPU vertex storage owns nested attribute arrays") {
    std::vector<VertexAttributeDesc> attributes{
        {.format = VertexFormat::Float32x3, .offset = 0, .shader_location = 0},
        {.format = VertexFormat::Float32x2, .offset = 12, .shader_location = 1},
    };
    const VertexBufferLayoutDesc buffer{.array_stride = 20, .attributes = attributes};
    const VertexStateDesc desc{.entry_point = "vs", .buffers = std::span(&buffer, 1)};
    woki::rhi::wgpu::detail::VertexStateStorage storage(desc);
    attributes.clear();

    REQUIRE(storage.native.bufferCount == 1);
    REQUIRE(storage.native.buffers == storage.buffers.data());
    REQUIRE(storage.native.buffers[0].attributeCount == 2);
    CHECK(storage.native.buffers[0].attributes[0].shaderLocation == 0);
    CHECK(storage.native.buffers[0].attributes[1].offset == 12);
}

TEST_CASE("Typed multisample state converts to WebGPU") {
    const MultisampleStateDesc desc{.count = 4, .mask = 0x0f0f0f0f, .alpha_to_coverage_enabled = true};
    const auto native = woki::rhi::wgpu::detail::ToWgpu(desc);

    CHECK(native.count == 4);
    CHECK(native.mask == 0x0f0f0f0f);
    CHECK(native.alphaToCoverageEnabled == WGPU_TRUE);
}

TEST_CASE("WebGPU feature conversion maps D3D12 file mapping handles") {
#ifdef WOKI_WGPU_HAS_D3D12_FILE_MAPPING_FEATURE
    REQUIRE(woki::rhi::wgpu::convert::ToWgpu(FeatureName::SharedBufferMemoryD3D12SharedMemoryFileMappingHandle) == WGPUFeatureName_SharedBufferMemoryD3D12SharedMemoryFileMappingHandle);
    REQUIRE(woki::rhi::wgpu::convert::FromWgpu(WGPUFeatureName_SharedBufferMemoryD3D12SharedMemoryFileMappingHandle) == FeatureName::SharedBufferMemoryD3D12SharedMemoryFileMappingHandle);
#else
    REQUIRE(woki::rhi::wgpu::convert::ToWgpu(FeatureName::SharedBufferMemoryD3D12SharedMemoryFileMappingHandle) == WGPUFeatureName_Force32);
#endif
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
