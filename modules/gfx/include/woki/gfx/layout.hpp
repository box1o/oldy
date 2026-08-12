#pragma once

#include <map>

#include <woki/rhi.hpp>

#include "reflection.hpp"

namespace woki::gfx {

struct BindGroupLayoutKey {
    u32 group{0};
    std::vector<BindingInfo> bindings;
    ContentHash hash;
    [[nodiscard]] friend bool operator==(const BindGroupLayoutKey&, const BindGroupLayoutKey&) = default;
};

struct PipelineLayoutKey {
    std::vector<BindGroupLayoutKey> groups;
    ContentHash hash;
    [[nodiscard]] friend bool operator==(const PipelineLayoutKey&, const PipelineLayoutKey&) = default;
};

[[nodiscard]] Result<PipelineLayoutKey> MakePipelineLayoutKey(const ShaderInterface& interface);

class LayoutCache {
public:
    explicit LayoutCache(rhi::Device& device)
        : device_(device) {}

    [[nodiscard]] Result<rhi::PipelineLayout*> GetOrCreate(const PipelineLayoutKey& key);

private:
    struct Entry {
        PipelineLayoutKey key;
        std::vector<scope<rhi::BindGroupLayout>> groups;
        scope<rhi::PipelineLayout> pipeline;
    };

    rhi::Device& device_;
    std::vector<Entry> entries_;
};

} // namespace woki::gfx
