#include "../internal/runtime_state.hpp"

namespace woki::gfx {

Result<void> RenderRuntime::Impl::RecoverDevice() {
    if (recovery_factory == nullptr)
        return Err(ErrorCode::GraphicsDeviceLost, "device recovery requires a RenderDeviceFactory");
    recovery_state = RenderRecoveryState::Recovering;
    RenderDeviceBundle replacement;
    TRY_ASSIGN(replacement, recovery_factory->CreateDevice());
    if (replacement.device == nullptr)
        return Err(ErrorCode::GraphicsDeviceLost, "recovery factory returned no replacement device");
    auto validation_buffer = replacement.device->CreateBuffer({.size = 16, .usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::CopyDst, .label = "Recovery diagnostic"});
    if (!validation_buffer)
        return Err(ErrorCode::GraphicsResourceCreationFailed, "replacement device failed minimal resource validation");
    scope<RenderRuntimeServices> next_resources;
    TRY_ASSIGN(next_resources, RenderRuntimeServices::Create(replacement.device, resource_descriptor));
    scope<TextureLibrary> next_textures;
    TRY_ASSIGN(next_textures, textures->PrepareReplacement(replacement.device, next_resources->Uploads(), next_resources->Releases()));
    scope<MeshLibrary> next_meshes;
    TRY_ASSIGN(next_meshes, meshes->PrepareReplacement(*next_resources));
    scope<SkinPaletteRegistry> next_palettes;
    TRY_ASSIGN(next_palettes, palettes->PrepareReplacement(*next_resources->Pool(BufferPoolClass::Skinning), next_resources->Uploads()));
    ShaderLibrary next_shaders(*replacement.device);
    const auto next_diagnostic_shader = next_shaders.Create();
    if (next_diagnostic_shader != diagnostic_shader)
        return Err(ErrorCode::InvalidState, "replacement shader library changed a logical handle");
    TRY_VOID(next_shaders.Publish(next_diagnostic_shader, diagnostic_shader_product));
    LayoutCache next_layouts(*replacement.device);
    PipelineCache next_pipeline_cache;
    MaterialBindingCache next_material_bindings(replacement.device);
    auto next_preparation = createScope<MaterialPreparation>(*materials, *next_textures, *next_resources->Pool(BufferPoolClass::Material), next_resources->Uploads(), next_layouts, next_material_bindings,
        resolve_material_shader);
    auto next_world = createScope<RenderWorldServices>(*next_resources->Pool(BufferPoolClass::GpuScene), next_resources->Uploads());
    scope<PhysicalBindings> next_bindings;
    ref<const StandardFeaturePrograms> next_programs;
    TRY_ASSIGN(next_programs, feature_services.programs->PrepareReplacement(replacement.device));
    const auto* canvas_product = next_programs->ProgramProduct("canvas");
    if (canvas_product == nullptr)
        return Err(ErrorCode::FileNotFound, "replacement standard programs have no canvas shader");
    BorrowedShader diagnostic_generation;
    TRY_ASSIGN(diagnostic_generation, next_shaders.Borrow(next_diagnostic_shader));
    TRY_ASSIGN(next_bindings, PhysicalBindings::Create(replacement.device, next_layouts, *next_palettes, *next_resources->Pool(BufferPoolClass::Skinning), next_resources->Uploads(), std::move(diagnostic_generation)));
    auto next_feature_readbacks = createScope<ReadbackManager>(replacement.device);
    auto next_histories = createScope<RenderHistoryRegistry>(replacement.device, next_resources->Releases());
    scope<CanvasFeature> next_canvas;
    TRY_ASSIGN(next_canvas, CanvasFeature::Create(replacement.device, next_resources->Uploads(), next_resources->Releases(), *canvas_product));

    std::vector<OffscreenSlot> next_offscreens(offscreen_slots.size());
    for (u32 index = 0; index < offscreen_slots.size(); ++index) {
        const auto& live = offscreen_slots[index];
        auto& candidate = next_offscreens[index];
        candidate.generation = live.generation;
        candidate.descriptor = live.descriptor;
        candidate.version = live.version;
        if (!candidate.descriptor)
            continue;
        const auto& desc = *candidate.descriptor;
        rhi::TextureFormat format = rhi::TextureFormat::RGBA8Unorm;
        if (desc.format == PixelFormat::BGRA8Unorm)
            format = rhi::TextureFormat::BGRA8Unorm;
        else if (desc.format == PixelFormat::RGBA8UnormSrgb)
            format = rhi::TextureFormat::RGBA8UnormSrgb;
        else if (desc.format == PixelFormat::RGBA16Float)
            format = rhi::TextureFormat::RGBA16Float;
        else if (desc.format == PixelFormat::R32Uint)
            format = rhi::TextureFormat::R32Uint;
        scope<rhi::Texture> texture;
        TRY_ASSIGN(texture, replacement.device->CreateTexture({.size = {desc.width, desc.height, 1},
                                .mip_level_count = 1,
                                .sample_count = desc.sample_count,
                                .format = format,
                                .usage = rhi::TextureUsage::RenderAttachment | rhi::TextureUsage::TextureBinding | rhi::TextureUsage::CopySrc | rhi::TextureUsage::CopyDst,
                                .label = desc.label}));
        candidate.texture = ref<rhi::Texture>(std::move(texture));
        candidate.view = ref<rhi::TextureView>(candidate.texture->CreateView());
        ++candidate.version;
    }
    std::vector<SurfaceSlot> next_surfaces(surface_slots.size());
    for (u32 index = 0; index < surface_slots.size(); ++index) {
        const auto& live = surface_slots[index];
        auto& candidate = next_surfaces[index];
        candidate.generation = live.generation;
        candidate.descriptor = live.descriptor;
        if (live.surface == nullptr)
            continue;
        TRY_ASSIGN(candidate.surface, CreatePlatformSurface(*replacement.instance, *candidate.descriptor.source));
        if (candidate.descriptor.width == 0 || candidate.descriptor.height == 0) {
            candidate.state = SurfaceState::Minimized;
            continue;
        }
        TRY_ASSIGN(candidate.swapchain, rhi::Swapchain::Builder(replacement.device, candidate.surface)
                                            .Size(candidate.descriptor.width, candidate.descriptor.height)
                                            .PresentMode(ToRhiPresentMode(candidate.descriptor.present_mode))
                                            .Label(candidate.descriptor.label)
                                            .Build());
        candidate.state = SurfaceState::Ready;
    }

    // Every fallible reconstruction completed against the candidate device.
    // The remaining operations form the atomic service-generation switch.
    executables.clear();
    feature_pool.clear();
    resources = std::move(next_resources);
    textures = std::move(next_textures);
    meshes = std::move(next_meshes);
    palettes = std::move(next_palettes);
    shaders = std::move(next_shaders);
    layouts = std::move(next_layouts);
    pipeline_cache = std::move(next_pipeline_cache);
    material_bindings = std::move(next_material_bindings);
    material_preparation = std::move(next_preparation);
    world = std::move(next_world);
    bindings = std::move(next_bindings);
    feature_services.programs = std::move(next_programs);
    instance = std::move(replacement.instance);
    adapter = std::move(replacement.adapter);
    device = std::move(replacement.device);
    feature_services.materials = material_preparation.get();
    feature_services.meshes = meshes.get();
    feature_services.pipelines = &pipeline_cache;
    feature_services.visibility = &world->visibility;
    feature_services.bindings = bindings.get();
    feature_services.textures = textures.get();
    feature_services.gpu_scene = world->gpu_scene.get();
    feature_services.device = device;
    feature_services.releases = resources->Releases();
    RuntimeFacadeAccess::AttachComputeReleases(*compute, resources->Releases());
    feature_readbacks = std::move(next_feature_readbacks);
    feature_services.readbacks = feature_readbacks.get();
    histories = std::move(next_histories);
    feature_services.histories = histories.get();
    canvas = std::move(next_canvas);
    offscreen_slots = std::move(next_offscreens);
    surface_slots = std::move(next_surfaces);
    recovery_state = RenderRecoveryState::Ready;
    return Ok();
}

void RenderRuntime::Impl::MarkDeviceLost() noexcept {
    if (recovery_state != RenderRecoveryState::Ready)
        return;
    recovery_state = RenderRecoveryState::DeviceLost;
    executables.clear();
    meshes->MarkDeviceLost();
    textures->MarkDeviceLost();
    if (histories != nullptr)
        histories->MarkDeviceLost();
    if (feature_readbacks != nullptr)
        feature_readbacks->MarkDeviceLost();
    world->gpu_scene->MarkDeviceLost();
    resources->MarkDeviceLost();
    if (canvas != nullptr)
        canvas->MarkDeviceLost();
    RuntimeFacadeAccess::DeviceLostCompute(*compute);
    RuntimeFacadeAccess::DeviceLostReadbacks(*readback_service);
}

Result<void> RenderRuntime::Impl::RestoreOffscreen(OffscreenSlot& target) {
    const auto& descriptor = *target.descriptor;
    rhi::TextureFormat format = rhi::TextureFormat::RGBA8Unorm;
    switch (descriptor.format) {
        case PixelFormat::BGRA8Unorm:
            format = rhi::TextureFormat::BGRA8Unorm;
            break;
        case PixelFormat::RGBA8Unorm:
            format = rhi::TextureFormat::RGBA8Unorm;
            break;
        case PixelFormat::RGBA8UnormSrgb:
            format = rhi::TextureFormat::RGBA8UnormSrgb;
            break;
        case PixelFormat::RGBA16Float:
            format = rhi::TextureFormat::RGBA16Float;
            break;
        case PixelFormat::R32Uint:
            format = rhi::TextureFormat::R32Uint;
            break;
    }
    scope<rhi::Texture> texture;
    TRY_ASSIGN(texture, device->CreateTexture({.size = {descriptor.width, descriptor.height, 1},
                            .mip_level_count = 1,
                            .sample_count = descriptor.sample_count,
                            .format = format,
                            .usage = rhi::TextureUsage::RenderAttachment | rhi::TextureUsage::TextureBinding | rhi::TextureUsage::CopySrc | rhi::TextureUsage::CopyDst,
                            .label = descriptor.label}));
    target.texture = ref<rhi::Texture>(std::move(texture));
    target.view = ref<rhi::TextureView>(target.texture->CreateView());
    ++target.version;
    return Ok();
}

} // namespace woki::gfx
