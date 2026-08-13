#pragma once

#include <woki/rhi.hpp>

#include "reflection.hpp"
#include "deferred_release.hpp"

namespace woki::gfx {

struct BindGroupLayoutKey {
    u32 group{0};
    std::vector<BindingInfo> bindings;
    std::vector<u32> dynamic_buffer_bindings;
    ContentHash hash;
    [[nodiscard]] friend bool operator==(const BindGroupLayoutKey&, const BindGroupLayoutKey&) = default;
};

struct PipelineLayoutKey {
    std::vector<BindGroupLayoutKey> groups;
    ContentHash hash;
    [[nodiscard]] friend bool operator==(const PipelineLayoutKey&, const PipelineLayoutKey&) = default;
};

struct DynamicBufferBindingPolicy {
    u32 group{0};
    u32 binding{0};
    [[nodiscard]] friend bool operator==(const DynamicBufferBindingPolicy&, const DynamicBufferBindingPolicy&) = default;
};

[[nodiscard]] Result<PipelineLayoutKey> MakePipelineLayoutKey(const ShaderInterface& interface, std::span<const DynamicBufferBindingPolicy> dynamic_buffers = {});

struct LayoutGeneration {
    std::vector<scope<rhi::BindGroupLayout>> groups;
    scope<rhi::PipelineLayout> pipeline;
    std::vector<rhi::BindGroupLayout*> ordered_groups;
};

struct BorrowedLayout {
    ref<const LayoutGeneration> generation;

    [[nodiscard]] rhi::PipelineLayout& Pipeline() const noexcept {
        return *generation->pipeline;
    }

    [[nodiscard]] std::span<rhi::BindGroupLayout* const> BindGroupLayouts() const noexcept {
        return generation->ordered_groups;
    }
};

class LayoutCache {
public:
    explicit LayoutCache(rhi::Device& device)
        : device_(&device) {}

    [[nodiscard]] Result<BorrowedLayout> GetOrCreate(const PipelineLayoutKey& key);
    void MarkUsed(const BorrowedLayout& layout, rhi::SubmissionTicket submission);
    [[nodiscard]] size_t Prune(DeferredReleaseQueue& releases);

private:
    struct Entry {
        PipelineLayoutKey key;
        ref<const LayoutGeneration> generation;
        rhi::SubmissionTicket last_used;
    };

    rhi::Device* device_;
    std::vector<Entry> entries_;
};

} // namespace woki::gfx
