#include <algorithm>
#include <map>
#include <optional>
#include <unordered_map>

#include <woki/gfx/compute.hpp>
#include <woki/gfx/advanced/layout.hpp>
#include <woki/gfx/advanced/library.hpp>
#include <woki/gfx/advanced/pipeline_cache.hpp>
#include <woki/gfx/advanced/deferred_release.hpp>
#include <woki/rhi/compute_pass_encoder.hpp>
#include <woki/rhi/device.hpp>

#include "internal/runtime_facade.hpp"

namespace woki::gfx {

GpuJobBuilder& GpuJobBuilder::DependsOn(const GpuJobTicket ticket) {
    dependencies_.push_back(ticket);
    return *this;
}

GpuJobBuilder& GpuJobBuilder::Dispatch(
    const asset::AssetId cooked_shader,
    std::string entry_point,
    const DispatchSize size
) {
    declarations_.push_back(DispatchDeclaration{cooked_shader, std::move(entry_point), size});
    return *this;
}

GpuJobBuilder& GpuJobBuilder::Copy(
    const BufferHandle source,
    const BufferHandle destination,
    const u64 size,
    const u64 source_offset,
    const u64 destination_offset
) {
    declarations_.push_back(CopyDeclaration{source, destination, size, source_offset, destination_offset});
    return *this;
}

GpuJobBuilder& GpuJobBuilder::BindBuffer(
    const u32 group,
    const u32 binding,
    const BufferHandle buffer,
    const GpuResourceAccess access,
    const u64 offset,
    const u64 size
) {
    buffers_.push_back({group, binding, buffer, access, offset, size});
    return *this;
}

GpuJobBuilder& GpuJobBuilder::BindTexture(
    const u32 group,
    const u32 binding,
    const GpuTextureHandle texture,
    const GpuResourceAccess access,
    const GpuTextureSubresource subresource
) {
    textures_.push_back({group, binding, texture, access, subresource});
    return *this;
}

GpuJobBuilder& GpuJobBuilder::Priority(const GpuJobPriority priority) noexcept {
    priority_ = priority;
    return *this;
}

GpuJobBuilder& GpuJobBuilder::Budget(const u64 bytes, const u32 dispatches) noexcept {
    byte_budget_ = bytes;
    dispatch_budget_ = dispatches;
    return *this;
}

struct ComputeService::Impl final {
    struct Entry final {
        GpuJobState state{GpuJobState::Queued};
        GpuJobBuilder job;
        rhi::SubmissionTicket submission;
        std::string error;
        GraphPass last_pass;
    };

    template <typename Descriptor>
    struct Slot final {
        u32 generation{1};
        std::optional<Descriptor> descriptor;
        ref<rhi::Buffer> buffer;
        ref<rhi::Texture> texture;
        rhi::SubmissionTicket last_used;
    };

    struct ActiveDispatch final {
        u64 job{};
        ComputePipelineKey key;
        BorrowedShader shader;
        BorrowedLayout layout;
    };

    std::map<u64, Entry> jobs;
    ref<DeferredReleaseQueue> releases;
    std::vector<Slot<BufferDescriptor>> buffers;
    std::vector<Slot<GpuTextureDescriptor>> textures;
    u64 next_ticket{1};
    u32 maximum_queued{64};
    u64 fairness_cursor{};
    std::vector<u64> active_jobs;
    std::vector<ActiveDispatch> active_dispatches;
    std::map<BufferHandle, GraphBuffer> graph_buffers;
    std::map<GpuTextureHandle, GraphTexture> graph_textures;
    std::map<BufferHandle, GraphBufferRef> graph_buffer_versions;
    std::map<GpuTextureHandle, GraphTextureRef> graph_texture_versions;
    std::map<asset::AssetId, u64> shader_asset_generations;
    std::map<asset::AssetId, ref<const ShaderGeneration>> compute_shaders;
};

ComputeService::ComputeService()
    : impl_(createScope<Impl>()) {}

ComputeService::~ComputeService() = default;

void RuntimeFacadeAccess::AttachComputeReleases(ComputeService& service, ref<DeferredReleaseQueue> releases) {
    service.impl_->releases = std::move(releases);
}

Result<BufferHandle> ComputeService::CreateBuffer(BufferDescriptor descriptor) {
    if (descriptor.size == 0)
        return Err(ErrorCode::InvalidArgument, "GPU buffer size must be non-zero");
    const u32 index = static_cast<u32>(impl_->buffers.size());
    Impl::Slot<BufferDescriptor> slot;
    slot.descriptor = std::move(descriptor);
    impl_->buffers.push_back(std::move(slot));
    return Ok(BufferHandle::Create(index, impl_->buffers.back().generation));
}

Result<void> ComputeService::DestroyBuffer(const BufferHandle buffer) {
    if (!buffer.IsValid() || buffer.Index() >= impl_->buffers.size())
        return Err(ErrorCode::InvalidArgument, "GPU buffer handle is stale");
    auto& slot = impl_->buffers[buffer.Index()];
    if (slot.generation != buffer.Generation() || !slot.descriptor)
        return Err(ErrorCode::InvalidArgument, "GPU buffer handle is stale");
    slot.descriptor.reset();
    if (slot.buffer != nullptr) {
        if (impl_->releases == nullptr)
            return Err(ErrorCode::InvalidState, "compute service is not attached to a runtime release queue");
        impl_->releases->Retire(std::move(slot.buffer), slot.last_used);
    }
    ++slot.generation;
    if (slot.generation == 0)
        ++slot.generation;
    return Ok();
}

Result<GpuTextureHandle> ComputeService::CreateTexture(GpuTextureDescriptor descriptor) {
    if (descriptor.width == 0 || descriptor.height == 0 || descriptor.depth_or_layers == 0)
        return Err(ErrorCode::InvalidArgument, "GPU texture dimensions must be non-zero");
    const u32 index = static_cast<u32>(impl_->textures.size());
    Impl::Slot<GpuTextureDescriptor> slot;
    slot.descriptor = std::move(descriptor);
    impl_->textures.push_back(std::move(slot));
    return Ok(GpuTextureHandle::Create(index, impl_->textures.back().generation));
}

Result<void> ComputeService::DestroyTexture(const GpuTextureHandle texture) {
    if (!texture.IsValid() || texture.Index() >= impl_->textures.size())
        return Err(ErrorCode::InvalidArgument, "GPU texture handle is stale");
    auto& slot = impl_->textures[texture.Index()];
    if (slot.generation != texture.Generation() || !slot.descriptor)
        return Err(ErrorCode::InvalidArgument, "GPU texture handle is stale");
    slot.descriptor.reset();
    if (slot.texture != nullptr) {
        if (impl_->releases == nullptr)
            return Err(ErrorCode::InvalidState, "compute service is not attached to a runtime release queue");
        impl_->releases->Retire(std::move(slot.texture), slot.last_used);
    }
    ++slot.generation;
    if (slot.generation == 0)
        ++slot.generation;
    return Ok();
}

GpuJobBuilder ComputeService::CreateJob() const {
    return {};
}

Result<GpuJobTicket> ComputeService::Submit(GpuJobBuilder job) {
    if (job.declarations_.empty())
        return Err(ErrorCode::InvalidArgument, "GPU job has no dispatch or copy declarations");
    for (const auto& declaration : job.declarations_) {
        if (const auto* dispatch = std::get_if<GpuJobBuilder::DispatchDeclaration>(&declaration)) {
            if (!dispatch->shader || dispatch->entry.empty() || dispatch->size.x == 0 || dispatch->size.y == 0
                || dispatch->size.z == 0)
                return Err(ErrorCode::InvalidArgument, "GPU dispatch declaration is incomplete");
        } else if (std::get<GpuJobBuilder::CopyDeclaration>(declaration).size == 0) {
            return Err(ErrorCode::InvalidArgument, "GPU copy declaration has zero size");
        }
    }
    if (static_cast<u64>(std::ranges::count_if(
            impl_->jobs,
            [](const auto& item) {
                return item.second.state == GpuJobState::Queued || item.second.state == GpuJobState::Submitted;
            }
        ))
        >= static_cast<u64>(impl_->maximum_queued))
        return Err(ErrorCode::InvalidState, "GPU job queue is at its bounded backpressure limit");
    for (const auto dependency : job.dependencies_)
        if (!dependency.IsValid() || !impl_->jobs.contains(dependency.id))
            return Err(ErrorCode::InvalidArgument, "GPU job dependency is unknown");
    // Runtime graph integration publishes the submission ticket atomically. A
    // job is never represented as submitted before that publication occurs.
    const u64 id = impl_->next_ticket++;
    Impl::Entry entry;
    entry.state = GpuJobState::Queued;
    entry.job = std::move(job);
    impl_->jobs.emplace(id, std::move(entry));
    return Ok(GpuJobTicket{.id = id, .submission = {}});
}

GpuJobState ComputeService::Poll(const GpuJobTicket ticket) const noexcept {
    const auto found = impl_->jobs.find(ticket.id);
    return found == impl_->jobs.end() ? GpuJobState::Failed : found->second.state;
}

GpuSubmissionId ComputeService::Submission(const GpuJobTicket ticket) const noexcept {
    const auto found = impl_->jobs.find(ticket.id);
    return found == impl_->jobs.end() ? GpuSubmissionId{} : GpuSubmissionId(found->second.submission.Value());
}

std::string ComputeService::Error(const GpuJobTicket ticket) const {
    const auto found = impl_->jobs.find(ticket.id);
    return found == impl_->jobs.end() ? "GPU job ticket is unknown" : found->second.error;
}

Result<void> ComputeService::Cancel(const GpuJobTicket ticket) {
    const auto found = impl_->jobs.find(ticket.id);
    if (found == impl_->jobs.end())
        return Err(ErrorCode::InvalidArgument, "GPU job ticket is unknown");
    if (found->second.state == GpuJobState::Submitted || found->second.state == GpuJobState::Complete)
        return Err(ErrorCode::InvalidState, "submitted GPU work cannot be cancelled");
    found->second.state = GpuJobState::Cancelled;
    return Ok();
}

namespace {

rhi::TextureFormat ToRhiFormat(const GpuTextureFormat format) {
    switch (format) {
        case GpuTextureFormat::RGBA8UnormSrgb:
            return rhi::TextureFormat::RGBA8UnormSrgb;
        case GpuTextureFormat::BGRA8Unorm:
            return rhi::TextureFormat::BGRA8Unorm;
        case GpuTextureFormat::RGBA16Float:
            return rhi::TextureFormat::RGBA16Float;
        case GpuTextureFormat::R32Uint:
            return rhi::TextureFormat::R32Uint;
        default:
            return rhi::TextureFormat::RGBA8Unorm;
    }
}

GraphAccess ToGraphAccess(const GpuResourceAccess access) {
    return access == GpuResourceAccess::Read ? GraphAccess::StorageRead : GraphAccess::StorageWrite;
}

TextureSubresourceRange ToGraphRange(const GpuTextureSubresource& range) {
    return {range.base_mip_level, range.mip_level_count, range.base_array_layer, range.array_layer_count};
}

} // namespace

Result<void> RuntimeFacadeAccess::AppendCompute(
    ComputeService& service,
    RuntimeFacadeGraph& graph,
    rhi::Device& device,
    asset::AssetManager& assets,
    LayoutCache& layouts,
    PipelineCache& pipelines,
    const rhi::SubmissionEpoch completed
) {
    auto& impl = *service.impl_;
    impl.active_jobs.clear();
    impl.active_dispatches.clear();
    impl.graph_buffers.clear();
    impl.graph_textures.clear();
    impl.graph_buffer_versions.clear();
    impl.graph_texture_versions.clear();
    for (auto& [_, entry] : impl.jobs)
        if (entry.state == GpuJobState::Submitted && completed.HasReached(entry.submission))
            entry.state = GpuJobState::Complete;

    std::vector<std::pair<u64, ComputeService::Impl::Entry*>> queued;
    for (auto& [id, entry] : impl.jobs)
        if (entry.state == GpuJobState::Queued)
            queued.emplace_back(id, &entry);
    const u64 fairness = impl.fairness_cursor++;
    std::ranges::stable_sort(queued, [fairness](const auto& lhs, const auto& rhs) {
        if (lhs.second->job.priority_ != rhs.second->job.priority_)
            return lhs.second->job.priority_ > rhs.second->job.priority_;
        return lhs.first - fairness < rhs.first - fairness;
    });

    u64 frame_bytes{};
    u32 frame_dispatches{};
    std::map<u64, GraphPass> scheduled;
    for (auto [id, entry] : queued) {
        bool dependency_wait{};
        for (const auto dependency : entry->job.dependencies_) {
            const auto found = impl.jobs.find(dependency.id);
            if (found == impl.jobs.end() || found->second.state == GpuJobState::Failed
                || found->second.state == GpuJobState::Cancelled || found->second.state == GpuJobState::DeviceLost) {
                entry->state = GpuJobState::Failed;
                entry->error = "GPU job dependency did not complete successfully";
                dependency_wait = true;
                break;
            }
            if (found->second.state == GpuJobState::Queued && !scheduled.contains(dependency.id))
                dependency_wait = true;
        }
        if (dependency_wait)
            continue;

        u64 job_bytes{};
        u32 job_dispatches{};
        for (const auto& declaration : entry->job.declarations_)
            if (const auto* copy = std::get_if<GpuJobBuilder::CopyDeclaration>(&declaration))
                job_bytes += copy->size;
            else
                ++job_dispatches;
        if ((entry->job.byte_budget_ != 0 && job_bytes > entry->job.byte_budget_)
            || (entry->job.dispatch_budget_ != 0 && job_dispatches > entry->job.dispatch_budget_)) {
            entry->state = GpuJobState::Failed;
            entry->error = "GPU job declarations exceed the submitted budget";
            continue;
        }
        if (frame_bytes + job_bytes > 64ull * 1024ull * 1024ull || frame_dispatches + job_dispatches > 16)
            continue;

        bool shaders_ready = true;
        for (const auto& declaration : entry->job.declarations_) {
            const auto* dispatch = std::get_if<GpuJobBuilder::DispatchDeclaration>(&declaration);
            if (dispatch == nullptr || assets.Borrow(dispatch->shader))
                continue;
            shaders_ready = false;
            const auto status = assets.Status(dispatch->shader);
            if (status.state == asset::AssetState::Failed) {
                entry->state = GpuJobState::Failed;
                entry->error = status.error ? status.error->message : "cooked compute shader failed to load";
            } else if (status.state == asset::AssetState::Unloaded) {
                auto requested = assets.Request(
                    dispatch->shader,
                    asset::AssetRequestOptions{.cancellation = {},
                        .priority = asset::AssetPriority::High,
                        .prefetch = false}
                );
                if (!requested) {
                    entry->state = GpuJobState::Failed;
                    entry->error = std::string(requested.error().Message());
                }
            }
        }
        if (!shaders_ready)
            continue;

        auto import_buffer = [&](const BufferHandle handle) -> Result<GraphBuffer> {
            if (!handle.IsValid() || handle.Index() >= impl.buffers.size())
                return Err(ErrorCode::InvalidArgument, "GPU job references a stale buffer");
            auto& slot = impl.buffers[handle.Index()];
            if (slot.generation != handle.Generation() || !slot.descriptor)
                return Err(ErrorCode::InvalidArgument, "GPU job references a stale buffer");
            if (slot.buffer == nullptr) {
                scope<rhi::Buffer> created;
                TRY_ASSIGN(
                    created,
                    device.CreateBuffer(
                        {.size = slot.descriptor->size,
                            .usage = rhi::BufferUsage::Storage | rhi::BufferUsage::Uniform | rhi::BufferUsage::Indirect
                                     | rhi::BufferUsage::CopySrc | rhi::BufferUsage::CopyDst,
                            .label = slot.descriptor->label}
                    )
                );
                slot.buffer = ref<rhi::Buffer>(std::move(created));
            }
            if (const auto found = impl.graph_buffers.find(handle); found != impl.graph_buffers.end())
                return Ok(found->second);
            GraphBufferDesc descriptor{.label = slot.descriptor->label,
                .size = slot.descriptor->size,
                .usage = rhi::BufferUsage::Storage | rhi::BufferUsage::Uniform | rhi::BufferUsage::Indirect
                         | rhi::BufferUsage::CopySrc | rhi::BufferUsage::CopyDst};
            const auto imported = graph.builder.ImportBuffer(
                {descriptor, ExternalState::ShaderRead, ExternalState::ShaderRead, true}
            );
            impl.graph_buffers.emplace(handle, imported);
            impl.graph_buffer_versions.emplace(handle, graph.builder.Initial(imported));
            return Ok(imported);
        };
        auto import_texture = [&](const GpuTextureHandle handle) -> Result<GraphTexture> {
            if (!handle.IsValid() || handle.Index() >= impl.textures.size())
                return Err(ErrorCode::InvalidArgument, "GPU job references a stale texture");
            auto& slot = impl.textures[handle.Index()];
            if (slot.generation != handle.Generation() || !slot.descriptor)
                return Err(ErrorCode::InvalidArgument, "GPU job references a stale texture");
            if (slot.texture == nullptr) {
                scope<rhi::Texture> created;
                TRY_ASSIGN(
                    created,
                    device.CreateTexture(
                        {.size = {slot.descriptor->width, slot.descriptor->height, slot.descriptor->depth_or_layers},
                            .mip_level_count = slot.descriptor->mip_levels,
                            .format = ToRhiFormat(slot.descriptor->format),
                            .usage = rhi::TextureUsage::TextureBinding | rhi::TextureUsage::StorageBinding
                                     | rhi::TextureUsage::CopySrc | rhi::TextureUsage::CopyDst,
                            .label = slot.descriptor->label}
                    )
                );
                slot.texture = ref<rhi::Texture>(std::move(created));
            }
            if (const auto found = impl.graph_textures.find(handle); found != impl.graph_textures.end())
                return Ok(found->second);
            GraphTextureDesc descriptor;
            descriptor.label = slot.descriptor->label;
            descriptor.extent = GraphExtent::Fixed(slot.descriptor->width, slot.descriptor->height);
            descriptor.depth_or_layers = slot.descriptor->depth_or_layers;
            descriptor.mip_levels = slot.descriptor->mip_levels;
            descriptor.format = ToRhiFormat(slot.descriptor->format);
            descriptor.usage = rhi::TextureUsage::TextureBinding | rhi::TextureUsage::StorageBinding
                               | rhi::TextureUsage::CopySrc | rhi::TextureUsage::CopyDst;
            ExternalTextureContract contract;
            contract.descriptor = descriptor;
            contract.initial_state = ExternalState::ShaderRead;
            contract.final_state = ExternalState::ShaderRead;
            contract.frame_bound = true;
            const auto imported = graph.builder.ImportTexture(std::move(contract));
            impl.graph_textures.emplace(handle, imported);
            impl.graph_texture_versions.emplace(handle, graph.builder.Initial(imported));
            return Ok(imported);
        };

        impl.active_jobs.push_back(id);
        GraphPass last;
        for (const auto& declaration : entry->job.declarations_) {
            if (const auto* copy = std::get_if<GpuJobBuilder::CopyDeclaration>(&declaration)) {
                GraphBuffer source;
                GraphBuffer destination;
                TRY_ASSIGN(source, import_buffer(copy->source));
                TRY_ASSIGN(destination, import_buffer(copy->destination));
                auto pass = graph.builder.AddPass("Runtime compute buffer copy", PassKind::Copy);
                for (const auto dependency : entry->job.dependencies_)
                    if (const auto found = scheduled.find(dependency.id); found != scheduled.end())
                        pass.DependsOn(found->second);
                const auto source_version = impl.graph_buffer_versions.at(copy->source);
                pass.Read(source_version, GraphAccess::CopySource);
                const auto written = pass.Write(destination, GraphAccess::CopyDestination);
                impl.graph_buffer_versions[copy->destination] = written;
                pass.SideEffect("gpu-job").Execute(
                    [source_ref = source_version, written, copy = *copy](RenderGraphContext& context) -> Result<void> {
                        auto source_buffer = context.Buffer(source_ref);
                        auto destination_buffer = context.Buffer(written);
                        if (!source_buffer)
                            return Err(std::move(source_buffer).error());
                        if (!destination_buffer)
                            return Err(std::move(destination_buffer).error());
                        return context.CommandEncoder().CopyBufferToBuffer(
                            source_buffer->get(),
                            copy.source_offset,
                            destination_buffer->get(),
                            copy.destination_offset,
                            copy.size
                        );
                    }
                );
                last = pass.Handle();
            } else {
                const auto& dispatch = std::get<GpuJobBuilder::DispatchDeclaration>(declaration);
                auto lease = assets.Borrow(dispatch.shader);
                if (!lease) {
                    const auto status = assets.Status(dispatch.shader);
                    if (status.state == asset::AssetState::Failed) {
                        entry->state = GpuJobState::Failed;
                        entry->error = status.error ? status.error->message : "cooked compute shader failed to load";
                    } else if (status.state == asset::AssetState::Unloaded) {
                        auto requested = assets.Request(
                            dispatch.shader,
                            asset::AssetRequestOptions{.cancellation = {},
                                .priority = asset::AssetPriority::High,
                                .prefetch = false}
                        );
                        if (!requested) {
                            entry->state = GpuJobState::Failed;
                            entry->error = std::string(requested.error().Message());
                        }
                    }
                    last = {};
                    break;
                }
                if (lease->Get().type != kShaderProductType) {
                    entry->state = GpuJobState::Failed;
                    entry->error = "compute asset is not a cooked shader product";
                    last = {};
                    break;
                }
                auto resident = impl.compute_shaders.find(dispatch.shader);
                if (resident == impl.compute_shaders.end()
                    || impl.shader_asset_generations[dispatch.shader] != lease->Get().version.generation) {
                    ShaderPayload payload;
                    TRY_ASSIGN(payload, ParseShaderPayload(lease->Bytes()));
                    scope<rhi::ShaderModule> module;
                    TRY_ASSIGN(
                        module,
                        device.CreateShaderModule({.code = payload.code, .label = "Runtime compute shader"})
                    );
                    auto generation = createRef<const ShaderGeneration>(ShaderGeneration{
                        ref<rhi::ShaderModule>(std::move(module)),
                        std::move(payload),
                        lease->Get().version.generation,
                        lease->Get().version.product_hash});
                    resident = impl.compute_shaders.insert_or_assign(dispatch.shader, std::move(generation)).first;
                    impl.shader_asset_generations[dispatch.shader] = lease->Get().version.generation;
                }
                BorrowedShader shader{resident->second};
                const auto reflected = std::ranges::find_if(shader.Interface().entry_points, [&](const auto& value) {
                    return value.stage == ShaderStage::Compute && value.name == dispatch.entry;
                });
                if (reflected == shader.Interface().entry_points.end()) {
                    entry->state = GpuJobState::Failed;
                    entry->error = "cooked shader does not contain the requested compute entry point";
                    last = {};
                    break;
                }
                PipelineLayoutKey layout_key;
                TRY_ASSIGN(layout_key, MakePipelineLayoutKey(shader.Interface()));
                BorrowedLayout layout;
                TRY_ASSIGN(layout, layouts.GetOrCreate(layout_key));
                ComputePipelineKey key{
                    .shader_product = lease->Get().version.product_hash,
                    .shader_variant = shader.generation->payload.variant_hash,
                    .shader_generation = shader.Version(),
                    .compute_entry = StringId(dispatch.entry),
                    .pipeline_layout = layout_key.hash,
                    .overrides = {},
                };
                PipelineRequest<rhi::ComputePipeline> pipeline;
                TRY_ASSIGN(
                    pipeline,
                    pipelines.Request(
                        key,
                        [&device, shader, layout, entry_point = dispatch.entry]() -> Result<ref<rhi::ComputePipeline>> {
                            scope<rhi::ComputePipeline> created;
                            TRY_ASSIGN(
                                created,
                                device.CreateComputePipeline(
                                    {.layout = &layout.Pipeline(),
                                        .compute = {.module = &shader.Module(), .entry_point = entry_point},
                                        .label = "Runtime compute job"}
                                )
                            );
                            return Ok(ref<rhi::ComputePipeline>(std::move(created)));
                        }
                    )
                );
                if (pipeline.state != PipelineRequestState::Ready || pipeline.pipeline == nullptr) {
                    entry->state = GpuJobState::Failed;
                    entry->error = pipeline.diagnostic.empty() ? "compute pipeline is not ready" : pipeline.diagnostic;
                    last = {};
                    break;
                }

                auto pass = graph.builder.AddPass("Runtime compute dispatch", PassKind::Compute);
                for (const auto dependency : entry->job.dependencies_)
                    if (const auto found = scheduled.find(dependency.id); found != scheduled.end())
                        pass.DependsOn(found->second);
                std::vector<std::pair<GpuJobBuilder::BufferBinding, GraphBufferRef>> buffer_refs;
                std::vector<std::pair<GpuJobBuilder::TextureBinding, GraphTextureRef>> texture_refs;
                for (const auto& binding : entry->job.buffers_) {
                    GraphBuffer resource;
                    TRY_ASSIGN(resource, import_buffer(binding.buffer));
                    auto version = impl.graph_buffer_versions.at(binding.buffer);
                    if (binding.access == GpuResourceAccess::Read)
                        pass.Read(version, ToGraphAccess(binding.access));
                    else if (binding.access == GpuResourceAccess::ReadWrite)
                        version = pass.ReadWrite(version, GraphAccess::StorageWrite);
                    else
                        version = pass.Write(resource, ToGraphAccess(binding.access));
                    if (binding.access != GpuResourceAccess::Read)
                        impl.graph_buffer_versions[binding.buffer] = version;
                    buffer_refs.emplace_back(binding, version);
                }
                for (const auto& binding : entry->job.textures_) {
                    GraphTexture resource;
                    TRY_ASSIGN(resource, import_texture(binding.texture));
                    auto version = impl.graph_texture_versions.at(binding.texture);
                    if (binding.access == GpuResourceAccess::Read)
                        pass.Read(version, ToGraphAccess(binding.access), ToGraphRange(binding.subresource));
                    else if (binding.access == GpuResourceAccess::ReadWrite) {
                        pass.Read(version, GraphAccess::StorageRead, ToGraphRange(binding.subresource));
                        version = pass.Write(resource, GraphAccess::StorageWrite, ToGraphRange(binding.subresource));
                    } else
                        version = pass.Write(
                            resource,
                            ToGraphAccess(binding.access),
                            ToGraphRange(binding.subresource)
                        );
                    if (binding.access != GpuResourceAccess::Read)
                        impl.graph_texture_versions[binding.texture] = version;
                    texture_refs.emplace_back(binding, version);
                }
                pass.Queue(QueuePreference::Compute, true)
                    .SideEffect("gpu-job")
                    .Execute(
                        [pipeline = pipeline.pipeline,
                            layout,
                            buffer_refs = std::move(buffer_refs),
                            texture_refs = std::move(texture_refs),
                            size = dispatch.size](RenderGraphContext& context) -> Result<void> {
                            std::map<u32, std::vector<rhi::BindGroupEntryDesc>> groups;
                            std::vector<scope<rhi::TextureView>> views;
                            for (const auto& [binding, resource] : buffer_refs) {
                                auto buffer = context.Buffer(resource);
                                if (!buffer)
                                    return Err(std::move(buffer).error());
                                groups[binding.group].push_back(
                                    {.binding = binding.binding,
                                        .buffer = &buffer->get(),
                                        .offset = binding.offset,
                                        .size = binding.size == 0 ? rhi::kWholeSize : binding.size}
                                );
                            }
                            for (const auto& [binding, resource] : texture_refs) {
                                GraphTextureViewDesc view_desc;
                                view_desc.subresources = ToGraphRange(binding.subresource);
                                auto view = context.TextureView(resource, std::move(view_desc));
                                if (!view)
                                    return Err(std::move(view).error());
                                groups[binding.group]
                                    .push_back({.binding = binding.binding, .texture_view = &view->get()});
                            }
                            std::vector<scope<rhi::BindGroup>> bind_groups;
                            for (auto& [group, entries] : groups) {
                                if (group >= layout.BindGroupLayouts().size())
                                    return Err(
                                        ErrorCode::ValidationOutOfRange,
                                        "compute binding group is absent from the cooked interface"
                                    );
                                scope<rhi::BindGroup> created;
                                TRY_ASSIGN(
                                    created,
                                    context.Device().CreateBindGroup(
                                        {.layout = layout.BindGroupLayouts()[group],
                                            .entries = entries,
                                            .label = "Runtime compute bindings"}
                                    )
                                );
                                context.ComputeEncoder()->SetBindGroup(group, created.get());
                                bind_groups.push_back(std::move(created));
                            }
                            context.ComputeEncoder()->SetPipeline(*pipeline);
                            context.ComputeEncoder()->DispatchWorkgroups(size.x, size.y, size.z);
                            return Ok();
                        }
                    );
                last = pass.Handle();
                impl.active_dispatches.push_back({id, key, shader, layout});
            }
        }
        if (!last) {
            std::erase(impl.active_jobs, id);
            continue;
        }
        entry->last_pass = last;
        scheduled[id] = last;
        frame_bytes += job_bytes;
        frame_dispatches += job_dispatches;
        graph.has_work = true;
    }
    return Ok();
}

Result<ResolvedReadbackBuffer> RuntimeFacadeAccess::ResolveBuffer(
    ComputeService& service,
    rhi::Device& device,
    const BufferHandle handle
) {
    auto& impl = *service.impl_;
    if (!handle.IsValid() || handle.Index() >= impl.buffers.size())
        return Err(ErrorCode::InvalidArgument, "readback references a stale GPU buffer");
    auto& slot = impl.buffers[handle.Index()];
    if (slot.generation != handle.Generation() || !slot.descriptor)
        return Err(ErrorCode::InvalidArgument, "readback references a stale GPU buffer");
    if (slot.buffer == nullptr) {
        scope<rhi::Buffer> created;
        TRY_ASSIGN(
            created,
            device.CreateBuffer(
                {.size = slot.descriptor->size,
                    .usage = rhi::BufferUsage::Storage | rhi::BufferUsage::Uniform | rhi::BufferUsage::Indirect
                             | rhi::BufferUsage::CopySrc | rhi::BufferUsage::CopyDst,
                    .label = slot.descriptor->label}
            )
        );
        slot.buffer = ref<rhi::Buffer>(std::move(created));
    }
    return Ok(
        ResolvedReadbackBuffer{slot.buffer,
            {.label = slot.descriptor->label,
                .size = slot.descriptor->size,
                .usage = rhi::BufferUsage::Storage | rhi::BufferUsage::Uniform | rhi::BufferUsage::Indirect
                         | rhi::BufferUsage::CopySrc | rhi::BufferUsage::CopyDst}}
    );
}

Result<ResolvedReadbackTexture> RuntimeFacadeAccess::ResolveTexture(
    ComputeService& service,
    rhi::Device& device,
    const GpuTextureHandle handle
) {
    auto& impl = *service.impl_;
    if (!handle.IsValid() || handle.Index() >= impl.textures.size())
        return Err(ErrorCode::InvalidArgument, "readback references a stale GPU texture");
    auto& slot = impl.textures[handle.Index()];
    if (slot.generation != handle.Generation() || !slot.descriptor)
        return Err(ErrorCode::InvalidArgument, "readback references a stale GPU texture");
    if (slot.texture == nullptr) {
        scope<rhi::Texture> created;
        TRY_ASSIGN(
            created,
            device.CreateTexture(
                {.size = {slot.descriptor->width, slot.descriptor->height, slot.descriptor->depth_or_layers},
                    .mip_level_count = slot.descriptor->mip_levels,
                    .format = ToRhiFormat(slot.descriptor->format),
                    .usage = rhi::TextureUsage::TextureBinding | rhi::TextureUsage::StorageBinding
                             | rhi::TextureUsage::CopySrc | rhi::TextureUsage::CopyDst,
                    .label = slot.descriptor->label}
            )
        );
        slot.texture = ref<rhi::Texture>(std::move(created));
    }
    PixelFormat format = PixelFormat::RGBA8Unorm;
    switch (slot.descriptor->format) {
        case GpuTextureFormat::RGBA8UnormSrgb:
            format = PixelFormat::RGBA8UnormSrgb;
            break;
        case GpuTextureFormat::BGRA8Unorm:
            format = PixelFormat::BGRA8Unorm;
            break;
        case GpuTextureFormat::RGBA16Float:
            format = PixelFormat::RGBA16Float;
            break;
        case GpuTextureFormat::R32Uint:
            format = PixelFormat::R32Uint;
            break;
        default:
            break;
    }
    GraphTextureDesc descriptor;
    descriptor.label = slot.descriptor->label;
    descriptor.extent = GraphExtent::Fixed(slot.descriptor->width, slot.descriptor->height);
    descriptor.depth_or_layers = slot.descriptor->depth_or_layers;
    descriptor.mip_levels = slot.descriptor->mip_levels;
    descriptor.format = ToRhiFormat(slot.descriptor->format);
    descriptor.usage = rhi::TextureUsage::TextureBinding | rhi::TextureUsage::StorageBinding
                       | rhi::TextureUsage::CopySrc | rhi::TextureUsage::CopyDst;
    return Ok(ResolvedReadbackTexture{slot.texture, std::move(descriptor), format});
}

std::vector<GraphPass> RuntimeFacadeAccess::ComputeDependencies(const ComputeService& service) {
    std::vector<GraphPass> result;
    for (const u64 id : service.impl_->active_jobs)
        if (const auto found = service.impl_->jobs.find(id);
            found != service.impl_->jobs.end() && found->second.last_pass)
            result.push_back(found->second.last_pass);
    return result;
}

Result<void> RuntimeFacadeAccess::BindCompute(ComputeService& service, GraphFrame& frame) {
    auto& impl = *service.impl_;
    for (const auto& [handle, resource] : impl.graph_buffers)
        TRY_VOID(frame.Bind(
            {resource,
                impl.buffers[handle.Index()].buffer,
                {.label = impl.buffers[handle.Index()].descriptor->label,
                    .size = impl.buffers[handle.Index()].descriptor->size,
                    .usage = rhi::BufferUsage::Storage | rhi::BufferUsage::Uniform | rhi::BufferUsage::Indirect
                             | rhi::BufferUsage::CopySrc | rhi::BufferUsage::CopyDst}}
        ));
    for (const auto& [handle, resource] : impl.graph_textures) {
        const auto& descriptor = *impl.textures[handle.Index()].descriptor;
        GraphTextureBinding binding;
        binding.resource = resource;
        binding.texture = impl.textures[handle.Index()].texture;
        binding.view = ref<rhi::TextureView>(binding.texture->CreateView());
        binding.signature.label = descriptor.label;
        binding.signature.extent = GraphExtent::Fixed(descriptor.width, descriptor.height);
        binding.signature.depth_or_layers = descriptor.depth_or_layers;
        binding.signature.mip_levels = descriptor.mip_levels;
        binding.signature.format = ToRhiFormat(descriptor.format);
        binding.signature.usage = rhi::TextureUsage::TextureBinding | rhi::TextureUsage::StorageBinding
                                  | rhi::TextureUsage::CopySrc | rhi::TextureUsage::CopyDst;
        TRY_VOID(frame.Bind(std::move(binding)));
    }
    return Ok();
}

void RuntimeFacadeAccess::PublishCompute(
    ComputeService& service,
    LayoutCache& layouts,
    PipelineCache& pipelines,
    const rhi::SubmissionTicket submission
) {
    auto& impl = *service.impl_;
    for (const u64 id : impl.active_jobs) {
        auto& entry = impl.jobs.at(id);
        if (entry.state == GpuJobState::Queued) {
            entry.submission = submission;
            entry.state = GpuJobState::Submitted;
        }
    }
    for (const auto& dispatch : impl.active_dispatches) {
        layouts.MarkUsed(dispatch.layout, submission);
        static_cast<void>(pipelines.MarkUsed(dispatch.key, submission));
    }
    for (const auto& [handle, _] : impl.graph_buffers)
        impl.buffers[handle.Index()].last_used = std::max(impl.buffers[handle.Index()].last_used, submission);
    for (const auto& [handle, _] : impl.graph_textures)
        impl.textures[handle.Index()].last_used = std::max(impl.textures[handle.Index()].last_used, submission);
    impl.active_jobs.clear();
    impl.active_dispatches.clear();
}

void RuntimeFacadeAccess::FailCompute(ComputeService& service, const Error& error) {
    auto& impl = *service.impl_;
    for (const u64 id : impl.active_jobs) {
        auto& entry = impl.jobs.at(id);
        entry.state = GpuJobState::Failed;
        entry.error = std::string(error.Message());
    }
    impl.active_jobs.clear();
    impl.active_dispatches.clear();
}

void RuntimeFacadeAccess::PollCompute(ComputeService& service, const rhi::SubmissionEpoch completed) {
    for (auto& [_, entry] : service.impl_->jobs)
        if (entry.state == GpuJobState::Submitted && completed.HasReached(entry.submission))
            entry.state = GpuJobState::Complete;
}

void RuntimeFacadeAccess::DeviceLostCompute(ComputeService& service) noexcept {
    for (auto& [_, entry] : service.impl_->jobs)
        if (entry.state == GpuJobState::Queued || entry.state == GpuJobState::Submitted)
            entry.state = GpuJobState::DeviceLost;
    for (auto& slot : service.impl_->buffers)
        slot.buffer.reset();
    for (auto& slot : service.impl_->textures)
        slot.texture.reset();
    service.impl_->shader_asset_generations.clear();
    service.impl_->compute_shaders.clear();
}

void RuntimeFacadeAccess::CancelCompute(ComputeService& service) noexcept {
    for (auto& [_, entry] : service.impl_->jobs)
        if (entry.state == GpuJobState::Queued)
            entry.state = GpuJobState::Cancelled;
}

} // namespace woki::gfx
