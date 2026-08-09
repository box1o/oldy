#pragma once

#include <functional>
#include <string_view>
#include <type_traits>

#include <woki/core.hpp>
#include <woki/rhi/forward.hpp>
#include <woki/assert/assert.hpp>

#include "context.hpp"
#include "internal.hpp"
#include "resources.hpp"

namespace woki::rhi {

class RenderGraph;

class CopyPassContext;
class RenderPassContext;

class PassBuilder final {
public:
    PassBuilder& Target(Framebuffer framebuffer, FramebufferTargetConfig config = {});
    PassBuilder& Color(u32 slot, Resource resource, ColorAttachmentConfig config = {});
    PassBuilder& Color(u32 slot, PerFrameSlot resource, ColorAttachmentConfig config = {});
    PassBuilder& Depth(Resource resource, DepthAttachmentConfig config = {});
    PassBuilder& Depth(PerFrameSlot resource, DepthAttachmentConfig config = {});
    PassBuilder& Sample(Resource resource, SampleMode mode = SampleMode::ColorTexture);
    PassBuilder& Copy(Resource src, Resource dst);

    template <typename Fn>
    PassBuilder& Execute(Fn&& callback);

private:
    friend class RenderGraphBuilder;
    PassBuilder(ref<render_graph::detail::GraphBlueprint> blueprint, u32 pass_index);

    ref<render_graph::detail::GraphBlueprint> blueprint_{};
    u32 pass_index_{kInvalidGraphResource};
};

class FramebufferBuilder final {
public:
    FramebufferBuilder& Color(u32 slot, Resource resource);
    FramebufferBuilder& Depth(Resource resource);
    [[nodiscard]] Framebuffer Build();

private:
    friend class RenderGraphBuilder;
    explicit FramebufferBuilder(ref<render_graph::detail::GraphBlueprint> blueprint, u32 framebuffer_index);

    ref<render_graph::detail::GraphBlueprint> blueprint_{};
    u32 framebuffer_index_{kInvalidGraphResource};
};

class RenderGraphBuilder final {
public:
    explicit RenderGraphBuilder(ref<Device> device);

    [[nodiscard]] PerFrameSlot PerFrame();
    [[nodiscard]] Resource Transient(TransientDesc desc);
    [[nodiscard]] Resource Use(ref<Texture> texture);

    [[nodiscard]] FramebufferBuilder Framebuffer();
    [[nodiscard]] PassBuilder AddPass(std::string_view debug_name);

    template <typename T>
    void SetPassData(const std::string_view pass_name, T user_data) {
        const auto it = blueprint_->pass_name_to_index.find(std::string(pass_name));
        if (it != blueprint_->pass_name_to_index.end()) {
            blueprint_->passes[it->second].user_data = createRef<T>(std::move(user_data));
        }
    }

    [[nodiscard]] Result<ref<RenderGraph>> Compile(u32 width, u32 height);

private:
    friend class PassBuilder;
    friend class FramebufferBuilder;
    friend class RenderGraph;

    [[nodiscard]] u32 AllocateResource(render_graph::detail::ResourceRecord record);
    [[nodiscard]] u32 AllocateFramebuffer();
    [[nodiscard]] u32 AllocatePass(std::string_view debug_name);

    ref<Device> device_{};
    ref<render_graph::detail::GraphBlueprint> blueprint_{createRef<render_graph::detail::GraphBlueprint>()};
};

template <typename Fn>
PassBuilder& PassBuilder::Execute(Fn&& callback) {
    WOKI_ASSERT(blueprint_ != nullptr);
    render_graph::detail::PassRecord& pass = blueprint_->passes[pass_index_];

    if constexpr (std::is_invocable_v<Fn, CopyPassContext&> && !std::is_invocable_v<Fn, RenderPassContext&>) {
        pass.copy_execute = [fn = std::forward<Fn>(callback)](CopyPassContext& ctx) mutable { fn(ctx); };
    } else if constexpr (std::is_invocable_v<Fn, RenderPassContext&>) {
        pass.render_execute = [fn = std::forward<Fn>(callback)](RenderPassContext& ctx) mutable { fn(ctx); };
    } else {
        static_assert(sizeof(Fn) == 0, "Pass callback must be callable as void(RenderPassContext&) or void(CopyPassContext&)");
    }
    return *this;
}

} // namespace woki::rhi
