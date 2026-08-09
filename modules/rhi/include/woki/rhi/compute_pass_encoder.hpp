#pragma once

#include <span>
#include <string_view>

#include <woki/core.hpp>

#include "forward.hpp"
#include "descriptors.hpp"

namespace woki::rhi {

class ComputePassEncoder {
public:
    virtual ~ComputePassEncoder() = default;

    virtual void DispatchWorkgroups(u32 workgroup_count_x, u32 workgroup_count_y = 1, u32 workgroup_count_z = 1) = 0;
    virtual void DispatchWorkgroupsIndirect(const Buffer& indirect_buffer, u64 indirect_offset) = 0;
    virtual void End() = 0;
    virtual void InsertDebugMarker(std::string_view marker_label) = 0;
    virtual void PopDebugGroup() = 0;
    virtual void PushDebugGroup(std::string_view group_label) = 0;
    virtual void SetBindGroup(u32 group_index, const BindGroup* group = nullptr, std::span<const u32> dynamic_offsets = {}) = 0;
    virtual void SetImmediates(u32 offset, const void* data, size_t size) = 0;
    virtual void SetLabel(std::string_view label) = 0;
    virtual void SetPipeline(const ComputePipeline& pipeline) = 0;
    virtual void SetResourceTable(const ResourceTable* table = nullptr) = 0;
    virtual void WriteTimestamp(const QuerySet& query_set, u32 query_index) = 0;

    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    ComputePassEncoder() = default;
};

} // namespace woki::rhi
