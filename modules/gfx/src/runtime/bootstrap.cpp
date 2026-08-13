#include "../internal/runtime_state.hpp"

namespace woki::gfx {

rhi::BackendType ToRhiBackend(const RenderBackend backend) {
    switch (backend) {
        case RenderBackend::Automatic:
            return rhi::BackendType::Undefined;
        case RenderBackend::WebGPU:
            return rhi::BackendType::WebGPU;
        case RenderBackend::Vulkan:
            return rhi::BackendType::Vulkan;
        case RenderBackend::Metal:
            return rhi::BackendType::Metal;
        case RenderBackend::D3D12:
            return rhi::BackendType::D3D12;
        case RenderBackend::OpenGL:
            return rhi::BackendType::OpenGL;
        case RenderBackend::Null:
            return rhi::BackendType::Null;
    }
    return rhi::BackendType::Undefined;
}

std::string BackendName(const rhi::BackendType backend) {
    switch (backend) {
        case rhi::BackendType::WebGPU:
            return "WebGPU";
        case rhi::BackendType::Vulkan:
            return "Vulkan";
        case rhi::BackendType::Metal:
            return "Metal";
        case rhi::BackendType::D3D12:
            return "D3D12";
        case rhi::BackendType::D3D11:
            return "D3D11";
        case rhi::BackendType::OpenGL:
            return "OpenGL";
        case rhi::BackendType::OpenGLES:
            return "OpenGLES";
        case rhi::BackendType::Null:
            return "Null";
        default:
            return "Automatic";
    }
}

rhi::PresentMode ToRhiPresentMode(const PresentMode mode) {
    switch (mode) {
        case PresentMode::Fifo:
            return rhi::PresentMode::Fifo;
        case PresentMode::Mailbox:
            return rhi::PresentMode::Mailbox;
        case PresentMode::Immediate:
            return rhi::PresentMode::Immediate;
    }
    return rhi::PresentMode::Fifo;
}

Result<ref<rhi::Surface>> CreatePlatformSurface(rhi::Instance& instance, const SurfaceSource& source) {
    const auto platform = source.Describe();
    if (platform.platform != SurfacePlatform::WokiWindow || platform.window == nullptr)
        return Err(
            ErrorCode::GraphicsUnsupportedApi,
            "surface platform contract is not supported by the selected runtime backend"
        );
    return instance.CreateSurface(*static_cast<Window*>(platform.window));
}

Result<RenderDeviceBundle> CreateDefaultDevice(
    const RenderRuntimeDescriptor& descriptor,
    rhi::DeviceLostCallback device_lost = {}
) {
    RenderDeviceBundle result;
    TRY_ASSIGN(
        result.instance,
        rhi::Instance::Create(
            {.enable_validation = descriptor.validation != ValidationMode::Disabled, .label = "Woki RenderRuntime"}
        )
    );
    TRY_ASSIGN(result.adapter, result.instance->RequestAdapter({.backend_type = ToRhiBackend(descriptor.backend)}));
    rhi::DeviceDesc device_descriptor{.label = "Woki RenderRuntime device"};
    device_descriptor.device_lost_callback = std::move(device_lost);
    device_descriptor.uncaptured_error_callback = [](const rhi::ErrorType type, const std::string_view message) {
        slog::Error("WebGPU device error ({}): {}", static_cast<u32>(type), message);
    };
    TRY_ASSIGN(result.device, result.adapter->CreateDevice(device_descriptor));
    if (descriptor.validation != ValidationMode::Disabled)
        result.device = rhi::CreateValidationDevice(
            std::move(result.device),
            {
                .check_thread_ownership = true,
                .full = descriptor.validation == ValidationMode::Full,
                .diagnostic = {},
            }
        );
    return Ok(std::move(result));
}

class DefaultRenderDeviceFactory final : public RenderDeviceFactory {
public:
    explicit DefaultRenderDeviceFactory(const RenderRuntimeDescriptor& descriptor)
        : backend_(descriptor.backend),
          validation_(descriptor.validation) {}

    Result<RenderDeviceBundle> CreateDevice() override {
        lost_->store(false, std::memory_order_release);
        RenderRuntimeDescriptor descriptor;
        descriptor.backend = backend_;
        descriptor.validation = validation_;
        return CreateDefaultDevice(descriptor, [lost = lost_](rhi::DeviceLostReason, std::string_view) {
            lost->store(true, std::memory_order_release);
        });
    }

    bool DeviceLost() const noexcept override {
        return lost_->load(std::memory_order_acquire);
    }

private:
    RenderBackend backend_;
    ValidationMode validation_;
    ref<std::atomic_bool> lost_{createRef<std::atomic_bool>(false)};
};

struct OwnedAssetBootstrap final {
    ref<asset::Vfs> vfs{createRef<asset::Vfs>()};
    asset::AssetDatabase database;
    scope<asset::ProductCache> cache;
    std::filesystem::path manifest;
    std::mutex manifest_mutex;
    asset::DependencyGraph dependencies;
    asset::BuilderRegistry builders;
    scope<asset::AssetManager> manager;
    scope<task::Scheduler> scheduler;
    scope<task::IoExecutor> io;
    scope<task::CompletionQueue> completions;
    asset::AssetServices services;
    asset::AssetManifest shipping_manifest;
    asset::ProductCacheDescriptor cache_descriptor;
};

Result<asset::Product> ReadPackagedProduct(
    const asset::Vfs& vfs,
    const asset::AssetManifest& manifest,
    const asset::AssetId id
) {
    const auto* entry = manifest.Resolve(id);
    if (entry == nullptr)
        return Err(ErrorCode::FileNotFound, "asset ID is absent from the shipping manifest");
    auto opened = asset::ProductReader::Open(
        vfs,
        entry->locator,
        {.max_payload_bytes = 1024ULL * 1024ULL * 1024ULL, .max_decompressed_chunk_bytes = 1024ULL * 1024ULL * 1024ULL}
    );
    if (!opened)
        return Err(std::move(opened).error());
    auto product = opened->ReadProduct();
    if (!product)
        return product;
    if (product->product_hash != entry->product_hash || product->type != entry->type)
        return Err(ErrorCode::ParseInvalidFormat, "manifest entry does not match its product");
    return product;
}

Result<std::pair<ref<OwnedAssetBootstrap>, ref<RenderRuntimeAdvancedDescriptor>>> CreatePackagedAssets(
    const RenderRuntimeDescriptor& descriptor,
    ref<rhi::Device> device
) {
    if (descriptor.assets.roots.empty())
        return Err(ErrorCode::InvalidArgument, "RenderRuntime requires at least one packaged asset root");
    auto owned = createRef<OwnedAssetBootstrap>();
    owned->scheduler = createScope<task::Scheduler>(descriptor.worker_count);
    owned->io = createScope<task::IoExecutor>(descriptor.io_worker_count);
    owned->completions = createScope<task::CompletionQueue>();
    for (u32 index = 0; index < descriptor.assets.roots.size(); ++index) {
        ref<asset::DirectoryMount> mount;
        TRY_ASSIGN(mount, asset::DirectoryMount::Create(descriptor.assets.roots[index]));
        TRY_VOID(owned->vfs->MountAt(
            "runtime-" + std::to_string(index),
            asset::AssetScheme::Engine,
            {},
            static_cast<i32>(index),
            mount
        ));
    }
    auto shipping_manifest_uri = asset::AssetUri::Parse("engine://cooked/manifest.wkam");
    if (!shipping_manifest_uri)
        return Err(shipping_manifest_uri.error());
    TRY_ASSIGN(owned->shipping_manifest, asset::AssetManifest::Load(*owned->vfs, *shipping_manifest_uri));
    if (!descriptor.assets.cache.empty()) {
        owned->cache_descriptor = {descriptor.assets.cache,
            descriptor.assets.target,
            descriptor.assets.platform,
            descriptor.assets.capability_fingerprint};
        auto cache = asset::ProductCache::Create(owned->cache_descriptor);
        if (!cache)
            return Err(std::move(cache).error());
        owned->cache = createScope<asset::ProductCache>(std::move(*cache));
        owned->manifest = descriptor.assets.manifest.empty() ? descriptor.assets.cache / "assets.woki-manifest"
                                                             : descriptor.assets.manifest;
        if (std::filesystem::exists(owned->manifest))
            TRY_VOID(owned->database.Load(owned->manifest));
    } else if (!descriptor.assets.manifest.empty()) {
        owned->manifest = descriptor.assets.manifest;
        if (std::filesystem::exists(owned->manifest))
            TRY_VOID(owned->database.Load(owned->manifest));
    }
    if (!owned->manifest.empty() && !owned->manifest.parent_path().empty()) {
        std::error_code manifest_error;
        std::filesystem::create_directories(owned->manifest.parent_path(), manifest_error);
        if (manifest_error)
            return Err(ErrorCode::FileAccessDenied, "asset manifest directory is unavailable");
    }
    auto vfs = owned->vfs;
    owned->manager = createScope<asset::AssetManager>(
        [vfs, owned_ptr = owned.get()](const asset::AssetId id, task::CancellationToken) -> Result<asset::Product> {
            if (owned_ptr->cache != nullptr)
                if (const auto record = owned_ptr->database.Find(id); record) {
                    const auto* entry = owned_ptr->shipping_manifest.Resolve(id);
                    auto cached = entry == nullptr
                                      ? Result<asset::Product>(
                                            Err(ErrorCode::FileNotFound, "asset is absent from shipping manifest")
                                        )
                                      : owned_ptr->cache->Load(
                                            {owned_ptr->cache_descriptor.target,
                                                owned_ptr->cache_descriptor.platform,
                                                entry->capability_fingerprint,
                                                record->product_type,
                                                record->product_hash}
                                        );
                    if (cached)
                        return cached;
                }
            asset::Product product;
            TRY_ASSIGN(product, ReadPackagedProduct(*vfs, owned_ptr->shipping_manifest, id));
            if (owned_ptr->cache != nullptr) {
                const auto* entry = owned_ptr->shipping_manifest.Resolve(id);
                if (entry == nullptr)
                    return Err(ErrorCode::FileNotFound, "asset is absent from shipping manifest");
                TRY_VOID(owned_ptr->cache->Store(
                    {owned_ptr->cache_descriptor.target,
                        owned_ptr->cache_descriptor.platform,
                        entry->capability_fingerprint,
                        product.type,
                        product.product_hash},
                    product
                ));
            }
            auto uri = asset::AssetUri::Parse("engine://runtime/" + id.String());
            if (!uri)
                return Err(std::move(uri).error());
            std::lock_guard manifest_lock(owned_ptr->manifest_mutex);
            auto transaction = owned_ptr->database.BeginTransaction();
            TRY_VOID(transaction
                    .Upsert({id, std::move(*uri), 0, product.type, product.source_hash, product.product_hash, 0, 1}));
            TRY_VOID(transaction.Commit());
            if (!owned_ptr->manifest.empty())
                TRY_VOID(owned_ptr->database.SaveAtomic(owned_ptr->manifest));
            return Ok(std::move(product));
        }
    );
    owned->services = {.vfs = owned->vfs.get(),
        .database = &owned->database,
        .cache = owned->cache.get(),
        .manager = owned->manager.get(),
        .dependencies = &owned->dependencies,
        .builders = &owned->builders};
    auto advanced = createRef<RenderRuntimeAdvancedDescriptor>();
    advanced->assets = owned->services;
    advanced->scheduler = owned->scheduler.get();
    advanced->io = owned->io.get();
    advanced->completions = owned->completions.get();
    advanced->stop_requests = [owned_ptr = owned.get()] {
        owned_ptr->scheduler->RequestStop();
        owned_ptr->io->RequestStop();
        owned_ptr->completions->RequestStop();
    };
    advanced->join_workers = [owned_ptr = owned.get()] {
        owned_ptr->scheduler->Join();
        owned_ptr->io->Join();
    };
    advanced->flush_asset_manifest = [owned] {
        std::lock_guard lock(owned->manifest_mutex);
        return owned->manifest.empty() ? Ok() : owned->database.SaveAtomic(owned->manifest);
    };
    const auto manifest = asset::AssetUri::Parse("engine://cooked/programs.json");
    if (!manifest)
        return Err(manifest.error());
    TRY_ASSIGN(advanced->standard_feature_programs, LoadStandardFeaturePrograms(*owned->vfs, device, *manifest));
    advanced->resolve_material_shader = [programs = advanced->standard_feature_programs](const asset::AssetId shader) {
        return programs->ResolveMaterialShader(shader);
    };
    advanced->diagnostic_shader_product = advanced->standard_feature_programs->DiagnosticShaderProduct();
    return Ok(std::pair{std::move(owned), std::move(advanced)});
}

RenderRuntimeDesc WithDefaultPools(RenderRuntimeDesc descriptor) {
    if (!descriptor.buffer_pools.empty())
        return descriptor;
    descriptor.buffer_pools = {
        DefaultBufferPoolDesc(BufferPoolClass::Vertex, 128U * 1024U * 1024U),
        DefaultBufferPoolDesc(BufferPoolClass::Index, 64U * 1024U * 1024U),
        DefaultBufferPoolDesc(BufferPoolClass::Material, 16U * 1024U * 1024U),
        DefaultBufferPoolDesc(BufferPoolClass::GpuScene, 32U * 1024U * 1024U),
        DefaultBufferPoolDesc(BufferPoolClass::Skinning, 32U * 1024U * 1024U),
        DefaultBufferPoolDesc(BufferPoolClass::Indirect, 16U * 1024U * 1024U),
        DefaultBufferPoolDesc(BufferPoolClass::Dynamic, 32U * 1024U * 1024U),
    };
    return descriptor;
}

RenderRuntime::Impl::Impl(
    BootstrapDescriptor descriptor,
    scope<RenderRuntimeServices> runtime,
    scope<TextureLibrary> texture_library
)
    : instance(std::move(descriptor.device.instance)),
      adapter(std::move(descriptor.device.adapter)),
      device(std::move(descriptor.device.device)),
      recovery_factory(std::move(descriptor.recovery_factory)),
      owned_assets(std::move(descriptor.owned_assets)),
      assets(descriptor.advanced->assets),
      scheduler(descriptor.advanced->scheduler),
      io(descriptor.advanced->io),
      completions(descriptor.advanced->completions),
      capabilities(descriptor.advanced->feature_capabilities),
      upload_failure_policy(descriptor.advanced->upload_failure_policy),
      material_failure_policy(descriptor.advanced->material_failure_policy),
      resource_descriptor(descriptor.advanced->resources),
      resources(std::move(runtime)),
      shaders(*device),
      diagnostic_shader_product(descriptor.advanced->diagnostic_shader_product),
      layouts(*device),
      textures(std::move(texture_library)),
      materials(createScope<MaterialLibrary>(*assets.manager)),
      material_bindings(device),
      resolve_material_shader(descriptor.advanced->resolve_material_shader) {
    stop_requests = descriptor.advanced->stop_requests;
    join_workers = descriptor.advanced->join_workers;
    drain_publications = descriptor.advanced->drain_publications;
    flush_asset_manifest = descriptor.advanced->flush_asset_manifest;
    wait_for_shutdown_submission = descriptor.advanced->wait_for_shutdown_submission;
    auto* material_pool = resources->Pool(BufferPoolClass::Material);
    material_preparation = createScope<MaterialPreparation>(
        *materials,
        *textures,
        *material_pool,
        resources->Uploads(),
        layouts,
        material_bindings,
        resolve_material_shader
    );
    meshes = createScope<MeshLibrary>(*assets.manager, *resources);
    palettes = createScope<SkinPaletteRegistry>(*resources->Pool(BufferPoolClass::Skinning), resources->Uploads());
    world = createScope<RenderWorldServices>(*resources->Pool(BufferPoolClass::GpuScene), resources->Uploads());
    feature_services = {.meshes = meshes.get(),
        .materials = material_preparation.get(),
        .material_library = materials.get(),
        .pipelines = &pipeline_cache,
        .visibility = &world->visibility,
        .scheduler = scheduler,
        .frame = &frame_state,
        .bindings = bindings.get(),
        .readbacks = nullptr,
        .textures = textures.get(),
        .histories = histories.get(),
        .gpu_scene = world->gpu_scene.get(),
        .resolve_environment = descriptor.advanced->resolve_environment,
        .grading_lut = descriptor.advanced->grading_lut,
        .capabilities = &capabilities,
        .programs = descriptor.advanced->standard_feature_programs,
        .device = device,
        .releases = resources->Releases(),
        .create_graphics_pipeline = {}};
    feature_readbacks = createScope<ReadbackManager>(device);
    feature_services.readbacks = feature_readbacks.get();
    if (!feature_services.create_graphics_pipeline)
        feature_services.create_graphics_pipeline = [this](
                                                        const GraphicsPipelineKey& key,
                                                        rhi::PipelineLayout& layout,
                                                        const VertexSchema& schema,
                                                        const MaterialRenderState& state
                                                    ) { return CreateGraphicsPipeline(key, layout, schema, state); };
}

Result<scope<RenderRuntime>> RenderRuntime::Create(RenderRuntimeDescriptor descriptor) {
    if (descriptor.frames_in_flight == 0 || descriptor.frames_in_flight > 8)
        return Err(ErrorCode::InvalidArgument, "frames_in_flight must be between one and eight");
    ref<RenderDeviceFactory> device_factory = descriptor.device_factory;
    if (device_factory == nullptr)
        device_factory = createRef<DefaultRenderDeviceFactory>(descriptor);
    RenderDeviceBundle device;
    TRY_ASSIGN(device, device_factory->CreateDevice());
    if (device.instance == nullptr || device.adapter == nullptr || device.device == nullptr)
        return Err(ErrorCode::GraphicsInitFailed, "device factory returned an incomplete runtime device bundle");
    ref<void> owned_assets;
    auto advanced = descriptor.advanced;
    if (advanced != nullptr)
        advanced = createRef<RenderRuntimeAdvancedDescriptor>(*advanced);
    if (advanced == nullptr) {
        std::pair<ref<OwnedAssetBootstrap>, ref<RenderRuntimeAdvancedDescriptor>> packaged;
        TRY_ASSIGN(packaged, CreatePackagedAssets(descriptor, device.device));
        owned_assets = packaged.first;
        advanced = std::move(packaged.second);
    }
    if (advanced->assets.manager == nullptr || !advanced->resolve_material_shader
        || advanced->standard_feature_programs == nullptr
        || advanced->diagnostic_shader_product.type != kShaderProductType)
        return Err(
            ErrorCode::InvalidArgument,
            "advanced runtime bootstrap is missing its asset manager or cooked standard programs"
        );
    advanced->resources.frames_in_flight = descriptor.frames_in_flight;
    advanced->resources = WithDefaultPools(std::move(advanced->resources));
    scope<RenderRuntimeServices> resources;
    TRY_ASSIGN(resources, RenderRuntimeServices::Create(device.device, advanced->resources));
    if (resources->Pool(BufferPoolClass::Material) == nullptr || resources->Pool(BufferPoolClass::GpuScene) == nullptr
        || resources->Pool(BufferPoolClass::Vertex) == nullptr || resources->Pool(BufferPoolClass::Index) == nullptr
        || resources->Pool(BufferPoolClass::Skinning) == nullptr)
        return Err(
            ErrorCode::InvalidArgument,
            "RenderRuntime requires material, GPU-scene, vertex, index, and skinning pools"
        );
    scope<TextureLibrary> textures;
    TRY_ASSIGN(
        textures,
        TextureLibrary::Create(
            *advanced->assets.manager,
            device.device,
            resources->Uploads(),
            resources->Releases(),
            advanced->texture_budget
        )
    );
    auto impl = createScope<Impl>(
        BootstrapDescriptor{std::move(device), std::move(device_factory), advanced, std::move(owned_assets)},
        std::move(resources),
        std::move(textures)
    );
    impl->compute = createScope<ComputeService>();
    RuntimeFacadeAccess::AttachComputeReleases(*impl->compute, impl->resources->Releases());
    impl->readback_service = createScope<ReadbackService>();
    const auto& normalized = impl->device->Capabilities();
    const auto& limits = normalized.GetLimits();
    impl->public_capabilities.compute = normalized.Has(rhi::CapabilityFeature::Compute);
    impl->public_capabilities.timestamp_queries = normalized.Has(rhi::CapabilityFeature::TimestampQueries);
    impl->public_capabilities.texture_compression = !normalized.Compression().empty();
    impl->public_capabilities.maximum_texture_dimension_2d = limits.max_texture_dimension_2d;
    impl->public_capabilities.maximum_buffer_size = limits.max_buffer_size;
    const auto add_capability = [&](std::string_view name) {
        const StringId id(name);
        if (std::ranges::find(impl->capabilities, id) == impl->capabilities.end())
            impl->capabilities.push_back(id);
    };
    if (normalized.Format(rhi::TextureFormat::Depth24Plus) != nullptr)
        add_capability("depth-texture");
    if (normalized.Supports(rhi::TextureFormat::RGBA16Float, rhi::TextureUsage::RenderAttachment))
        add_capability("hdr");
    if (normalized.Has(rhi::CapabilityFeature::TimestampQueries))
        add_capability("timestamp-query");
    if (normalized.Has(rhi::CapabilityFeature::IndirectDraw))
        add_capability("indirect-draw");
    if (normalized.Has(rhi::CapabilityFeature::IndirectCount))
        add_capability("indirect-count");
    std::ranges::sort(impl->capabilities);
    impl->capabilities.erase(std::ranges::unique(impl->capabilities).begin(), impl->capabilities.end());
    const auto adapter_info = impl->adapter->GetInfo();
    impl->diagnostics.backend_.backend_name = BackendName(adapter_info.backend_type);
    impl->diagnostics.backend_
        .adapter_name = adapter_info.description.empty() ? adapter_info.device : adapter_info.description;
    impl->histories = createScope<RenderHistoryRegistry>(impl->device, impl->resources->Releases());
    impl->feature_services.histories = impl->histories.get();
    impl->diagnostic_shader = impl->shaders.Create();
    TRY_VOID(impl->shaders.Publish(impl->diagnostic_shader, impl->diagnostic_shader_product));
    BorrowedShader diagnostic;
    TRY_ASSIGN(diagnostic, impl->shaders.Borrow(impl->diagnostic_shader));
    TRY_ASSIGN(
        impl->bindings,
        PhysicalBindings::Create(
            impl->device,
            impl->layouts,
            *impl->palettes,
            *impl->resources->Pool(BufferPoolClass::Skinning),
            impl->resources->Uploads(),
            std::move(diagnostic)
        )
    );
    impl->feature_services.bindings = impl->bindings.get();
    const auto* canvas_product = impl->feature_services.programs->ProgramProduct("canvas");
    if (canvas_product == nullptr)
        return Err(ErrorCode::FileNotFound, "cooked standard programs have no canvas shader");
    TRY_ASSIGN(
        impl->canvas,
        CanvasFeature::Create(impl->device, impl->resources->Uploads(), impl->resources->Releases(), *canvas_product)
    );
    TRY_VOID(RegisterStandardFeatures(impl->features));
    impl->features.Freeze();
    auto runtime = scope<RenderRuntime>(new RenderRuntime(std::move(impl)));
    for (auto& surface : descriptor.surfaces)
        if (auto created = runtime->CreateSurface(std::move(surface)); !created)
            return Err(std::move(created).error());
    return Ok(std::move(runtime));
}

RenderRuntime::RenderRuntime(scope<Impl> impl)
    : impl_(std::move(impl)) {}

RenderRuntime::~RenderRuntime() {
    if (impl_ != nullptr) {
        auto shutdown = Shutdown();
        if (!shutdown)
            static_cast<void>(impl_.release()); // Never destroy unproven in-flight ownership.
    }
}

Result<void> RenderRuntime::Shutdown() {
    if (impl_ == nullptr || impl_->shutdown)
        return Ok();
    if (impl_->stop_requests)
        impl_->stop_requests();
    if (impl_->join_workers)
        impl_->join_workers();
    if (impl_->drain_publications)
        impl_->drain_publications();
    if (impl_->completions != nullptr)
        while (impl_->completions->Drain() != 0) {
        }
    while (impl_->assets.manager->PumpPublications() != 0) {
    }
    if (impl_->flush_asset_manifest)
        TRY_VOID(impl_->flush_asset_manifest());
    RuntimeFacadeAccess::CancelCompute(*impl_->compute);
    RuntimeFacadeAccess::CancelReadbacks(*impl_->readback_service);
    impl_->executables.clear();
    if (impl_->recovery_state == RenderRecoveryState::DeviceLost
        || impl_->recovery_state == RenderRecoveryState::Failed) {
        static_cast<void>(impl_->resources->Releases()->AbandonForDeviceLoss());
        impl_->shutdown = true;
        return Ok();
    }
    if (impl_->last_submission.IsValid()) {
        if (impl_->wait_for_shutdown_submission)
            TRY_VOID(impl_->wait_for_shutdown_submission(*impl_->device, impl_->last_submission));
        else
            while (!impl_->device->GetQueue().CompletedSubmission().HasReached(impl_->last_submission)) {
                impl_->device->Tick();
                impl_->instance->ProcessEvents();
            }
        const auto completed = impl_->device->GetQueue().CompletedSubmission();
        if (!completed.HasReached(impl_->last_submission))
            return Err(ErrorCode::GraphicsInitFailed, "shutdown drain returned before the final submission completed");
    }
    impl_->resources->Collect();
    if (impl_->feature_readbacks != nullptr)
        static_cast<void>(impl_->feature_readbacks->Poll(impl_->device->GetQueue().CompletedSubmission()));
    if (impl_->resources->Releases()->PendingCount() != 0)
        return Err(ErrorCode::InvalidState, "shutdown left deferred GPU ownership without completion proof");
    impl_->shutdown = true;
    return Ok();
}

} // namespace woki::gfx
