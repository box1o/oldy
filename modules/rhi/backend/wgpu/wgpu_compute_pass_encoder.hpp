#pragma once

#include <woki/rhi/compute_pass_encoder.hpp>

#include "detail/handle.hpp"

namespace woki::rhi::wgpu {

class WgpuComputePassEncoderImpl final : public ComputePassEncoder {
public:
    explicit WgpuComputePassEncoderImpl(WGPUComputePassEncoder encoder) noexcept;
    ~WgpuComputePassEncoderImpl() override = default;

    void DispatchWorkgroups(u32 workgroup_count_x, u32 workgroup_count_y, u32 workgroup_count_z) override;
    void DispatchWorkgroupsIndirect(const Buffer& indirect_buffer, u64 indirect_offset) override;
    void End() override;
    void InsertDebugMarker(std::string_view marker_label) override;
    void PopDebugGroup() override;
    void PushDebugGroup(std::string_view group_label) override;
    void SetBindGroup(u32 group_index, const BindGroup* group, std::span<const u32> dynamic_offsets) override;
    void SetImmediates(u32 offset, const void* data, size_t size) override;
    void SetLabel(std::string_view label) override;
    void SetPipeline(const ComputePipeline& pipeline) override;
    void SetResourceTable(const ResourceTable* table) override;
    void WriteTimestamp(const QuerySet& query_set, u32 query_index) override;

    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

private:
    detail::ComputePassEncoderHandle encoder_;
};

} // namespace woki::rhi::wgpu
