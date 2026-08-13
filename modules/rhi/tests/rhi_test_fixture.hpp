#pragma once

#include <algorithm>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include <woki/rhi.hpp>

namespace woki::rhi::test {

struct NullContext final {
    scope<Instance> instance;
    scope<Adapter> adapter;
    ref<Device> device;

    explicit NullContext(NullRhiDescriptor null_desc = {}, DeviceDesc device_desc = {}) {
        auto instance_result = CreateNullInstance(std::move(null_desc));
        REQUIRE(instance_result);
        instance = std::move(*instance_result);

        auto adapter_result = instance->RequestAdapter();
        REQUIRE(adapter_result);
        adapter = std::move(*adapter_result);

        auto device_result = adapter->CreateDevice(device_desc);
        REQUIRE(device_result);
        device = std::move(*device_result);
    }
};

inline scope<ShaderModule> CreateShader(Device& device) {
    auto result = device.CreateShaderModule({.code = R"(
        override scale: f32 = 1.0;
        @vertex fn vs(@builtin(vertex_index) i: u32) -> @builtin(position) vec4f {
            var p = array<vec2f, 3>(vec2f(-1, -1), vec2f(3, -1), vec2f(-1, 3));
            return vec4f(p[i] * scale, 0, 1);
        }
        @fragment fn fs() -> @location(0) vec4f { return vec4f(0, 0, 0, 1); }
        @compute @workgroup_size(1) fn cs() {}
    )"});
    REQUIRE(result);
    return std::move(*result);
}

inline scope<RenderPipeline> CreateRenderPipeline(Device& device, ShaderModule& shader, u32 sample_count = 1) {
    const ConstantEntryDesc constant{.key = "scale", .value = 0.5};
    const VertexStateDesc vertex{.module = &shader, .entry_point = "vs", .constants = std::span(&constant, 1)};
    const ColorTargetStateDesc target{.format = TextureFormat::RGBA8Unorm};
    const FragmentStateDesc fragment{.module = &shader, .entry_point = "fs", .targets = std::span(&target, 1)};
    auto result = device.CreateRenderPipeline({
        .vertex = &vertex,
        .multisample = {.count = sample_count, .mask = 0x55aa55aa, .alpha_to_coverage_enabled = sample_count > 1},
        .fragment = &fragment,
    });
    REQUIRE(result);
    return std::move(*result);
}

inline scope<ComputePipeline> CreateComputePipeline(Device& device, ShaderModule& shader, PipelineLayout* layout = nullptr) {
    const ConstantEntryDesc constant{.key = "mode", .value = 2.0};
    auto result = device.CreateComputePipeline({
        .layout = layout,
        .compute = {.module = &shader, .entry_point = "cs", .constants = std::span(&constant, 1)},
    });
    REQUIRE(result);
    return std::move(*result);
}

inline bool LogContains(const Device& device, std::string_view entry) {
    const auto log = NullCommandLog(device);
    return std::ranges::find(log, entry) != log.end();
}

} // namespace woki::rhi::test
