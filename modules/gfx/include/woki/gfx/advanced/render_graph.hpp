#pragma once

#include <any>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <typeindex>
#include <functional>
#include <unordered_map>

#include <woki/core.hpp>
#include <woki/rhi/descriptors.hpp>

namespace woki::rhi {
class Buffer;
class CommandEncoder;
class ComputePassEncoder;
class Device;
class RenderPassEncoder;
class Texture;
class TextureView;
} // namespace woki::rhi

namespace woki::gfx {

class CompiledRenderGraph;
class GraphCompiler;
class GraphExecutor;
struct GraphCompileReport;
class PassBuilder;
class RenderGraphContext;

namespace graph_detail {
struct Definition;
struct ExecutorState;
struct Identity;
} // namespace graph_detail

inline constexpr u32 kInvalidGraphIndex = ~u32{0};

enum class ResourceLifetime : u8 { Transient, Imported, Persistent, Temporal, Presentation, Readback };
enum class PassKind : u8 { Render, Compute, Copy };
enum class QueueClass : u8 { Graphics, Compute, Copy };
enum class QueuePreference : u8 { Automatic, Graphics, Compute, Copy };
enum class GraphAccess : u8 { Sampled, StorageRead, StorageWrite, ColorAttachment, DepthRead, DepthWrite, CopySource, CopyDestination, Vertex, Index, Uniform, Indirect };
enum class ExternalState : u8 { Undefined, ShaderRead, RenderAttachment, CopySource, CopyDestination, Present, HostRead };
enum class ExtentKind : u8 { Fixed, Relative };

struct GraphExtent final {
    ExtentKind kind{ExtentKind::Relative};
    u32 width{};
    u32 height{};
    f32 scale_x{1.f};
    f32 scale_y{1.f};

    [[nodiscard]] static constexpr GraphExtent Fixed(u32 width, u32 height) noexcept {
        return {.kind = ExtentKind::Fixed, .width = width, .height = height};
    }

    [[nodiscard]] static constexpr GraphExtent Relative(f32 x = 1.f, f32 y = 1.f) noexcept {
        return {.kind = ExtentKind::Relative, .scale_x = x, .scale_y = y};
    }
};

struct GraphTextureDesc final {
    std::string label;
    GraphExtent extent{};
    u32 depth_or_layers{1};
    u32 mip_levels{1};
    u32 sample_count{1};
    rhi::TextureDimension dimension{rhi::TextureDimension::e2D};
    rhi::TextureFormat format{rhi::TextureFormat::Undefined};
    rhi::TextureUsage usage{rhi::TextureUsage::None};
    std::vector<rhi::TextureFormat> view_formats;
    rhi::TextureAspect view_aspect{rhi::TextureAspect::All};
};

inline constexpr u32 kRemainingSubresources = ~u32{0};

struct TextureSubresourceRange final {
    u32 base_mip_level{};
    u32 mip_level_count{kRemainingSubresources};
    u32 base_array_layer{};
    u32 array_layer_count{kRemainingSubresources};
    rhi::TextureAspect aspect{rhi::TextureAspect::All};

    [[nodiscard]] friend bool operator==(const TextureSubresourceRange&, const TextureSubresourceRange&) = default;
};

struct GraphTextureViewDesc final {
    TextureSubresourceRange subresources;
    rhi::TextureFormat format{rhi::TextureFormat::Undefined};
    rhi::TextureViewDimension dimension{rhi::TextureViewDimension::Undefined};
    rhi::TextureUsage usage{rhi::TextureUsage::None};
    std::string label;

    [[nodiscard]] friend bool operator==(const GraphTextureViewDesc&, const GraphTextureViewDesc&) = default;
};

struct AttachmentRegion final {
    GraphTextureViewDesc view;
    std::optional<rhi::Origin2D> viewport_origin;
    std::optional<rhi::Extent2D> viewport_extent;
    std::optional<rhi::Origin2D> scissor_origin;
    std::optional<rhi::Extent2D> scissor_extent;
};

struct GraphBufferDesc final {
    std::string label;
    u64 size{};
    u64 alignment{1};
    rhi::BufferUsage usage{rhi::BufferUsage::None};
};

struct ExternalTextureContract final {
    GraphTextureDesc descriptor;
    GraphTextureViewDesc default_view;
    ExternalState initial_state{ExternalState::Undefined};
    ExternalState final_state{ExternalState::Undefined};
    bool frame_bound{};
};

struct ExternalBufferContract final {
    GraphBufferDesc descriptor;
    ExternalState initial_state{ExternalState::Undefined};
    ExternalState final_state{ExternalState::Undefined};
    bool frame_bound{};
};

struct TemporalTextureImport final {
    ref<rhi::Texture> previous;
    ref<rhi::Texture> current;
    GraphTextureDesc descriptor;
};

template <typename Tag>
class GraphHandle {
public:
    GraphHandle() = default;

    [[nodiscard]] explicit operator bool() const noexcept {
        return index_ != kInvalidGraphIndex;
    }

    [[nodiscard]] u32 Index() const noexcept {
        return index_;
    }

    [[nodiscard]] u32 Generation() const noexcept {
        return generation_;
    }

    [[nodiscard]] static GraphHandle FromGraph(u32 index, u32 generation, std::weak_ptr<graph_detail::Identity> identity) noexcept {
        GraphHandle value;
        value.index_ = index;
        value.generation_ = generation;
        value.identity_ = std::move(identity);
        return value;
    }

    [[nodiscard]] bool IsFrom(const graph_detail::Identity* identity) const noexcept {
        return identity_.lock().get() == identity;
    }

    [[nodiscard]] friend bool operator==(const GraphHandle& lhs, const GraphHandle& rhs) noexcept {
        return lhs.index_ == rhs.index_ && lhs.generation_ == rhs.generation_ && lhs.identity_.lock().get() == rhs.identity_.lock().get();
    }

private:
    friend class RenderGraphBuilder;
    friend class PassBuilder;
    friend class GraphCompiler;
    friend class GraphExecutor;
    friend class RenderGraphContext;
    u32 index_{kInvalidGraphIndex};
    u32 generation_{};
    std::weak_ptr<graph_detail::Identity> identity_;
};

struct TextureTag;
struct BufferTag;
struct PassTag;
struct TextureVersionTag;
struct BufferVersionTag;
using GraphTexture = GraphHandle<TextureTag>;
using GraphBuffer = GraphHandle<BufferTag>;
using GraphPass = GraphHandle<PassTag>;
using GraphTextureRef = GraphHandle<TextureVersionTag>;
using GraphBufferRef = GraphHandle<BufferVersionTag>;

struct GraphTemporalTexture final {
    GraphTexture previous;
    GraphTextureRef previous_version;
    GraphTexture current;
};

struct ColorAttachment final {
    u32 slot{};
    rhi::LoadOp load{rhi::LoadOp::Clear};
    rhi::StoreOp store{rhi::StoreOp::Store};
    rhi::Color clear{0.12, 0.12, 0.18, 1.0};
    std::optional<GraphTextureRef> resolve;
    AttachmentRegion region;
};

struct DepthAttachment final {
    rhi::LoadOp load{rhi::LoadOp::Clear};
    rhi::StoreOp store{rhi::StoreOp::Store};
    f32 clear{1.f};
    bool read_only{};
    AttachmentRegion region;
};

struct GraphDiagnostic final {
    std::string code;
    std::string message;
    std::string stage;
    std::string pass;
    std::string resource;
    std::vector<std::string> chain;
};

class GraphBlackboard final {
public:
    template <typename T, typename... Args>
    [[nodiscard]] Result<std::reference_wrapper<T>> Emplace(Args&&... args) {
        const std::type_index key(typeid(T));
        if (values_.contains(key))
            return Err(ErrorCode::ValidationInvalidState, "GRF1018 duplicate blackboard type");
        auto [it, inserted] = values_.emplace(key, std::any(T(std::forward<Args>(args)...)));
        (void)inserted;
        return Ok(std::ref(*std::any_cast<T>(&it->second)));
    }

    template <typename T>
    [[nodiscard]] T* Get() noexcept {
        const auto it = values_.find(std::type_index(typeid(T)));
        return it == values_.end() ? nullptr : std::any_cast<T>(&it->second);
    }

    template <typename T>
    [[nodiscard]] const T* Get() const noexcept {
        const auto it = values_.find(std::type_index(typeid(T)));
        return it == values_.end() ? nullptr : std::any_cast<T>(&it->second);
    }

private:
    std::unordered_map<std::type_index, std::any> values_;
};

// libstdc++ versions without C++23 move_only_function use this equivalent
// owning type erasure. It intentionally cannot copy captured state.
class GraphExecuteCallback final {
public:
    GraphExecuteCallback() = default;

    template <typename Fn>
    requires(!std::same_as<std::remove_cvref_t<Fn>, GraphExecuteCallback>)
    GraphExecuteCallback(Fn&& fn)
        : callable_(std::make_unique<Model<std::remove_cvref_t<Fn>>>(std::forward<Fn>(fn))) {}

    GraphExecuteCallback(GraphExecuteCallback&&) noexcept = default;
    GraphExecuteCallback& operator=(GraphExecuteCallback&&) noexcept = default;
    GraphExecuteCallback(const GraphExecuteCallback&) = delete;
    GraphExecuteCallback& operator=(const GraphExecuteCallback&) = delete;

    [[nodiscard]] explicit operator bool() const noexcept {
        return callable_ != nullptr;
    }

    Result<void> operator()(RenderGraphContext& context) {
        return callable_->Call(context);
    }

private:
    struct Concept {
        virtual ~Concept() = default;
        virtual Result<void> Call(RenderGraphContext&) = 0;
    };

    template <typename Fn>
    struct Model final : Concept {
        explicit Model(Fn fn)
            : fn_(std::move(fn)) {}

        Result<void> Call(RenderGraphContext& context) override {
            if constexpr (std::is_void_v<std::invoke_result_t<Fn&, RenderGraphContext&>>) {
                fn_(context);
                return Ok();
            } else {
                return Result<void>(fn_(context));
            }
        }

        Fn fn_;
    };

    std::unique_ptr<Concept> callable_;
};

class PassBuilder final {
public:
    [[nodiscard]] GraphPass Handle() const noexcept {
        return pass_;
    }

    PassBuilder& Read(GraphTextureRef resource, GraphAccess access = GraphAccess::Sampled, TextureSubresourceRange range = {}, GraphTextureViewDesc view = {});
    PassBuilder& Read(GraphBufferRef resource, GraphAccess access);
    [[nodiscard]] GraphTextureRef Write(GraphTexture resource, GraphAccess access = GraphAccess::StorageWrite, TextureSubresourceRange range = {}, GraphTextureViewDesc view = {});
    [[nodiscard]] GraphBufferRef Write(GraphBuffer resource, GraphAccess access = GraphAccess::StorageWrite);
    [[nodiscard]] GraphTextureRef ReadWrite(GraphTextureRef resource, GraphAccess access = GraphAccess::StorageWrite);
    [[nodiscard]] GraphBufferRef ReadWrite(GraphBufferRef resource, GraphAccess access = GraphAccess::StorageWrite);
    [[nodiscard]] GraphTextureRef Color(GraphTexture resource, ColorAttachment attachment = {});
    [[nodiscard]] GraphTextureRef ResolveColor(GraphTexture multisample, GraphTexture resolve, ColorAttachment attachment = {});
    [[nodiscard]] GraphTextureRef Depth(GraphTexture resource, DepthAttachment attachment = {});
    PassBuilder& Copy(GraphTextureRef source, GraphTexture destination);
    PassBuilder& Copy(GraphBufferRef source, GraphBuffer destination, u64 size = 0);
    PassBuilder& DependsOn(GraphPass pass);
    PassBuilder& Queue(QueuePreference preference, bool async_compute_eligible = false);
    PassBuilder& SideEffect(std::string name);
    PassBuilder& Execute(GraphExecuteCallback callback);

private:
    friend class RenderGraphBuilder;
    PassBuilder(std::shared_ptr<graph_detail::Definition> definition, GraphPass pass);
    std::shared_ptr<graph_detail::Definition> definition_;
    GraphPass pass_;
};

// A builder is single-threaded and frame-local. Compile consumes it; all handles
// from another builder, or used after consumption, are rejected with GRF codes.
class RenderGraphBuilder final {
public:
    RenderGraphBuilder();
    RenderGraphBuilder(RenderGraphBuilder&&) noexcept = default;
    RenderGraphBuilder& operator=(RenderGraphBuilder&&) noexcept = default;
    RenderGraphBuilder(const RenderGraphBuilder&) = delete;
    RenderGraphBuilder& operator=(const RenderGraphBuilder&) = delete;

    [[nodiscard]] GraphTexture CreateTexture(GraphTextureDesc descriptor, ResourceLifetime lifetime = ResourceLifetime::Transient);
    [[nodiscard]] GraphBuffer CreateBuffer(GraphBufferDesc descriptor, ResourceLifetime lifetime = ResourceLifetime::Transient);
    [[nodiscard]] GraphTexture ImportTexture(ExternalTextureContract contract, ref<rhi::Texture> texture = {});
    [[nodiscard]] GraphBuffer ImportBuffer(ExternalBufferContract contract, ref<rhi::Buffer> buffer = {});
    [[nodiscard]] GraphTemporalTexture ImportTemporal(const TemporalTextureImport& history);
    [[nodiscard]] GraphTextureRef Initial(GraphTexture resource) const;
    [[nodiscard]] GraphBufferRef Initial(GraphBuffer resource) const;
    [[nodiscard]] PassBuilder AddPass(std::string name, PassKind kind);
    [[nodiscard]] Result<void> Export(GraphTextureRef resource, ExternalState final_state = ExternalState::Undefined);
    [[nodiscard]] Result<void> Export(GraphBufferRef resource, ExternalState final_state = ExternalState::Undefined);
    [[nodiscard]] GraphBlackboard& Blackboard() noexcept;
    [[nodiscard]] Result<CompiledRenderGraph> Compile(u32 width, u32 height);
    [[nodiscard]] GraphCompileReport CompileWithReport(u32 width, u32 height);

private:
    friend class PassBuilder;
    friend class GraphCompiler;
    std::shared_ptr<graph_detail::Definition> definition_;
    GraphBlackboard closed_blackboard_;
};

class RenderGraphContext final {
public:
    struct Runtime;
    [[nodiscard]] rhi::Device& Device() const noexcept;
    [[nodiscard]] rhi::CommandEncoder& CommandEncoder() const noexcept;
    [[nodiscard]] rhi::RenderPassEncoder* RenderEncoder() const noexcept;
    [[nodiscard]] rhi::ComputePassEncoder* ComputeEncoder() const noexcept;
    [[nodiscard]] Result<std::reference_wrapper<rhi::Texture>> Texture(GraphTextureRef resource) const;
    [[nodiscard]] Result<std::reference_wrapper<rhi::TextureView>> TextureView(GraphTextureRef resource) const;
    [[nodiscard]] Result<std::reference_wrapper<rhi::TextureView>> TextureView(GraphTextureRef resource, GraphTextureViewDesc view) const;
    [[nodiscard]] Result<std::reference_wrapper<rhi::Buffer>> Buffer(GraphBufferRef resource) const;
    [[nodiscard]] rhi::Extent3D Extent(GraphTextureRef resource) const noexcept;
    [[nodiscard]] rhi::Extent3D Extent(GraphTextureRef resource, TextureSubresourceRange range) const noexcept;

    [[nodiscard]] u32 Width() const noexcept {
        return width_;
    }

    [[nodiscard]] u32 Height() const noexcept {
        return height_;
    }

    [[nodiscard]] GraphBlackboard& Blackboard() const noexcept;

private:
    friend class GraphExecutor;
    Runtime* runtime_{};
    u32 pass_{};
    u32 width_{};
    u32 height_{};
};

} // namespace woki::gfx
