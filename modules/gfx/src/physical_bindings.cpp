#include <algorithm>
#include <cstring>

#include <woki/gfx/advanced/physical_bindings.hpp>
#include <woki/gfx/advanced/gpu_scene.hpp>
#include <woki/rhi/render_pass_encoder.hpp>
#include <woki/rhi/command_encoder.hpp>
#include "gfx_util.hpp"

namespace woki::gfx {
namespace {

template <typename T>
void CopyMatrix(std::array<f32, 16>& target, const T& source) {
    static_assert(sizeof(target) == sizeof(source));
    std::memcpy(target.data(), &source, sizeof(source));
}

bool HasBinding(const PipelineLayoutKey& key, const u32 group, const u32 binding) {
    return group < key.groups.size()
           && std::ranges::any_of(
               key.groups[group].bindings,
               [&](const BindingInfo& value) { return value.binding == binding; }
           );
}

math::mat4f Jittered(const math::mat4f& matrix, const math::vec2f jitter) {
    auto result = matrix;
    for (u32 column = 0; column < 4; ++column) {
        result(0, column) += jitter.x * matrix(3, column);
        result(1, column) += jitter.y * matrix(3, column);
    }
    return result;
}

template <typename T>
std::vector<std::byte> UploadBytes(const T& value) {
    std::vector<std::byte> bytes(sizeof(T));
    std::memcpy(bytes.data(), &value, sizeof(T));
    return bytes;
}

} // namespace

PhysicalBindings::~PhysicalBindings() {
    if (fallback_palette_.allocation.IsValid())
        static_cast<void>(skin_pool_.Free(fallback_palette_.allocation));
}

PhysicalBindings::PhysicalBindings(
    ref<rhi::Device> device,
    LayoutCache& layouts,
    SkinPaletteRegistry& palettes,
    BufferPool& skin_pool,
    UploadScheduler& uploads
)
    : device_(std::move(device)),
      layouts_(layouts),
      palettes_(palettes),
      skin_pool_(skin_pool),
      uploads_(uploads) {}

Result<scope<PhysicalBindings>> PhysicalBindings::Create(
    ref<rhi::Device> device,
    LayoutCache& layouts,
    SkinPaletteRegistry& palettes,
    BufferPool& skin_pool,
    UploadScheduler& uploads,
    BorrowedShader diagnostic_shader
) {
    if (device == nullptr)
        return Err(ErrorCode::InvalidArgument, "physical bindings require a device");
    if (!diagnostic_shader.generation)
        return Err(ErrorCode::InvalidArgument, "physical bindings require a cooked diagnostic shader generation");
    auto result = scope<PhysicalBindings>(
        new PhysicalBindings(std::move(device), layouts, palettes, skin_pool, uploads)
    );
    result->object_stride_ = detail::AlignUp(
        static_cast<u32>(sizeof(abi::ObjectData)),
        std::max(1U, result->device_->Capabilities().GetLimits().min_uniform_buffer_offset_alignment)
    );
    const auto create =
        [&](ref<rhi::Buffer>& buffer, const u64 size, const rhi::BufferUsage usage, std::string label) -> Result<void> {
        TRY_ASSIGN(
            buffer,
            result->device_
                ->CreateBuffer({.size = size, .usage = usage | rhi::BufferUsage::CopyDst, .label = std::move(label)})
        );
        return Ok();
    };
    TRY_VOID(create(result->frame_buffer_, sizeof(abi::FrameData), rhi::BufferUsage::Uniform, "Frame ABI"));
    TRY_VOID(create(result->scene_buffer_, sizeof(abi::SceneData), rhi::BufferUsage::Uniform, "Scene ABI"));
    TRY_VOID(create(result->view_buffer_, sizeof(abi::ViewData), rhi::BufferUsage::Uniform, "View ABI"));
    TRY_VOID(create(
        result->object_buffer_,
        static_cast<u64>(result->object_stride_) * result->object_capacity_,
        rhi::BufferUsage::Uniform,
        "Object ABI"
    ));
    TRY_VOID(create(result->fallback_lighting_buffer_, 64, rhi::BufferUsage::Storage, "Empty clustered lighting ABI"));
    TRY_VOID(create(
        result->cluster_params_buffer_,
        sizeof(abi::ClusterParams),
        rhi::BufferUsage::Uniform,
        "Cluster parameters ABI"
    ));
    TRY_VOID(
        create(result->shadow_data_buffer_, sizeof(abi::ShadowData), rhi::BufferUsage::Uniform, "Shadow metadata ABI")
    );
    TRY_VOID(create(
        result->environment_data_buffer_,
        sizeof(abi::EnvironmentData),
        rhi::BufferUsage::Uniform,
        "Environment metadata ABI"
    ));
    TRY_ASSIGN(
        result->fallback_shadow_texture_,
        result->device_->CreateTexture(
            {.size = {1, 1, 1},
                .format = rhi::TextureFormat::Depth32Float,
                .usage = rhi::TextureUsage::TextureBinding | rhi::TextureUsage::RenderAttachment,
                .label = "Empty shadow atlas"}
        )
    );
    result->fallback_shadow_view_ = ref<rhi::TextureView>(result->fallback_shadow_texture_
            ->CreateView(
                {.format = rhi::TextureFormat::Depth32Float,
                    .dimension = rhi::TextureViewDimension::e2DArray,
                    .mip_level_count = 1,
                    .array_layer_count = 1,
                    .aspect = rhi::TextureAspect::DepthOnly,
                    .usage = rhi::TextureUsage::TextureBinding,
                    .label = "Empty shadow atlas view"}
            )
            .release());
    TRY_ASSIGN(
        result->shadow_sampler_,
        result->device_->CreateSampler(
            {.mag_filter = rhi::FilterMode::Linear,
                .min_filter = rhi::FilterMode::Linear,
                .mipmap_filter = rhi::MipmapFilterMode::Nearest,
                .compare = rhi::CompareFunction::LessEqual,
                .label = "Shadow PCF sampler"}
        )
    );
    TRY_ASSIGN(
        result->environment_sampler_,
        result->device_->CreateSampler(
            {.mag_filter = rhi::FilterMode::Linear,
                .min_filter = rhi::FilterMode::Linear,
                .mipmap_filter = rhi::MipmapFilterMode::Linear,
                .label = "Environment sampler"}
        )
    );
    TRY_ASSIGN(
        result->fallback_environment_textures_[0],
        result->device_->CreateTexture(
            {.size = {1, 1, 6},
                .format = rhi::TextureFormat::RGBA8Unorm,
                .usage = rhi::TextureUsage::TextureBinding | rhi::TextureUsage::CopyDst,
                .label = "Black environment cube"}
        )
    );
    result->fallback_environment_views_[0] = ref<rhi::TextureView>(result->fallback_environment_textures_[0]
            ->CreateView(
                {.format = rhi::TextureFormat::RGBA8Unorm,
                    .dimension = rhi::TextureViewDimension::Cube,
                    .mip_level_count = 1,
                    .array_layer_count = 6,
                    .label = "Black environment cube view"}
            )
            .release());
    TRY_ASSIGN(
        result->fallback_environment_textures_[1],
        result->device_->CreateTexture(
            {.size = {1, 1, 1},
                .format = rhi::TextureFormat::RG16Float,
                .usage = rhi::TextureUsage::TextureBinding | rhi::TextureUsage::CopyDst,
                .label = "Neutral environment BRDF LUT"}
        )
    );
    result->fallback_environment_views_[1] = ref<rhi::TextureView>(result->fallback_environment_textures_[1]
            ->CreateView(
                {.format = rhi::TextureFormat::RG16Float,
                    .dimension = rhi::TextureViewDimension::e2D,
                    .mip_level_count = 1,
                    .array_layer_count = 1,
                    .label = "Neutral environment BRDF LUT view"}
            )
            .release());
    if (result->fallback_environment_views_[0] == nullptr || result->fallback_environment_views_[1] == nullptr)
        return Err(ErrorCode::GraphicsResourceCreationFailed, "failed to create type-correct environment fallbacks");
    // Bootstrap initialization occurs before this service can submit frame work.
    abi::ShadowData no_shadows;
    TRY_VOID(result->device_->GetQueue().WriteBuffer(*result->shadow_data_buffer_, 0, &no_shadows, sizeof(no_shadows)));
    abi::EnvironmentData no_environment;
    no_environment.specular_mip_count = 1.0F;
    TRY_VOID(result->device_->GetQueue()
            .WriteBuffer(*result->environment_data_buffer_, 0, &no_environment, sizeof(no_environment)));
    result->active_shadow_atlas_ = result->fallback_shadow_view_.get();
    result->active_lights_ = result->fallback_lighting_buffer_.get();
    result->active_clusters_ = result->fallback_lighting_buffer_.get();
    result->active_indices_ = result->fallback_lighting_buffer_.get();
    result->active_environment_ = {result->fallback_environment_views_[0].get(),
        result->fallback_environment_views_[0].get(),
        result->fallback_environment_views_[0].get(),
        result->fallback_environment_views_[1].get()};
    TRY_ASSIGN(result->fallback_palette_, skin_pool.Allocate(sizeof(math::mat4f), 256));
    // The fallback allocation is likewise initialized before the first submission.
    const auto identity = math::mat4f::identity();
    TRY_VOID(result->device_->GetQueue().WriteBuffer(
        *result->fallback_palette_.buffer,
        result->fallback_palette_.offset,
        &identity,
        sizeof(identity)
    ));
    result->diagnostic_shader_ = diagnostic_shader.generation->module;
    // The diagnostic shader is a self-contained full-screen fallback and has
    // no per-object ABI binding.
    TRY_ASSIGN(result->diagnostic_layout_key_, MakePipelineLayoutKey(diagnostic_shader.Interface()));
    return Ok(std::move(result));
}

Result<void> PhysicalBindings::Prepare(
    const RenderWorldSnapshot& world,
    const RenderView& view,
    const f32 time,
    const f32 delta_time
) {
    ++binding_generation_;
    if (world.ObjectData().ids.size() > object_capacity_)
        return Err(ErrorCode::OutOfRange, "frame exceeds the physical object ABI capacity");
    abi::FrameData frame{time, delta_time, view.exposure, 0};
    abi::SceneData scene;
    scene.ambient_color = {0.035F, 0.045F, 0.065F};
    if (!view.environment_lighting)
        scene.ambient_color = {0.0F, 0.0F, 0.0F};
    for (const auto& source : world.Lights()) {
        if (source.type != LightType::Directional || scene.directional_count == 4
            || (source.visibility_mask & view.layer_mask) == 0)
            continue;
        const auto light = PackGpuLight(source, view.camera.position);
        scene.directional_lights[scene.directional_count++] = light;
    }
    abi::ViewData view_data;
    CopyMatrix(view_data.view_projection, Jittered(view.camera.view_projection, view.jitter));
    CopyMatrix(
        view_data.previous_view_projection,
        view.HistoryInvalid() ? view.camera.view_projection : view.camera.previous_view_projection
    );
    CopyMatrix(view_data.inverse_view_projection, view.camera.inverse_view_projection);
    view_data.camera_position = {view.camera.position.x, view.camera.position.y, view.camera.position.z};
    view_data.near_plane = view.camera.near_plane;
    view_data.viewport_size = {static_cast<f32>(view.viewport.width), static_cast<f32>(view.viewport.height)};
    view_data.far_plane = view.camera.far_plane;
    std::vector<std::byte> objects(static_cast<size_t>(object_stride_) * world.ObjectData().ids.size());
    for (u32 index = 0; index < world.ObjectData().ids.size(); ++index) {
        abi::ObjectData object;
        CopyMatrix(object.model, world.ObjectData().transforms[index]);
        CopyMatrix(object.previous_model, world.ObjectData().previous_transforms[index]);
        CopyMatrix(object.normal_matrix, world.ObjectData().transforms[index].inverse().transpose());
        std::memcpy(objects.data() + static_cast<size_t>(index) * object_stride_, &object, sizeof(object));
    }
    TRY_VOID(uploads_.Enqueue({.target = frame_buffer_, .offset = 0, .bytes = UploadBytes(frame), .publication = {}}));
    TRY_VOID(uploads_.Enqueue({.target = scene_buffer_, .offset = 0, .bytes = UploadBytes(scene), .publication = {}}));
    TRY_VOID(uploads_
            .Enqueue({.target = view_buffer_, .offset = 0, .bytes = UploadBytes(view_data), .publication = {}}));
    if (!objects.empty())
        TRY_VOID(uploads_
                .Enqueue({.target = object_buffer_, .offset = 0, .bytes = std::move(objects), .publication = {}}));
    used_layouts_.clear();
    used_palettes_.clear();
    active_lights_ = fallback_lighting_buffer_.get();
    active_clusters_ = fallback_lighting_buffer_.get();
    active_indices_ = fallback_lighting_buffer_.get();
    active_shadow_atlas_ = fallback_shadow_view_.get();
    active_environment_ = {fallback_environment_views_[0].get(),
        fallback_environment_views_[0].get(),
        fallback_environment_views_[0].get(),
        fallback_environment_views_[1].get()};
    abi::ShadowData no_shadows;
    TRY_VOID(device_->GetQueue().WriteBuffer(*shadow_data_buffer_, 0, &no_shadows, sizeof(no_shadows)));
    abi::EnvironmentData no_environment;
    no_environment.specular_mip_count = 1.0F;
    TRY_VOID(device_->GetQueue().WriteBuffer(*environment_data_buffer_, 0, &no_environment, sizeof(no_environment)));
    return Ok();
}

Result<void> PhysicalBindings::UseEnvironment(
    const ResolvedEnvironment& environment,
    const bool lighting_enabled,
    const bool sky_visible
) {
    ++binding_generation_;
    const auto view = [](const ResolvedTexture& texture) -> rhi::TextureView* {
        return texture.physical == nullptr ? nullptr : texture.physical->default_view.get();
    };
    const std::array resolved{view(environment.radiance),
        view(environment.irradiance),
        view(environment.prefiltered_specular),
        view(environment.brdf_lut)};
    for (u32 index = 0; index < resolved.size(); ++index)
        if (resolved[index] != nullptr)
            active_environment_[index] = resolved[index];
    const abi::EnvironmentData data{static_cast<f32>(std::max(1U, environment.specular_mip_count)),
        lighting_enabled ? 1U : 0U,
        sky_visible ? 1U : 0U,
        0U};
    return device_->GetQueue().WriteBuffer(*environment_data_buffer_, 0, &data, sizeof(data));
}

Result<void> PhysicalBindings::UseShadows(rhi::CommandEncoder&, rhi::TextureView& atlas, const abi::ShadowData& data) {
    ++binding_generation_;
    active_shadow_atlas_ = &atlas;
    return device_->GetQueue().WriteBuffer(*shadow_data_buffer_, 0, &data, sizeof(data));
}

Result<void> PhysicalBindings::UseTemporalView(
    rhi::CommandEncoder&,
    const RenderView& view,
    const math::vec2f previous_jitter
) {
    abi::ViewData data;
    CopyMatrix(data.view_projection, Jittered(view.camera.view_projection, view.jitter));
    CopyMatrix(
        data.previous_view_projection,
        view.HistoryInvalid() ? Jittered(view.camera.view_projection, view.jitter)
                              : Jittered(view.camera.previous_view_projection, previous_jitter)
    );
    CopyMatrix(data.inverse_view_projection, view.camera.inverse_view_projection);
    data.camera_position = {view.camera.position.x, view.camera.position.y, view.camera.position.z};
    data.near_plane = view.camera.near_plane;
    data.viewport_size = {static_cast<f32>(view.render_width == 0 ? view.viewport.width : view.render_width),
        static_cast<f32>(view.render_height == 0 ? view.viewport.height : view.render_height)};
    data.far_plane = view.camera.far_plane;
    return device_->GetQueue().WriteBuffer(*view_buffer_, 0, &data, sizeof(data));
}

Result<void> PhysicalBindings::UseViewProjection(
    rhi::CommandEncoder&,
    const math::mat4f& view_projection,
    const math::mat4f& inverse_view_projection,
    const math::vec3f position,
    const f32 near_plane,
    const f32 far_plane,
    const f32 width,
    const f32 height
) {
    abi::ViewData data;
    CopyMatrix(data.view_projection, view_projection);
    CopyMatrix(data.previous_view_projection, view_projection);
    CopyMatrix(data.inverse_view_projection, inverse_view_projection);
    data.camera_position = {position.x, position.y, position.z};
    data.near_plane = near_plane;
    data.viewport_size = {width, height};
    data.far_plane = far_plane;
    return device_->GetQueue().WriteBuffer(*view_buffer_, 0, &data, sizeof(data));
}

Result<void> PhysicalBindings::UseClusteredLighting(
    rhi::CommandEncoder&,
    rhi::Buffer& lights,
    rhi::Buffer& grid,
    rhi::Buffer& indices,
    const abi::ClusterParams& params
) {
    ++binding_generation_;
    active_lights_ = &lights;
    active_clusters_ = &grid;
    active_indices_ = &indices;
    return device_->GetQueue().WriteBuffer(*cluster_params_buffer_, 0, &params, sizeof(params));
}

void PhysicalBindings::UseGpuScene(const BufferSlice instances) noexcept {
    if (active_gpu_instances_.buffer == instances.buffer && active_gpu_instances_.offset == instances.offset
        && active_gpu_instances_.size == instances.size)
        return;
    active_gpu_instances_ = instances;
    ++binding_generation_;
}

Result<PhysicalBindings::Generation*> PhysicalBindings::Resolve(
    const PipelineLayoutKey& key,
    const SkinPaletteHandle palette
) {
    const SkinPalette* skin = palettes_.Get(palette);
    const auto* drawable_palette = skin == nullptr ? nullptr : skin->DrawableSlice();
    if (palette.IsValid() && drawable_palette == nullptr)
        return Err(ErrorCode::InvalidState, "skin palette is not resident");
    const BufferSlice palette_slice = drawable_palette != nullptr && drawable_palette->allocation.IsValid()
                                          ? *drawable_palette
                                          : fallback_palette_;
    const auto* drawable_previous_palette = skin == nullptr ? nullptr : skin->PreviousDrawableSlice();
    const BufferSlice previous_palette_slice = drawable_previous_palette != nullptr
                                                       && drawable_previous_palette->allocation.IsValid()
                                                   ? *drawable_previous_palette
                                                   : palette_slice;
    const auto found = std::ranges::find_if(generations_, [&](const Generation& value) {
        return value.layout_hash == key.hash && value.palette == palette
               && value.palette_allocation == palette_slice.allocation
               && value.previous_palette_allocation == previous_palette_slice.allocation
               && value.binding_generation == binding_generation_;
    });
    if (found != generations_.end())
        return Ok(&*found);
    Generation generation{.layout_hash = key.hash,
        .palette = palette,
        .palette_allocation = palette_slice.allocation,
        .previous_palette_allocation = previous_palette_slice.allocation,
        .layout = {},
        .frame = {},
        .view = {},
        .material = {},
        .object = {},
        .lights = active_lights_,
        .clusters = active_clusters_,
        .indices = active_indices_,
        .gpu_instances = active_gpu_instances_.buffer,
        .gpu_instances_offset = active_gpu_instances_.offset,
        .shadow_atlas = active_shadow_atlas_,
        .environment = active_environment_,
        .last_used = {},
        .binding_generation = binding_generation_};
    TRY_ASSIGN(generation.layout, layouts_.GetOrCreate(key));
    const auto layouts = generation.layout.BindGroupLayouts();
    if (layouts.size() != abi::kAbiGroupCount)
        return Err(ErrorCode::ValidationInvalidState, "reflected physical layout does not contain ABI groups 0-3");
    std::vector<rhi::BindGroupEntryDesc> frame_entries;
    if (HasBinding(key, abi::kFrameGroup, abi::kFrameBinding))
        frame_entries
            .push_back({.binding = abi::kFrameBinding, .buffer = frame_buffer_.get(), .size = sizeof(abi::FrameData)});
    if (HasBinding(key, abi::kFrameGroup, abi::kSceneBinding))
        frame_entries
            .push_back({.binding = abi::kSceneBinding, .buffer = scene_buffer_.get(), .size = sizeof(abi::SceneData)});
    if (HasBinding(key, abi::kFrameGroup, abi::kLocalLightsBinding))
        frame_entries.push_back({.binding = abi::kLocalLightsBinding, .buffer = active_lights_});
    if (HasBinding(key, abi::kFrameGroup, abi::kClusterGridBinding))
        frame_entries.push_back({.binding = abi::kClusterGridBinding, .buffer = active_clusters_});
    if (HasBinding(key, abi::kFrameGroup, abi::kClusterIndicesBinding))
        frame_entries.push_back({.binding = abi::kClusterIndicesBinding, .buffer = active_indices_});
    if (HasBinding(key, abi::kFrameGroup, abi::kClusterParamsBinding))
        frame_entries.push_back(
            {.binding = abi::kClusterParamsBinding,
                .buffer = cluster_params_buffer_.get(),
                .size = sizeof(abi::ClusterParams)}
        );
    if (HasBinding(key, abi::kFrameGroup, abi::kShadowAtlasBinding))
        frame_entries.push_back({.binding = abi::kShadowAtlasBinding, .texture_view = active_shadow_atlas_});
    if (HasBinding(key, abi::kFrameGroup, abi::kShadowSamplerBinding))
        frame_entries.push_back({.binding = abi::kShadowSamplerBinding, .sampler = shadow_sampler_.get()});
    if (HasBinding(key, abi::kFrameGroup, abi::kShadowDataBinding))
        frame_entries.push_back(
            {.binding = abi::kShadowDataBinding, .buffer = shadow_data_buffer_.get(), .size = sizeof(abi::ShadowData)}
        );
    if (HasBinding(key, abi::kFrameGroup, abi::kEnvironmentRadianceBinding))
        frame_entries.push_back({.binding = abi::kEnvironmentRadianceBinding, .texture_view = active_environment_[0]});
    if (HasBinding(key, abi::kFrameGroup, abi::kEnvironmentIrradianceBinding))
        frame_entries
            .push_back({.binding = abi::kEnvironmentIrradianceBinding, .texture_view = active_environment_[1]});
    if (HasBinding(key, abi::kFrameGroup, abi::kEnvironmentPrefilterBinding))
        frame_entries.push_back({.binding = abi::kEnvironmentPrefilterBinding, .texture_view = active_environment_[2]});
    if (HasBinding(key, abi::kFrameGroup, abi::kEnvironmentBrdfBinding))
        frame_entries.push_back({.binding = abi::kEnvironmentBrdfBinding, .texture_view = active_environment_[3]});
    if (HasBinding(key, abi::kFrameGroup, abi::kEnvironmentSamplerBinding))
        frame_entries.push_back({.binding = abi::kEnvironmentSamplerBinding, .sampler = environment_sampler_.get()});
    if (HasBinding(key, abi::kFrameGroup, abi::kEnvironmentDataBinding))
        frame_entries.push_back(
            {.binding = abi::kEnvironmentDataBinding,
                .buffer = environment_data_buffer_.get(),
                .size = sizeof(abi::EnvironmentData)}
        );
    TRY_ASSIGN(
        generation.frame,
        device_->CreateBindGroup(
            {.layout = layouts[abi::kFrameGroup], .entries = frame_entries, .label = "Frame ABI group"}
        )
    );
    std::vector<rhi::BindGroupEntryDesc> view_entries;
    if (HasBinding(key, abi::kViewGroup, abi::kViewBinding))
        view_entries
            .push_back({.binding = abi::kViewBinding, .buffer = view_buffer_.get(), .size = sizeof(abi::ViewData)});
    TRY_ASSIGN(
        generation.view,
        device_
            ->CreateBindGroup({.layout = layouts[abi::kViewGroup], .entries = view_entries, .label = "View ABI group"})
    );
    if (key.groups[abi::kMaterialGroup].bindings.empty())
        TRY_ASSIGN(
            generation.material,
            device_->CreateBindGroup(
                {.layout = layouts[abi::kMaterialGroup], .entries = {}, .label = "Empty material ABI group"}
            )
        );
    std::vector<rhi::BindGroupEntryDesc> object_entries;
    if (HasBinding(key, abi::kObjectGroup, abi::kObjectBinding))
        object_entries.push_back(
            {.binding = abi::kObjectBinding, .buffer = object_buffer_.get(), .size = sizeof(abi::ObjectData)}
        );
    if (HasBinding(key, abi::kObjectGroup, abi::kSkinBinding))
        object_entries.push_back(
            {.binding = abi::kSkinBinding,
                .buffer = palette_slice.buffer,
                .offset = palette_slice.offset,
                .size = palette_slice.size}
        );
    if (HasBinding(key, abi::kObjectGroup, abi::kPreviousSkinBinding))
        object_entries.push_back(
            {.binding = abi::kPreviousSkinBinding,
                .buffer = previous_palette_slice.buffer,
                .offset = previous_palette_slice.offset,
                .size = previous_palette_slice.size}
        );
    if (HasBinding(key, abi::kObjectGroup, 3)) {
        if (active_gpu_instances_.buffer == nullptr || !active_gpu_instances_.allocation.IsValid())
            return Err(
                ErrorCode::ValidationInvalidState,
                "GPU-scene instance table is unavailable for reflected material layout"
            );
        object_entries.push_back(
            {.binding = 3,
                .buffer = active_gpu_instances_.buffer,
                .offset = active_gpu_instances_.offset,
                .size = active_gpu_instances_.size}
        );
    }
    TRY_ASSIGN(
        generation.object,
        device_->CreateBindGroup(
            {.layout = layouts[abi::kObjectGroup], .entries = object_entries, .label = "Object ABI group"}
        )
    );
    generations_.push_back(std::move(generation));
    return Ok(&generations_.back());
}

Result<void> PhysicalBindings::DrawDiagnostic(
    rhi::RenderPassEncoder& encoder,
    const PipelineTargetSignature& targets,
    const u32 object_index
) {
    if (targets.colors.empty())
        return Ok();
    static_cast<void>(object_index);
    Generation* generation{};
    TRY_ASSIGN(generation, Resolve(diagnostic_layout_key_, {}));
    auto found = std::ranges::find_if(diagnostic_pipelines_, [&](const DiagnosticPipeline& value) {
        return value.vertex_schema_id == 0 && value.targets == targets;
    });
    if (found == diagnostic_pipelines_.end())
        return Ok();
    encoder.SetPipeline(*found->pipeline);
    encoder.SetBindGroup(abi::kFrameGroup, generation->frame.get());
    encoder.SetBindGroup(abi::kViewGroup, generation->view.get());
    encoder.SetBindGroup(abi::kMaterialGroup, generation->material.get());
    encoder.SetBindGroup(abi::kObjectGroup, generation->object.get());
    encoder.Draw(3);
    used_layouts_.push_back(generation->layout);
    return Ok();
}

Result<void> PhysicalBindings::DrawFallbackMesh(
    rhi::RenderPassEncoder& encoder,
    const PipelineTargetSignature& targets,
    const VertexSchema& schema,
    const MeshResident& resident,
    const u32 lod,
    const u32 object_index,
    const SkinPaletteHandle palette,
    const u32 first_index,
    const u32 index_count,
    const i32 vertex_offset
) {
    static_cast<void>(object_index);
    Generation* generation{};
    TRY_ASSIGN(generation, Resolve(diagnostic_layout_key_, palette));
    const bool skinned = palette.IsValid();
    auto found = std::ranges::find_if(diagnostic_pipelines_, [&](const DiagnosticPipeline& value) {
        return value.vertex_schema_id == schema.id && value.skinned == skinned && value.targets == targets;
    });
    if (found == diagnostic_pipelines_.end())
        return Ok();
    encoder.SetPipeline(*found->pipeline);
    encoder.SetBindGroup(abi::kFrameGroup, generation->frame.get());
    encoder.SetBindGroup(abi::kViewGroup, generation->view.get());
    encoder.SetBindGroup(abi::kMaterialGroup, generation->material.get());
    encoder.SetBindGroup(abi::kObjectGroup, generation->object.get());
    encoder
        .SetVertexBuffer(0, resident.vertices[lod].buffer, resident.vertices[lod].offset, resident.vertices[lod].size);
    encoder.SetIndexBuffer(
        *resident.indices[lod].buffer,
        rhi::IndexFormat::Uint32,
        resident.indices[lod].offset,
        resident.indices[lod].size
    );
    encoder.DrawIndexed(index_count, 1, first_index, vertex_offset, 0);
    used_layouts_.push_back(generation->layout);
    if (palette.IsValid())
        used_palettes_.push_back(palette);
    return Ok();
}

Result<BorrowedLayout> PhysicalBindings::Bind(
    rhi::RenderPassEncoder& encoder,
    const PreparedMaterial& material,
    const u32 object_index,
    const SkinPaletteHandle palette
) {
    if (object_index >= object_capacity_)
        return Err(ErrorCode::OutOfRange, "draw object index exceeds ABI capacity");
    Generation* generation{};
    TRY_ASSIGN(generation, Resolve(material.layout, palette));
    encoder.SetBindGroup(abi::kFrameGroup, generation->frame.get());
    encoder.SetBindGroup(abi::kViewGroup, generation->view.get());
    encoder.SetBindGroup(abi::kMaterialGroup, material.group.get());
    const u32 dynamic_offset = object_index * object_stride_;
    encoder.SetBindGroup(abi::kObjectGroup, generation->object.get(), std::span(&dynamic_offset, 1));
    used_layouts_.push_back(generation->layout);
    if (palette.IsValid())
        used_palettes_.push_back(palette);
    return Ok(generation->layout);
}

Result<BorrowedLayout> PhysicalBindings::PrepareLayout(
    const PipelineLayoutKey& layout,
    const SkinPaletteHandle palette
) {
    Generation* generation{};
    TRY_ASSIGN(generation, Resolve(layout, palette));
    return Ok(generation->layout);
}

Result<void> PhysicalBindings::PrepareDiagnostic(const PipelineTargetSignature& targets) {
    if (targets.colors.empty() || std::ranges::any_of(diagnostic_pipelines_, [&](const DiagnosticPipeline& value) {
            return value.vertex_schema_id == 0 && value.targets == targets;
        }))
        return Ok();
    Generation* generation{};
    TRY_ASSIGN(generation, Resolve(diagnostic_layout_key_, {}));
    std::vector<rhi::ColorTargetStateDesc> colors;
    for (const auto format : targets.colors)
        colors.push_back({.format = format});
    const rhi::VertexStateDesc vertex{.module = diagnostic_shader_.get(), .entry_point = "error_vs"};
    const rhi::FragmentStateDesc fragment{.module = diagnostic_shader_.get(),
        .entry_point = "error_fs",
        .targets = colors};
    const rhi::PrimitiveStateDesc primitive{.topology = rhi::PrimitiveTopology::TriangleList,
        .front_face = rhi::FrontFace::CCW,
        .cull_mode = rhi::CullMode::None};
    const rhi::DepthStencilStateDesc depth{.format = targets.depth,
        .depth_write_enabled = false,
        .depth_compare = rhi::CompareFunction::LessEqual};
    auto pipeline = device_->CreateRenderPipeline(
        {.layout = &generation->layout.Pipeline(),
            .vertex = &vertex,
            .primitive = &primitive,
            .depth_stencil = targets.depth == rhi::TextureFormat::Undefined ? nullptr : &depth,
            .multisample = {.count = targets.samples},
            .fragment = &fragment,
            .label = "Missing asset diagnostic"}
    );
    if (!pipeline)
        return Err(std::move(pipeline).error());
    diagnostic_pipelines_.push_back({targets, 0, false, ref<rhi::RenderPipeline>(std::move(*pipeline)), {}});
    return Ok();
}

Result<void> PhysicalBindings::PrepareFallback(
    const PipelineTargetSignature& targets,
    const VertexSchema& schema,
    const SkinPaletteHandle palette
) {
    const bool skinned = palette.IsValid();
    if (std::ranges::any_of(diagnostic_pipelines_, [&](const DiagnosticPipeline& value) {
            return value.vertex_schema_id == schema.id && value.skinned == skinned && value.targets == targets;
        }))
        return Ok();
    Generation* generation{};
    TRY_ASSIGN(generation, Resolve(diagnostic_layout_key_, palette));
    std::vector<std::vector<rhi::VertexAttributeDesc>> attributes(schema.streams.size());
    std::vector<rhi::VertexBufferLayoutDesc> streams;
    for (u32 stream_index = 0; stream_index < schema.streams.size(); ++stream_index) {
        for (const auto& attribute : schema.streams[stream_index].attributes)
            attributes[stream_index].push_back(
                {.format = detail::ToRhiVertexFormat(attribute.format),
                    .offset = attribute.offset,
                    .shader_location = attribute.location}
            );
        streams.push_back(
            {.step_mode = schema.streams[stream_index].step_mode == VertexStepMode::Instance
                              ? rhi::VertexStepMode::Instance
                              : rhi::VertexStepMode::Vertex,
                .array_stride = schema.streams[stream_index].stride,
                .attributes = attributes[stream_index]}
        );
    }
    const rhi::VertexStateDesc vertex{
        .module = diagnostic_shader_.get(), .entry_point = "error_vs", .buffers = streams};
    std::vector<rhi::ColorTargetStateDesc> colors;
    for (const auto format : targets.colors)
        colors.push_back({.format = format});
    const rhi::FragmentStateDesc fragment{.module = diagnostic_shader_.get(),
        .entry_point = "error_fs",
        .targets = colors};
    const rhi::PrimitiveStateDesc primitive{.topology = rhi::PrimitiveTopology::TriangleList,
        .front_face = rhi::FrontFace::CCW,
        .cull_mode = rhi::CullMode::None};
    const rhi::DepthStencilStateDesc depth{.format = targets.depth,
        .depth_write_enabled = false,
        .depth_compare = rhi::CompareFunction::LessEqual};
    auto pipeline = device_->CreateRenderPipeline(
        {.layout = &generation->layout.Pipeline(),
            .vertex = &vertex,
            .primitive = &primitive,
            .depth_stencil = targets.depth == rhi::TextureFormat::Undefined ? nullptr : &depth,
            .multisample = {.count = targets.samples},
            .fragment = &fragment,
            .label = "Fallback mesh material"}
    );
    if (!pipeline)
        return Err(std::move(pipeline).error());
    diagnostic_pipelines_.push_back({targets, schema.id, skinned, ref<rhi::RenderPipeline>(std::move(*pipeline)), {}});
    return Ok();
}

Result<void> PhysicalBindings::BindViewGroups(rhi::RenderPassEncoder& encoder, const PipelineLayoutKey& layout) {
    Generation* generation{};
    TRY_ASSIGN(generation, Resolve(layout, {}));
    encoder.SetBindGroup(abi::kFrameGroup, generation->frame.get());
    encoder.SetBindGroup(abi::kViewGroup, generation->view.get());
    used_layouts_.push_back(generation->layout);
    return Ok();
}

void PhysicalBindings::MarkUsed(const rhi::SubmissionTicket submission) {
    for (const auto& layout : used_layouts_)
        layouts_.MarkUsed(layout, submission);
    for (const auto palette : used_palettes_)
        palettes_.MarkUsed(palette, submission);
    skin_pool_.MarkUsed(submission);
    for (auto& generation : generations_)
        if (generation.last_used < submission)
            generation.last_used = submission;
    for (auto& pipeline : diagnostic_pipelines_)
        if (pipeline.last_used < submission)
            pipeline.last_used = submission;
}

} // namespace woki::gfx
