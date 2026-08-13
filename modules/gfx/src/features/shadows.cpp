#include "internal.hpp"

namespace woki::gfx::feature_detail {
namespace {
math::mat4f OrthographicLightProjection(const f32 left, const f32 right, const f32 bottom, const f32 top, const f32 near_plane, const f32 far_plane) {
    math::mat4f result = math::mat4f::identity();
    result(0, 0) = 2.0F / (right - left);
    result(0, 3) = -(right + left) / (right - left);
    result(1, 1) = 2.0F / (top - bottom);
    result(1, 3) = -(top + bottom) / (top - bottom);
    result(2, 2) = -1.0F / (far_plane - near_plane);
    result(2, 3) = -near_plane / (far_plane - near_plane);
    return result;
}

std::array<math::vec3f, 8> FrustumSliceCorners(const CameraState& camera, const f32 slice_near, const f32 slice_far) {
    std::array<math::vec3f, 8> result{};
    const f32 denominator = std::max(camera.far_plane - camera.near_plane, 1.0e-6F);
    const f32 near_t = (slice_near - camera.near_plane) / denominator;
    const f32 far_t = (slice_far - camera.near_plane) / denominator;
    u32 index{};
    for (const f32 y : {-1.0F, 1.0F})
        for (const f32 x : {-1.0F, 1.0F}) {
            const auto near_h = camera.inverse_view_projection * math::vec4f{x, y, 0.0F, 1.0F};
            const auto far_h = camera.inverse_view_projection * math::vec4f{x, y, 1.0F, 1.0F};
            const math::vec3f near_corner{near_h.x / near_h.w, near_h.y / near_h.w, near_h.z / near_h.w};
            const math::vec3f far_corner{far_h.x / far_h.w, far_h.y / far_h.w, far_h.z / far_h.w};
            const auto ray = far_corner - near_corner;
            result[index] = near_corner + ray * near_t;
            result[index + 4] = near_corner + ray * far_t;
            ++index;
        }
    return result;
}

math::mat4f StableCascadeMatrix(const std::array<math::vec3f, 8>& corners, math::vec3f light_direction, const u32 resolution) {
    light_direction = light_direction.normalized();
    math::vec3f center{};
    for (const auto& corner : corners)
        center += corner;
    center /= static_cast<f32>(corners.size());
    f32 radius{};
    for (const auto& corner : corners)
        radius = std::max(radius, (corner - center).length());
    radius = std::ceil(radius * 16.0F) / 16.0F;
    const math::vec3f up = std::abs(light_direction.y) > 0.95F ? math::vec3f{1.0F, 0.0F, 0.0F} : math::vec3f{0.0F, 1.0F, 0.0F};
    const auto light_view = math::lookAt(center - light_direction * (radius + 100.0F), center, up);
    math::vec3f minimum{std::numeric_limits<f32>::max(), std::numeric_limits<f32>::max(), std::numeric_limits<f32>::max()};
    math::vec3f maximum{std::numeric_limits<f32>::lowest(), std::numeric_limits<f32>::lowest(), std::numeric_limits<f32>::lowest()};
    for (const auto& corner : corners) {
        const auto point = light_view * math::vec4f{corner.x, corner.y, corner.z, 1.0F};
        minimum.x = std::min(minimum.x, point.x);
        minimum.y = std::min(minimum.y, point.y);
        minimum.z = std::min(minimum.z, point.z);
        maximum.x = std::max(maximum.x, point.x);
        maximum.y = std::max(maximum.y, point.y);
        maximum.z = std::max(maximum.z, point.z);
    }
    const f32 extent = std::max(maximum.x - minimum.x, maximum.y - minimum.y);
    const f32 texel = extent / static_cast<f32>(resolution);
    const f32 center_x = std::floor((minimum.x + maximum.x) * 0.5F / texel) * texel;
    const f32 center_y = std::floor((minimum.y + maximum.y) * 0.5F / texel) * texel;
    const f32 half = extent * 0.5F;
    constexpr f32 depth_padding = 50.0F;
    const f32 near_plane = std::max(0.0F, -maximum.z - depth_padding);
    const f32 far_plane = std::max(near_plane + 0.001F, -minimum.z + depth_padding);
    return OrthographicLightProjection(center_x - half, center_x + half, center_y - half, center_y + half, near_plane, far_plane) * light_view;
}
} // namespace
} // namespace woki::gfx::feature_detail

namespace woki::gfx::feature_detail {

namespace {

class ShadowFeature final : public FeatureBase {
public:
    using FeatureBase::FeatureBase;

    ~ShadowFeature() override {
        if (atlas_ != nullptr && services_ != nullptr && services_->releases != nullptr)
            services_->releases->Retire(std::move(atlas_), last_used_);
    }

    void OnSubmitted(const rhi::SubmissionTicket submission) const override {
        if (last_used_ < submission)
            last_used_ = submission;
    }

    Result<void> DeclareGraph(GraphDeclarationContext& context) const override {
        u32 resolution = 2048;
        u32 cascades = 4;
        if (const auto* value = Setting("resolution"))
            resolution = static_cast<u32>(std::max<i64>(1, std::get<i64>(*value)));
        if (const auto* value = Setting("cascades"))
            cascades = static_cast<u32>(std::clamp<i64>(std::get<i64>(*value), 1, 4));
        std::array<f32, 4> splits{};
        const f32 near_plane = Frame() != nullptr && Frame()->view != nullptr ? Frame()->view->camera.near_plane : 0.1F;
        const f32 far_plane = Frame() != nullptr && Frame()->view != nullptr ? Frame()->view->camera.far_plane : 1000.0F;
        constexpr f32 split_lambda = 0.65F;
        for (u32 cascade = 1; cascade <= cascades; ++cascade) {
            const f32 fraction = static_cast<f32>(cascade) / static_cast<f32>(cascades);
            const f32 logarithmic = near_plane * std::pow(far_plane / near_plane, fraction);
            const f32 uniform = near_plane + (far_plane - near_plane) * fraction;
            splits[cascade - 1] = std::lerp(uniform, logarithmic, split_lambda);
        }
        auto descriptor = TextureDescriptor("Directional shadow atlas", rhi::TextureFormat::Depth32Float, 1);
        descriptor.extent = GraphExtent::Fixed(resolution, resolution);
        descriptor.depth_or_layers = cascades;
        descriptor.usage = rhi::TextureUsage::RenderAttachment | rhi::TextureUsage::TextureBinding;
        if (atlas_ == nullptr || atlas_resolution_ != resolution || atlas_layers_ != cascades) {
            if (services_ == nullptr || services_->device == nullptr)
                return Err(ErrorCode::InvalidState, "shadow feature has no device service");
            if (atlas_ != nullptr && services_->releases != nullptr)
                services_->releases->Retire(std::move(atlas_), last_used_);
            TRY_ASSIGN(atlas_, services_->device->CreateTexture({.size = {resolution, resolution, cascades},
                                   .format = rhi::TextureFormat::Depth32Float,
                                   .usage = rhi::TextureUsage::RenderAttachment | rhi::TextureUsage::TextureBinding,
                                   .label = "Persistent directional shadow atlas"}));
            atlas_resolution_ = resolution;
            atlas_layers_ = cascades;
            last_used_ = {};
        }
        ExternalTextureContract contract{.descriptor = descriptor, .default_view = {}, .initial_state = ExternalState::ShaderRead, .final_state = ExternalState::ShaderRead, .frame_bound = false};
        auto atlas = context.graph.ImportTexture(std::move(contract), atlas_);
        const auto* gpu_visibility = context.blackboard.Get<GpuVisibilityOutput>();
        const IndirectDrawStream* indirect = gpu_visibility != nullptr && gpu_visibility->stream.ready ? &gpu_visibility->stream : nullptr;
        GraphTextureRef version;
        auto* services = services_;
        std::array<math::mat4f, 4> matrices{};
        std::array<std::array<f32, 4>, 4> regions{};
        for (u32 cascade = 0; cascade < cascades; ++cascade) {
            matrices[cascade] = math::mat4f::identity();
            if (Frame() != nullptr && Frame()->view != nullptr) {
                const auto& camera = Frame()->view->camera;
                math::vec3f direction{0.0F, -1.0F, 0.0F};
                for (const auto& light : Frame()->world->Lights())
                    if (light.type == LightType::Directional && (light.visibility_mask & Frame()->view->layer_mask) != 0) {
                        direction = light.direction;
                        break;
                    }
                matrices[cascade] = StableCascadeMatrix(FrustumSliceCorners(camera, cascade == 0 ? near_plane : splits[cascade - 1], splits[cascade]), direction, resolution);
            }
            regions[cascade] = {0.0F, 0.0F, 1.0F, static_cast<f32>(cascade)};
            auto pass = context.graph.AddPass("Directional shadow cascade " + std::to_string(cascade), PassKind::Render);
            if (indirect != nullptr)
                pass.Read(indirect->commands_version, GraphAccess::Indirect).Read(indirect->counts_version, GraphAccess::Indirect);
            pass.SideEffect("shadow-cascade-" + std::to_string(cascade));
            DepthAttachment attachment = DepthLoad(rhi::LoadOp::Clear);
            attachment.region.view.subresources = {.base_mip_level = 0, .mip_level_count = 1, .base_array_layer = cascade, .array_layer_count = 1, .aspect = rhi::TextureAspect::DepthOnly};
            attachment.region.view.dimension = rhi::TextureViewDimension::e2D;
            attachment.region.viewport_extent = rhi::Extent2D{resolution, resolution};
            attachment.region.scissor_extent = rhi::Extent2D{resolution, resolution};
            version = pass.Depth(atlas, std::move(attachment));
            const auto shadow_matrix = matrices[cascade];
            pass.Execute([services, cascades, shadow_matrix, resolution, indirect](RenderGraphContext& graph) -> Result<void> {
                const u64 before = services->frame->draw_calls;
                TRY_VOID(services->bindings->UseViewProjection(graph.CommandEncoder(), shadow_matrix, shadow_matrix.inverse(), {}, 0.0F, 1.0F, static_cast<f32>(resolution), static_cast<f32>(resolution)));
                bool gpu_drawn{};
                if (indirect != nullptr)
                    TRY_ASSIGN(gpu_drawn, DrawIndirect(services, graph, *indirect, RenderPhase::Shadow, MaterialPass::Shadow, {{}, rhi::TextureFormat::Depth32Float, 1}, false));
                if (!gpu_drawn) {
                    if (indirect != nullptr)
                        services->frame->gpu_visibility.fallback = GpuDrivenFallbackReason::PipelinePending;
                    TRY_VOID(Draw(services, graph, RenderPhase::Shadow, MaterialPass::Shadow, {{}, rhi::TextureFormat::Depth32Float, 1}, false));
                }
                services->frame->shadow_draws += services->frame->draw_calls - before;
                services->frame->shadow_cascades = cascades;
                return Ok();
            });
        }
        auto result = context.blackboard.Emplace<ShadowFeatureOutput>(ShadowFeatureOutput{atlas, version, cascades, splits, matrices, regions});
        return result ? Ok() : Err(std::move(result).error());
    }

private:
    mutable ref<rhi::Texture> atlas_;
    mutable u32 atlas_resolution_{};
    mutable u32 atlas_layers_{};
    mutable rhi::SubmissionTicket last_used_;
};

} // namespace

Result<void> RegisterShadowFeatures(FeatureRegistry& registry) {
    auto shadows = Metadata("shadows", FeatureScope::ViewFamily, {StringId("lighting")},
        {{"resolution", ConfigType::Integer, false, ConfigValue{i64{2048}}}, {"cascades", ConfigType::Integer, false, ConfigValue{i64{4}}}});
    shadows.required_capabilities = {StringId("depth-texture")};
    shadows.insertion_points = {StringId("depth-prepass")};
    return RegisterFactory(registry, std::move(shadows), [](CompiledFeatureConfig config, StandardFeatureServices* services) { return createRef<ShadowFeature>(std::move(config), services); });
}

} // namespace woki::gfx::feature_detail
