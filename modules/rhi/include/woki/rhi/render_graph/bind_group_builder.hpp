#pragma once

#include <string>
#include <vector>
#include <string_view>

#include <woki/core.hpp>
#include <woki/rhi/forward.hpp>
#include <woki/rhi/descriptors.hpp>

namespace woki::rhi {

class BindGroupBuilder final {
public:
    BindGroupBuilder(ref<Device> device, ref<BindGroupLayout> layout, std::string_view label = "RenderGraphBindGroup");

    BindGroupBuilder& BindTexture(u32 binding, TextureView& view);
    BindGroupBuilder& BindSampler(u32 binding, Sampler& sampler);
    BindGroupBuilder& BindBuffer(u32 binding, Buffer& buffer, u64 offset = 0, u64 size = kWholeSize);

    [[nodiscard]] Result<scope<BindGroup>> Build();

private:
    ref<Device> device_{};
    ref<BindGroupLayout> layout_{};
    std::string label_{};
    std::vector<BindGroupEntryDesc> entries_{};
};

} // namespace woki::rhi
