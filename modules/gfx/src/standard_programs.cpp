#include <array>
#include <map>

#include <woki/config.hpp>

#include <woki/gfx/advanced/standard_features.hpp>
#include <woki/rhi/compute_pass_encoder.hpp>
#include <woki/rhi/device.hpp>
#include <woki/rhi/render_pass_encoder.hpp>

namespace woki::gfx {
namespace {

using Json = config::Json;

struct ProgramSpec final {
    const char* name;
    const char* vertex;
    const char* fragment;
};

constexpr std::array kFullscreenPrograms{
    ProgramSpec{"sky", "sky_vs", "sky_fs"},
    ProgramSpec{"bloom-threshold", "bloom_vs", "bloom_threshold_fs"},
    ProgramSpec{"bloom-downsample", "bloom_vs", "bloom_downsample_fs"},
    ProgramSpec{"bloom-blur", "bloom_vs", "bloom_blur_fs"},
    ProgramSpec{"bloom-composite", "bloom_vs", "bloom_composite_fs"},
    ProgramSpec{"taa", "taa_vs", "taa_fs"},
    ProgramSpec{"fxaa", "fxaa_vs", "fxaa_fs"},
    ProgramSpec{"copy", "copy_vs", "copy_fs"},
    ProgramSpec{"tone-map", "tone_map_vs", "tone_map_fs"},
};

constexpr std::array<const char*, 10> kRequiredMaterialPrograms{"error",
    "depth",
    "pbr",
    "unlit",
    "copy",
    "cube",
    "bitmap-text",
    "msdf-text",
    "sdf-text",
    "validate-compute"};

Result<asset::Product> ReadProduct(const asset::Vfs& vfs, const std::string& path) {
    auto uri = asset::AssetUri::Parse("engine://cooked/" + path);
    if (!uri)
        return Err(std::move(uri).error());
    std::vector<std::byte> bytes;
    TRY_ASSIGN(bytes, vfs.ReadBinary(*uri, 256U * 1024U * 1024U));
    return asset::ParseProduct(bytes, {.max_payload_bytes = 256U * 1024U * 1024U});
}

class CookedStandardFeaturePrograms final : public StandardFeaturePrograms {
public:
    struct Shader final {
        asset::Product product;
        ShaderAssetHandle handle;
        BorrowedShader shader;
        PipelineLayoutKey layout_key;
        BorrowedLayout layout;
    };

    struct FullscreenPipeline final {
        FullscreenProgram program{};
        rhi::TextureFormat format{rhi::TextureFormat::Undefined};
        u32 samples{1};
        ref<rhi::RenderPipeline> pipeline;
    };

    static Result<ref<const StandardFeaturePrograms>> Create(
        ref<rhi::Device> device,
        std::map<std::string, asset::Product> products,
        const u64 version = 1
    ) {
        if (device == nullptr)
            return Err(ErrorCode::InvalidArgument, "standard feature programs require a device");
        auto generation = createRef<CookedStandardFeaturePrograms>(std::move(device), std::move(products), version);
        TRY_VOID(generation->Prepare());
        return Ok(ref<const StandardFeaturePrograms>(std::move(generation)));
    }

    CookedStandardFeaturePrograms(
        ref<rhi::Device> device,
        std::map<std::string, asset::Product> products,
        const u64 version
    )
        : device_(std::move(device)),
          products_(std::move(products)),
          shaders_(*device_),
          layouts_(*device_),
          version_(version) {}

    Result<ref<const StandardFeaturePrograms>> PrepareReplacement(ref<rhi::Device> device) const override {
        return Create(std::move(device), products_, version_ + 1);
    }

    Result<void> DrawFullscreen(RenderGraphContext& graph, const FullscreenDraw& draw) const override {
        auto* encoder = graph.RenderEncoder();
        if (encoder == nullptr)
            return Err(ErrorCode::ValidationInvalidState, "fullscreen program requires a render pass");
        auto pipeline = std::ranges::find_if(fullscreen_, [&](const FullscreenPipeline& value) {
            return value.program == draw.program && value.format == draw.target_format
                   && value.samples == draw.sample_count;
        });
        if (pipeline == fullscreen_.end())
            return Err(ErrorCode::GraphicsResourceCreationFailed, "cooked fullscreen pipeline target was not prepared");
        const auto& shader = ShaderFor(draw.program);
        encoder->SetPipeline(*pipeline->pipeline);
        if (draw.program == FullscreenProgram::Sky) {
            const auto groups = shader.layout.BindGroupLayouts();
            for (u32 group = 0; group < 2; ++group)
                encoder->SetBindGroup(group, empty_groups_.at(shader.product.asset_id).at(group).get());
            std::reference_wrapper<rhi::TextureView> depth = std::ref(*fallback_depth_view_);
            if (draw.tertiary)
                TRY_ASSIGN(depth, graph.TextureView(draw.tertiary));
            std::array entries{
                rhi::BindGroupEntryDesc{.binding = 0,
                    .texture_view = draw.external_source == nullptr ? fallback_cube_view_.get() : draw.external_source},
                rhi::BindGroupEntryDesc{.binding = 1, .sampler = sampler_.get()},
                rhi::BindGroupEntryDesc{.binding = 2, .texture_view = &depth.get()}};
            ref<rhi::BindGroup> group;
            TRY_ASSIGN(
                group,
                device_->CreateBindGroup({.layout = groups[2], .entries = entries, .label = "Visible sky environment"})
            );
            encoder->SetBindGroup(2, group.get());
        } else {
            std::reference_wrapper<rhi::TextureView> source = std::ref(*fallback_view_);
            if (draw.source)
                TRY_ASSIGN(source, graph.TextureView(draw.source));
            std::reference_wrapper<rhi::TextureView> secondary = std::ref(*fallback_view_);
            if (draw.secondary)
                TRY_ASSIGN(secondary, graph.TextureView(draw.secondary));
            std::reference_wrapper<rhi::TextureView> tertiary = std::ref(*fallback_depth_view_);
            if (draw.tertiary)
                TRY_ASSIGN(tertiary, graph.TextureView(draw.tertiary));
            std::reference_wrapper<rhi::TextureView> quaternary = std::ref(*fallback_view_);
            if (draw.quaternary)
                TRY_ASSIGN(quaternary, graph.TextureView(draw.quaternary));

            const auto extent = draw.source ? graph.Extent(draw.source) : rhi::Extent3D{1, 1, 1};
            abi::PostData data{};
            data.parameters = draw.parameters;
            data.texel_size = {1.0F / std::max(1U, extent.width), 1.0F / std::max(1U, extent.height)};
            TRY_VOID(graph.CommandEncoder()
                    .WriteBuffer(*post_buffer_, 0, reinterpret_cast<const u8*>(&data), sizeof(data)));
            const auto groups = shader.layout.BindGroupLayouts();
            for (u32 group = 0; group < 2; ++group)
                encoder->SetBindGroup(group, empty_groups_.at(shader.product.asset_id).at(group).get());
            std::array entries{
                rhi::BindGroupEntryDesc{.binding = 0, .buffer = post_buffer_.get(), .size = sizeof(data)},
                rhi::BindGroupEntryDesc{.binding = 1, .texture_view = &source.get()},
                rhi::BindGroupEntryDesc{.binding = 2, .sampler = sampler_.get()},
                rhi::BindGroupEntryDesc{.binding = 3, .texture_view = &secondary.get()},
                rhi::BindGroupEntryDesc{.binding = 4, .texture_view = &tertiary.get()},
                rhi::BindGroupEntryDesc{.binding = 5, .texture_view = &quaternary.get()},
                rhi::BindGroupEntryDesc{.binding = 6,
                    .texture_view = draw.grading_lut == nullptr ? fallback_grading_view_.get() : draw.grading_lut},
            };
            ref<rhi::BindGroup> group;
            TRY_ASSIGN(
                group,
                device_->CreateBindGroup({.layout = groups[2], .entries = entries, .label = "Standard post bindings"})
            );
            encoder->SetBindGroup(2, group.get());
        }
        encoder->Draw(3);
        return Ok();
    }

    Result<void> DispatchExposure(
        RenderGraphContext& graph,
        rhi::TextureView& source,
        rhi::Buffer& histogram,
        rhi::Buffer& exposure,
        const ExposureSettings& settings,
        const f32 delta_time,
        const u32 width,
        const u32 height,
        const bool reduce
    ) const override {
        auto* encoder = graph.ComputeEncoder();
        if (encoder == nullptr)
            return Err(ErrorCode::ValidationInvalidState, "exposure program requires a compute pass");

        struct alignas(16) Params final {
            std::array<u32, 2> dimensions;
            f32 delta_time;
            f32 key;
            std::array<f32, 2> limits;
            std::array<f32, 2> speeds;
        };

        const Params params{{width, height},
            delta_time,
            settings.key,
            {settings.minimum, settings.maximum},
            {settings.speed_up, settings.speed_down}};
        TRY_VOID(graph.CommandEncoder()
                .WriteBuffer(*exposure_params_, 0, reinterpret_cast<const u8*>(&params), sizeof(params)));
        const auto& shader = programs_.at("exposure");
        std::array entries{
            rhi::BindGroupEntryDesc{.binding = 0, .buffer = exposure_params_.get(), .size = sizeof(params)},
            rhi::BindGroupEntryDesc{.binding = 1, .texture_view = &source},
            rhi::BindGroupEntryDesc{.binding = 2, .buffer = &histogram},
            rhi::BindGroupEntryDesc{.binding = 3, .buffer = &exposure}};
        ref<rhi::BindGroup> group;
        TRY_ASSIGN(
            group,
            device_->CreateBindGroup(
                {.layout = shader.layout.BindGroupLayouts()[0],
                    .entries = entries,
                    .label = "Exposure compute bindings"}
            )
        );
        encoder->SetPipeline(reduce ? *exposure_reduce_pipeline_ : *exposure_histogram_pipeline_);
        encoder->SetBindGroup(0, group.get());
        if (reduce)
            encoder->DispatchWorkgroups(1);
        else
            encoder->DispatchWorkgroups((width + 7U) / 8U, (height + 7U) / 8U);
        return Ok();
    }

    Result<void> DispatchClusters(
        RenderGraphContext& graph,
        rhi::Buffer& lights,
        rhi::Buffer& clusters,
        rhi::Buffer& indices,
        rhi::Buffer& diagnostics,
        const abi::ClusterParams& params,
        const u32 cluster_count,
        const u32 light_count
    ) const override {
        auto* encoder = graph.ComputeEncoder();
        if (encoder == nullptr)
            return Err(ErrorCode::ValidationInvalidState, "cluster program requires a compute pass");
        auto dispatch_params = params;
        dispatch_params.depth[3] = static_cast<f32>(light_count);
        const u32 zero{};
        TRY_VOID(graph.CommandEncoder().WriteBuffer(diagnostics, 0, reinterpret_cast<const u8*>(&zero), sizeof(zero)));
        TRY_VOID(graph.CommandEncoder().WriteBuffer(
            *cluster_params_,
            0,
            reinterpret_cast<const u8*>(&dispatch_params),
            sizeof(dispatch_params)
        ));
        const auto& shader = programs_.at("cluster-lights");
        std::array entries{
            rhi::BindGroupEntryDesc{.binding = 0, .buffer = cluster_params_.get(), .size = sizeof(dispatch_params)},
            rhi::BindGroupEntryDesc{.binding = 1, .buffer = &lights},
            rhi::BindGroupEntryDesc{.binding = 2, .buffer = &clusters},
            rhi::BindGroupEntryDesc{.binding = 3, .buffer = &indices},
            rhi::BindGroupEntryDesc{.binding = 4, .buffer = &diagnostics},
        };
        ref<rhi::BindGroup> group;
        TRY_ASSIGN(
            group,
            device_->CreateBindGroup(
                {.layout = shader.layout.BindGroupLayouts()[0],
                    .entries = entries,
                    .label = "Cluster assignment bindings"}
            )
        );
        encoder->SetPipeline(*cluster_pipeline_);
        encoder->SetBindGroup(0, group.get());
        encoder->DispatchWorkgroups(cluster_count);
        return Ok();
    }

    bool GpuDrivenReady() const noexcept override {
        return hiz_seed_pipeline_ != nullptr && hiz_downsample_pipeline_ != nullptr && visibility_pipeline_ != nullptr;
    }

    Result<void> DispatchHiZ(
        RenderGraphContext& graph,
        rhi::TextureView& source,
        rhi::TextureView& destination,
        const u32 width,
        const u32 height,
        const bool depth_source
    ) const override {
        auto* encoder = graph.ComputeEncoder();
        if (encoder == nullptr || !GpuDrivenReady())
            return Err(ErrorCode::ValidationInvalidState, "Hi-Z dispatch requires prepared compute programs");
        const std::array<u32, 4> params{width, height, 0, 0};
        TRY_VOID(graph.CommandEncoder()
                .WriteBuffer(*hiz_params_, 0, reinterpret_cast<const u8*>(params.data()), sizeof(params)));
        const auto& shader = programs_.at(depth_source ? "hiz-seed" : "hiz-downsample");
        std::array entries{rhi::BindGroupEntryDesc{.binding = 0, .buffer = hiz_params_.get(), .size = sizeof(params)},
            rhi::BindGroupEntryDesc{.binding = 1, .texture_view = &source},
            rhi::BindGroupEntryDesc{.binding = 2, .texture_view = &destination}};
        ref<rhi::BindGroup> group;
        TRY_ASSIGN(
            group,
            device_->CreateBindGroup(
                {.layout = shader.layout.BindGroupLayouts()[0],
                    .entries = entries,
                    .label = depth_source ? "Hi-Z depth seed" : "Hi-Z mip downsample"}
            )
        );
        encoder->SetPipeline(depth_source ? *hiz_seed_pipeline_ : *hiz_downsample_pipeline_);
        encoder->SetBindGroup(0, group.get());
        encoder->DispatchWorkgroups((width + 7U) / 8U, (height + 7U) / 8U);
        return Ok();
    }

    Result<void> DispatchVisibility(
        RenderGraphContext& graph,
        rhi::Buffer& candidates,
        rhi::Buffer& lods,
        rhi::Buffer& meshlet_bounds,
        rhi::TextureView& previous_hiz,
        rhi::Buffer& commands,
        rhi::Buffer& counts,
        rhi::Buffer& visible_instances,
        rhi::Buffer& visible_meshlets,
        rhi::Buffer& diagnostics,
        const GpuVisibilityParams& params
    ) const override {
        auto* encoder = graph.ComputeEncoder();
        if (encoder == nullptr || !GpuDrivenReady())
            return Err(ErrorCode::ValidationInvalidState, "GPU visibility requires prepared compute programs");
        std::vector<u32> zero_counts(params.counts[0], 0U);
        std::vector<std::array<u32, 2>> invisible_instances(params.counts[0], {~u32{0}, ~u32{0}});
        const std::array<u32, 13> zero_diagnostics{};
        if (!zero_counts.empty())
            TRY_VOID(graph.CommandEncoder().WriteBuffer(
                counts,
                0,
                reinterpret_cast<const u8*>(zero_counts.data()),
                zero_counts.size() * sizeof(u32)
            ));
        if (!invisible_instances.empty())
            TRY_VOID(graph.CommandEncoder().WriteBuffer(
                visible_instances,
                0,
                reinterpret_cast<const u8*>(invisible_instances.data()),
                invisible_instances.size() * sizeof(invisible_instances.front())
            ));
        TRY_VOID(graph.CommandEncoder().WriteBuffer(
            diagnostics,
            0,
            reinterpret_cast<const u8*>(zero_diagnostics.data()),
            sizeof(zero_diagnostics)
        ));
        TRY_VOID(graph.CommandEncoder()
                .WriteBuffer(*visibility_params_, 0, reinterpret_cast<const u8*>(&params), sizeof(params)));
        const auto& shader = programs_.at("gpu-visibility");
        std::array entries{
            rhi::BindGroupEntryDesc{.binding = 0, .buffer = visibility_params_.get(), .size = sizeof(params)},
            rhi::BindGroupEntryDesc{.binding = 1, .buffer = &candidates},
            rhi::BindGroupEntryDesc{.binding = 2, .buffer = &lods},
            rhi::BindGroupEntryDesc{.binding = 3, .buffer = &meshlet_bounds},
            rhi::BindGroupEntryDesc{.binding = 4, .texture_view = &previous_hiz},
            rhi::BindGroupEntryDesc{.binding = 5, .buffer = &commands},
            rhi::BindGroupEntryDesc{.binding = 6, .buffer = &counts},
            rhi::BindGroupEntryDesc{.binding = 7, .buffer = &visible_instances},
            rhi::BindGroupEntryDesc{.binding = 8, .buffer = &visible_meshlets},
            rhi::BindGroupEntryDesc{.binding = 9, .buffer = &diagnostics},
        };
        ref<rhi::BindGroup> group;
        TRY_ASSIGN(
            group,
            device_->CreateBindGroup(
                {.layout = shader.layout.BindGroupLayouts()[0],
                    .entries = entries,
                    .label = "GPU visibility and immutable-bin classification"}
            )
        );
        encoder->SetPipeline(*visibility_pipeline_);
        encoder->SetBindGroup(0, group.get());
        encoder->DispatchWorkgroups((params.counts[0] + 63U) / 64U);
        return Ok();
    }

    void OnSubmitted(const rhi::SubmissionTicket submission) const override {
        last_submission_ = std::max(last_submission_, submission);
    }

    Result<MaterialShaderIdentity> ResolveMaterialShader(const asset::AssetId shader) const override {
        const auto found = identities_.find(shader);
        if (found == identities_.end())
            return Err(ErrorCode::FileNotFound, "material references a shader outside the cooked manifest");
        return Ok(found->second);
    }

    Result<BorrowedShader> BorrowShader(const ContentHash product, const u64 generation) const override {
        return shaders_.BorrowProduct(product, generation);
    }

    const asset::Product& DiagnosticShaderProduct() const noexcept override {
        return programs_.at("error").product;
    }

    const asset::Product* ProgramProduct(const std::string_view name) const noexcept override {
        const auto found = programs_.find(std::string(name));
        return found == programs_.end() ? nullptr : &found->second.product;
    }

private:
    const Shader& ShaderFor(const FullscreenProgram program) const {
        return programs_.at(kFullscreenPrograms[static_cast<size_t>(program)].name);
    }

    Result<void> Prepare() {
        for (auto& [name, product] : products_) {
            if (product.type != kShaderProductType)
                return Err(
                    ErrorCode::ParseInvalidFormat,
                    "standard program manifest contains a non-shader product: " + name
                );
            Shader entry{.product = product, .handle = shaders_.Create(), .shader = {}, .layout_key = {}, .layout = {}};
            TRY_VOID(shaders_.Publish(entry.handle, entry.product));
            TRY_ASSIGN(entry.shader, shaders_.Borrow(entry.handle));
            TRY_ASSIGN(entry.layout_key, MakePipelineLayoutKey(entry.shader.Interface()));
            TRY_ASSIGN(entry.layout, layouts_.GetOrCreate(entry.layout_key));
            identities_.emplace(
                product.asset_id,
                MaterialShaderIdentity{{},
                    product.product_hash,
                    entry.shader.generation->payload.variant_hash,
                    entry.shader.Version(),
                    entry.shader.Interface().hash}
            );
            programs_.emplace(name, std::move(entry));
        }
        for (const auto& spec : kFullscreenPrograms)
            if (!programs_.contains(spec.name))
                return Err(
                    ErrorCode::FileNotFound,
                    std::string("required standard shader product is missing: ") + spec.name
                );
        if (!programs_.contains("cluster-lights"))
            return Err(ErrorCode::FileNotFound, "required standard shader product is missing: cluster-lights");
        if (!programs_.contains("exposure"))
            return Err(ErrorCode::FileNotFound, "required standard shader product is missing: exposure");
        if (!programs_.contains("canvas"))
            return Err(ErrorCode::FileNotFound, "required standard shader product is missing: canvas");
        for (const char* name : {"gpu-visibility", "hiz-seed", "hiz-downsample"})
            if (!programs_.contains(name))
                return Err(
                    ErrorCode::FileNotFound,
                    std::string("required GPU-driven shader product is missing: ") + name
                );
        for (const char* name : kRequiredMaterialPrograms)
            if (!programs_.contains(name))
                return Err(
                    ErrorCode::FileNotFound,
                    std::string("required standard shader product is missing: ") + name
                );

        TRY_ASSIGN(
            post_buffer_,
            device_->CreateBuffer(
                {.size = 32,
                    .usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::CopyDst,
                    .label = "Standard post parameters"}
            )
        );
        TRY_ASSIGN(
            dummy_uniform_,
            device_->CreateBuffer(
                {.size = 1024,
                    .usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::CopyDst,
                    .label = "Standard bootstrap uniforms"}
            )
        );
        TRY_ASSIGN(
            cluster_params_,
            device_->CreateBuffer(
                {.size = sizeof(abi::ClusterParams),
                    .usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::CopyDst,
                    .label = "Cluster parameters"}
            )
        );
        TRY_ASSIGN(
            exposure_params_,
            device_->CreateBuffer(
                {.size = 32,
                    .usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::CopyDst,
                    .label = "Exposure parameters"}
            )
        );
        TRY_ASSIGN(
            hiz_params_,
            device_->CreateBuffer(
                {.size = 16, .usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::CopyDst, .label = "Hi-Z parameters"}
            )
        );
        TRY_ASSIGN(
            visibility_params_,
            device_->CreateBuffer(
                {.size = sizeof(GpuVisibilityParams),
                    .usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::CopyDst,
                    .label = "GPU visibility parameters"}
            )
        );
        TRY_ASSIGN(
            sampler_,
            device_->CreateSampler(
                {.mag_filter = rhi::FilterMode::Linear,
                    .min_filter = rhi::FilterMode::Linear,
                    .mipmap_filter = rhi::MipmapFilterMode::Linear,
                    .label = "Standard post sampler"}
            )
        );
        TRY_ASSIGN(
            fallback_texture_,
            device_->CreateTexture(
                {.size = {1, 1, 1},
                    .format = rhi::TextureFormat::RGBA8Unorm,
                    .usage = rhi::TextureUsage::TextureBinding | rhi::TextureUsage::CopyDst,
                    .label = "Standard post fallback"}
            )
        );
        fallback_view_ = ref<rhi::TextureView>(fallback_texture_
                ->CreateView({.format = rhi::TextureFormat::RGBA8Unorm, .label = "Standard post fallback view"})
                .release());
        if (fallback_view_ == nullptr)
            return Err(ErrorCode::GraphicsResourceCreationFailed, "failed to create standard fallback texture view");
        TRY_ASSIGN(
            fallback_depth_texture_,
            device_->CreateTexture(
                {.size = {1, 1, 1},
                    .format = rhi::TextureFormat::Depth32Float,
                    .usage = rhi::TextureUsage::TextureBinding | rhi::TextureUsage::RenderAttachment,
                    .label = "Standard depth fallback"}
            )
        );
        fallback_depth_view_ = ref<rhi::TextureView>(fallback_depth_texture_
                ->CreateView(
                    {.format = rhi::TextureFormat::Depth32Float,
                        .aspect = rhi::TextureAspect::DepthOnly,
                        .label = "Standard depth fallback view"}
                )
                .release());
        TRY_ASSIGN(
            fallback_grading_texture_,
            device_->CreateTexture(
                {.size = {1, 1, 1},
                    .dimension = rhi::TextureDimension::e3D,
                    .format = rhi::TextureFormat::RGBA8Unorm,
                    .usage = rhi::TextureUsage::TextureBinding | rhi::TextureUsage::CopyDst,
                    .label = "Identity grading fallback"}
            )
        );
        fallback_grading_view_ = ref<rhi::TextureView>(fallback_grading_texture_
                ->CreateView(
                    {.format = rhi::TextureFormat::RGBA8Unorm,
                        .dimension = rhi::TextureViewDimension::e3D,
                        .label = "Identity grading fallback view"}
                )
                .release());
        if (fallback_depth_view_ == nullptr || fallback_grading_view_ == nullptr)
            return Err(ErrorCode::GraphicsResourceCreationFailed, "failed to create typed post-process fallbacks");
        TRY_ASSIGN(
            fallback_cube_texture_,
            device_->CreateTexture(
                {.size = {1, 1, 6},
                    .format = rhi::TextureFormat::RGBA8Unorm,
                    .usage = rhi::TextureUsage::TextureBinding | rhi::TextureUsage::CopyDst,
                    .label = "Visible sky fallback cube"}
            )
        );
        fallback_cube_view_ = ref<rhi::TextureView>(fallback_cube_texture_
                ->CreateView(
                    {.format = rhi::TextureFormat::RGBA8Unorm,
                        .dimension = rhi::TextureViewDimension::Cube,
                        .mip_level_count = 1,
                        .array_layer_count = 6,
                        .label = "Visible sky fallback cube view"}
                )
                .release());
        if (fallback_cube_view_ == nullptr)
            return Err(ErrorCode::GraphicsResourceCreationFailed, "failed to create sky cube fallback");
        constexpr std::array formats{rhi::TextureFormat::BGRA8Unorm,
            rhi::TextureFormat::RGBA8Unorm,
            rhi::TextureFormat::RGBA16Float};
        for (u32 index = 0; index < kFullscreenPrograms.size(); ++index) {
            const auto& spec = kFullscreenPrograms[index];
            auto& shader = programs_.at(spec.name);
            std::vector<ref<rhi::BindGroup>> empty;
            const auto group_layouts = shader.layout.BindGroupLayouts();
            for (u32 group_index = 0; group_index < group_layouts.size(); ++group_index) {
                auto* layout = group_layouts[group_index];
                std::vector<rhi::BindGroupEntryDesc> entries;
                bool bootstrap_supported = true;
                if (group_index < shader.layout_key.groups.size()) {
                    for (const auto& binding : shader.layout_key.groups[group_index].bindings) {
                        if (binding.kind == ResourceKind::UniformBuffer) {
                            entries.push_back(
                                {.binding = binding.binding,
                                    .buffer = dummy_uniform_.get(),
                                    .size = std::max<u64>(16, binding.min_binding_size)}
                            );
                        } else {
                            bootstrap_supported = false;
                        }
                    }
                }
                ref<rhi::BindGroup> group;
                if (bootstrap_supported) {
                    auto created = device_->CreateBindGroup(
                        {.layout = layout, .entries = entries, .label = "Standard bootstrap group"}
                    );
                    if (!created)
                        return Err(std::move(created).error());
                    group = ref<rhi::BindGroup>(std::move(*created));
                }
                empty.push_back(std::move(group));
            }
            empty_groups_.emplace(shader.product.asset_id, std::move(empty));
            for (const auto format : formats)
                for (const u32 samples : {1U, 4U}) {
                    const rhi::VertexStateDesc vertex{.module = &shader.shader.Module(), .entry_point = spec.vertex};
                    const std::array targets{rhi::ColorTargetStateDesc{.format = format}};
                    const rhi::FragmentStateDesc fragment{.module = &shader.shader.Module(),
                        .entry_point = spec.fragment,
                        .targets = targets};
                    const rhi::PrimitiveStateDesc primitive{};
                    scope<rhi::RenderPipeline> pipeline;
                    TRY_ASSIGN(
                        pipeline,
                        device_->CreateRenderPipeline(
                            {.layout = &shader.layout.Pipeline(),
                                .vertex = &vertex,
                                .primitive = &primitive,
                                .multisample = {.count = samples},
                                .fragment = &fragment,
                                .label = std::string("Standard ") + spec.name}
                        )
                    );
                    fullscreen_.push_back(
                        {static_cast<FullscreenProgram>(index),
                            format,
                            samples,
                            ref<rhi::RenderPipeline>(std::move(pipeline))}
                    );
                }
        }
        auto& cluster = programs_.at("cluster-lights");
        scope<rhi::ComputePipeline> compute;
        TRY_ASSIGN(
            compute,
            device_->CreateComputePipeline(
                {.layout = &cluster.layout.Pipeline(),
                    .compute = {.module = &cluster.shader.Module(), .entry_point = "assign_clusters"},
                    .label = "Cluster assignment"}
            )
        );
        cluster_pipeline_ = ref<rhi::ComputePipeline>(std::move(compute));
        auto& exposure = programs_.at("exposure");
        TRY_ASSIGN(
            compute,
            device_->CreateComputePipeline(
                {.layout = &exposure.layout.Pipeline(),
                    .compute = {.module = &exposure.shader.Module(), .entry_point = "luminance_histogram_cs"},
                    .label = "Luminance histogram"}
            )
        );
        exposure_histogram_pipeline_ = ref<rhi::ComputePipeline>(std::move(compute));
        TRY_ASSIGN(
            compute,
            device_->CreateComputePipeline(
                {.layout = &exposure.layout.Pipeline(),
                    .compute = {.module = &exposure.shader.Module(), .entry_point = "exposure_reduce_cs"},
                    .label = "Adapted exposure reduction"}
            )
        );
        exposure_reduce_pipeline_ = ref<rhi::ComputePipeline>(std::move(compute));
        auto& hiz_seed = programs_.at("hiz-seed");
        TRY_ASSIGN(
            compute,
            device_->CreateComputePipeline(
                {.layout = &hiz_seed.layout.Pipeline(),
                    .compute = {.module = &hiz_seed.shader.Module(), .entry_point = "hiz_seed"},
                    .label = "Hi-Z depth seed"}
            )
        );
        hiz_seed_pipeline_ = ref<rhi::ComputePipeline>(std::move(compute));
        auto& hiz_downsample = programs_.at("hiz-downsample");
        TRY_ASSIGN(
            compute,
            device_->CreateComputePipeline(
                {.layout = &hiz_downsample.layout.Pipeline(),
                    .compute = {.module = &hiz_downsample.shader.Module(), .entry_point = "hiz_downsample"},
                    .label = "Hi-Z downsample"}
            )
        );
        hiz_downsample_pipeline_ = ref<rhi::ComputePipeline>(std::move(compute));
        auto& visibility = programs_.at("gpu-visibility");
        TRY_ASSIGN(
            compute,
            device_->CreateComputePipeline(
                {.layout = &visibility.layout.Pipeline(),
                    .compute = {.module = &visibility.shader.Module(), .entry_point = "cull_and_classify"},
                    .label = "GPU visibility and classification"}
            )
        );
        visibility_pipeline_ = ref<rhi::ComputePipeline>(std::move(compute));
        return Ok();
    }

    ref<rhi::Device> device_;
    std::map<std::string, asset::Product> products_;
    ShaderLibrary shaders_;
    LayoutCache layouts_;
    u64 version_{};
    std::map<std::string, Shader> programs_;
    std::map<asset::AssetId, MaterialShaderIdentity> identities_;
    std::map<asset::AssetId, std::vector<ref<rhi::BindGroup>>> empty_groups_;
    std::vector<FullscreenPipeline> fullscreen_;
    ref<rhi::ComputePipeline> cluster_pipeline_;
    ref<rhi::ComputePipeline> exposure_histogram_pipeline_;
    ref<rhi::ComputePipeline> exposure_reduce_pipeline_;
    ref<rhi::ComputePipeline> hiz_seed_pipeline_;
    ref<rhi::ComputePipeline> hiz_downsample_pipeline_;
    ref<rhi::ComputePipeline> visibility_pipeline_;
    ref<rhi::Buffer> post_buffer_;
    ref<rhi::Buffer> dummy_uniform_;
    ref<rhi::Buffer> cluster_params_;
    ref<rhi::Buffer> exposure_params_;
    ref<rhi::Buffer> hiz_params_;
    ref<rhi::Buffer> visibility_params_;
    ref<rhi::Sampler> sampler_;
    ref<rhi::Texture> fallback_texture_;
    ref<rhi::TextureView> fallback_view_;
    ref<rhi::Texture> fallback_depth_texture_;
    ref<rhi::TextureView> fallback_depth_view_;
    ref<rhi::Texture> fallback_grading_texture_;
    ref<rhi::TextureView> fallback_grading_view_;
    ref<rhi::Texture> fallback_cube_texture_;
    ref<rhi::TextureView> fallback_cube_view_;
    mutable rhi::SubmissionTicket last_submission_;
};

} // namespace

Result<ref<const StandardFeaturePrograms>> LoadStandardFeaturePrograms(
    const asset::Vfs& vfs,
    ref<rhi::Device> device,
    const asset::AssetUri& manifest
) {
    std::string text;
    TRY_ASSIGN(text, vfs.ReadText(manifest, 1024U * 1024U));
    auto parsed = Json::Parse(text, manifest.String(), config::ParsePolicy::Strict());
    if (!parsed || !parsed->is_object() || parsed->value("schema", 0) != 1 || !parsed->contains("shaderProducts")
        || !(*parsed)["shaderProducts"].is_object())
        return Err(ErrorCode::ParseInvalidFormat, "cooked manifest has no versioned shaderProducts object");
    const Json json = std::move(*parsed);
    std::map<std::string, asset::Product> products;
    for (const auto& [name, value] : json["shaderProducts"].items()) {
        if (!value.is_string())
            return Err(ErrorCode::ParseInvalidFormat, "cooked manifest has an invalid or duplicate shader product");
        asset::Product product;
        TRY_ASSIGN(product, ReadProduct(vfs, value.get<std::string>()));
        if (!products.emplace(name, std::move(product)).second)
            return Err(ErrorCode::ParseInvalidFormat, "cooked manifest has an invalid or duplicate shader product");
    }
    return CookedStandardFeaturePrograms::Create(std::move(device), std::move(products));
}

} // namespace woki::gfx
