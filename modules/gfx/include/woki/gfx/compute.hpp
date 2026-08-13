#pragma once

#include <string>
#include <compare>
#include <variant>
#include <vector>

#include <woki/asset.hpp>
#include <woki/core.hpp>
#include "handles.hpp"

namespace woki::gfx {

struct GpuBufferTag;
struct GpuTextureTag;
using BufferHandle = Handle<GpuBufferTag>;
using GpuTextureHandle = Handle<GpuTextureTag>;

enum class BufferUsage : u8 { Storage, Uniform, Indirect, Transfer };
enum class GpuTextureFormat : u8 { RGBA8Unorm, RGBA8UnormSrgb, BGRA8Unorm, RGBA16Float, R32Uint };
enum class GpuResourceAccess : u8 { Read, Write, ReadWrite };

struct GpuTextureSubresource final {
    u32 base_mip_level{};
    u32 mip_level_count{1};
    u32 base_array_layer{};
    u32 array_layer_count{1};
};

struct BufferDescriptor final {
    u64 size{};
    BufferUsage usage{BufferUsage::Storage};
    std::string label;
};

struct GpuTextureDescriptor final {
    u32 width{1};
    u32 height{1};
    u32 depth_or_layers{1};
    u32 mip_levels{1};
    GpuTextureFormat format{GpuTextureFormat::RGBA8Unorm};
    std::string label;
};

enum class GpuJobPriority : u8 { Background, Normal, Interactive };
enum class GpuJobState : u8 { Queued, Submitted, Complete, Cancelled, Failed, DeviceLost };

struct GpuJobTicket final {
    u64 id{};
    GpuSubmissionId submission;

    [[nodiscard]] bool IsValid() const noexcept {
        return id != 0;
    }
};

struct DispatchSize final {
    u32 x{1};
    u32 y{1};
    u32 z{1};
};

class GpuJobBuilder final {
public:
    GpuJobBuilder& DependsOn(GpuJobTicket ticket);
    GpuJobBuilder& Dispatch(asset::AssetId cooked_shader, std::string entry_point, DispatchSize size);
    GpuJobBuilder& Copy(BufferHandle source, BufferHandle destination, u64 size, u64 source_offset = 0, u64 destination_offset = 0);
    GpuJobBuilder& BindBuffer(u32 group, u32 binding, BufferHandle buffer, GpuResourceAccess access, u64 offset = 0, u64 size = 0);
    GpuJobBuilder& BindTexture(u32 group, u32 binding, GpuTextureHandle texture, GpuResourceAccess access, GpuTextureSubresource subresource = {});
    GpuJobBuilder& Priority(GpuJobPriority priority) noexcept;
    GpuJobBuilder& Budget(u64 bytes, u32 dispatches) noexcept;

private:
    friend class ComputeService;
    friend struct RuntimeFacadeAccess;

    struct DispatchDeclaration final {
        asset::AssetId shader;
        std::string entry;
        DispatchSize size;
    };

    struct CopyDeclaration final {
        BufferHandle source;
        BufferHandle destination;
        u64 size{};
        u64 source_offset{};
        u64 destination_offset{};
    };

    struct BufferBinding final {
        u32 group{};
        u32 binding{};
        BufferHandle buffer;
        GpuResourceAccess access{GpuResourceAccess::Read};
        u64 offset{};
        u64 size{};
    };

    struct TextureBinding final {
        u32 group{};
        u32 binding{};
        GpuTextureHandle texture;
        GpuResourceAccess access{GpuResourceAccess::Read};
        GpuTextureSubresource subresource;
    };

    std::vector<GpuJobTicket> dependencies_;
    std::vector<std::variant<DispatchDeclaration, CopyDeclaration>> declarations_;
    std::vector<BufferBinding> buffers_;
    std::vector<TextureBinding> textures_;
    GpuJobPriority priority_{GpuJobPriority::Normal};
    u64 byte_budget_{};
    u32 dispatch_budget_{};
};

class ComputeService final {
public:
    ComputeService();
    ~ComputeService();
    [[nodiscard]] Result<BufferHandle> CreateBuffer(BufferDescriptor descriptor);
    [[nodiscard]] Result<void> DestroyBuffer(BufferHandle buffer);
    [[nodiscard]] Result<GpuTextureHandle> CreateTexture(GpuTextureDescriptor descriptor);
    [[nodiscard]] Result<void> DestroyTexture(GpuTextureHandle texture);
    [[nodiscard]] GpuJobBuilder CreateJob() const;
    [[nodiscard]] Result<GpuJobTicket> Submit(GpuJobBuilder job);
    [[nodiscard]] GpuJobState Poll(GpuJobTicket ticket) const noexcept;
    [[nodiscard]] GpuSubmissionId Submission(GpuJobTicket ticket) const noexcept;
    [[nodiscard]] std::string Error(GpuJobTicket ticket) const;
    [[nodiscard]] Result<void> Cancel(GpuJobTicket ticket);

private:
    friend class RenderRuntime;
    friend struct RuntimeFacadeAccess;
    struct Impl;
    scope<Impl> impl_;
};

} // namespace woki::gfx
