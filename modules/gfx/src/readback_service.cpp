#include <cstring>
#include <map>
#include <optional>
#include <atomic>
#include <mutex>

#include <woki/gfx/readback.hpp>
#include <woki/rhi/command_encoder.hpp>
#include <woki/rhi/device.hpp>
#include <woki/rhi/objects.hpp>
#include <woki/rhi/queue.hpp>

#include "internal/runtime_facade.hpp"
#include "gfx_util.hpp"

namespace woki::gfx {

Result<ReadbackImage> ReadbackImage::ToRgba8() const {
    if (format != PixelFormat::RGBA8Unorm && format != PixelFormat::RGBA8UnormSrgb && format != PixelFormat::BGRA8Unorm)
        return Err(ErrorCode::InvalidArgument, "readback image format has no common RGBA8 conversion");
    const u32 source_row_bytes = row_bytes == 0 ? width * 4 : row_bytes;
    if (source_row_bytes < width * 4 || pixels.size() < static_cast<size_t>(source_row_bytes) * height)
        return Err(ErrorCode::InvalidArgument, "readback image rows are truncated");
    ReadbackImage result;
    result.width = width;
    result.height = height;
    result.row_bytes = width * 4;
    result.format = format;
    result.pixels.resize(static_cast<size_t>(result.row_bytes) * height);
    for (u32 row = 0; row < height; ++row)
        std::memcpy(
            result.pixels.data() + static_cast<size_t>(row) * result.row_bytes,
            pixels.data() + static_cast<size_t>(row) * source_row_bytes,
            result.row_bytes
        );
    if (format == PixelFormat::BGRA8Unorm) {
        result.format = PixelFormat::RGBA8Unorm;
        for (size_t offset = 0; offset + 3 < result.pixels.size(); offset += 4)
            std::swap(result.pixels[offset], result.pixels[offset + 2]);
    }
    return Ok(std::move(result));
}

struct ReadbackService::Impl final {
    struct Entry final {
        std::atomic<ReadbackState> state{ReadbackState::Queued};
        LogicalReadbackSource source;
        ref<const ReadbackResult> result;
        ref<rhi::Buffer> staging;
        ref<rhi::Buffer> source_buffer;
        ref<rhi::Texture> source_texture;
        GraphBufferDesc source_buffer_descriptor;
        GraphTextureDesc source_texture_descriptor;
        rhi::SubmissionTicket submission;
        u64 staging_size{};
        u32 padded_row_bytes{};
        u32 row_bytes{};
        u32 width{};
        u32 height{};
        PixelFormat format{PixelFormat::RGBA8Unorm};
        std::string error;
        std::atomic_bool release_requested{};
        mutable std::mutex mutex;
        GraphBuffer graph_staging;
        std::optional<GraphBuffer> graph_source_buffer;
        std::optional<GraphTexture> graph_source_texture;
    };

    mutable std::mutex mutex;
    std::map<u64, std::shared_ptr<Entry>> entries;
    std::vector<std::shared_ptr<Entry>> active;
    u64 next_ticket{1};
    u32 maximum_queued{256};
};

ReadbackService::ReadbackService()
    : impl_(createScope<Impl>()) {}

ReadbackService::~ReadbackService() = default;

Result<ReadbackTicket> ReadbackService::Request(LogicalReadbackSource source) {
    std::lock_guard lock(impl_->mutex);
    if (impl_->entries.size() >= impl_->maximum_queued)
        return Err(ErrorCode::InvalidState, "readback queue is at its bounded backpressure limit");
    const ReadbackTicket ticket(impl_->next_ticket++);
    auto entry = std::make_shared<Impl::Entry>();
    entry->source = std::move(source);
    impl_->entries.emplace(ticket.Value(), std::move(entry));
    return Ok(ticket);
}

ReadbackState ReadbackService::Poll(const ReadbackTicket ticket) const noexcept {
    std::lock_guard lock(impl_->mutex);
    const auto found = impl_->entries.find(ticket.Value());
    return found == impl_->entries.end() ? ReadbackState::Failed : found->second->state.load(std::memory_order_acquire);
}

Result<ReadbackLease> ReadbackService::TryMap(const ReadbackTicket ticket) const {
    std::lock_guard lock(impl_->mutex);
    const auto found = impl_->entries.find(ticket.Value());
    if (found == impl_->entries.end())
        return Err(ErrorCode::InvalidArgument, "readback ticket is unknown");
    std::lock_guard entry_lock(found->second->mutex);
    if (found->second->state.load(std::memory_order_acquire) != ReadbackState::Ready || !found->second->result)
        return Ok(ReadbackLease{});
    return Ok(ReadbackLease(found->second->result));
}

Result<u32> ReadbackService::TryMapPickingU32(const ReadbackTicket ticket) const {
    ReadbackLease mapped;
    TRY_ASSIGN(mapped, TryMap(ticket));
    if (!mapped)
        return Err(ErrorCode::InvalidState, "picking readback is not ready");
    const std::vector<std::byte>* bytes = std::get_if<std::vector<std::byte>>(mapped.Get());
    if (bytes == nullptr)
        if (const auto* image = std::get_if<ReadbackImage>(mapped.Get());
            image != nullptr && image->format == PixelFormat::R32Uint)
            bytes = &image->pixels;
    if (bytes == nullptr || bytes->size() < sizeof(u32))
        return Err(ErrorCode::InvalidState, "picking readback does not contain a u32 value");
    u32 result{};
    std::memcpy(&result, bytes->data(), sizeof(result));
    return Ok(result);
}

Result<void> ReadbackService::Release(const ReadbackTicket ticket) {
    std::lock_guard lock(impl_->mutex);
    const auto found = impl_->entries.find(ticket.Value());
    if (found == impl_->entries.end())
        return Err(ErrorCode::InvalidArgument, "readback ticket is unknown");
    const auto entry = found->second;
    std::lock_guard entry_lock(entry->mutex);
    const auto state = entry->state.load(std::memory_order_acquire);
    if (state == ReadbackState::Submitted || state == ReadbackState::Mapping) {
        entry->release_requested.store(true, std::memory_order_release);
        entry->state.store(ReadbackState::Consumed, std::memory_order_release);
        return Ok();
    }
    impl_->entries.erase(found);
    return Ok();
}

Result<void> ReadbackService::Cancel(const ReadbackTicket ticket) {
    std::lock_guard lock(impl_->mutex);
    const auto found = impl_->entries.find(ticket.Value());
    if (found == impl_->entries.end())
        return Err(ErrorCode::InvalidArgument, "readback ticket is unknown");
    const auto state = found->second->state.load(std::memory_order_acquire);
    if (state == ReadbackState::Ready || state == ReadbackState::Consumed || state == ReadbackState::Submitted
        || state == ReadbackState::Mapping)
        return Err(ErrorCode::InvalidState, "completed readback cannot be cancelled");
    found->second->state.store(ReadbackState::Cancelled, std::memory_order_release);
    return Ok();
}

std::string ReadbackService::Error(const ReadbackTicket ticket) const {
    std::lock_guard lock(impl_->mutex);
    const auto found = impl_->entries.find(ticket.Value());
    if (found == impl_->entries.end())
        return "readback ticket is unknown";
    std::lock_guard entry_lock(found->second->mutex);
    return found->second->error;
}

namespace {

u32 PixelBytes(const PixelFormat format) {
    return format == PixelFormat::RGBA16Float ? 8U : 4U;
}

} // namespace

Result<void> RuntimeFacadeAccess::AppendReadbacks(
    ReadbackService& service,
    ComputeService& compute,
    RuntimeFacadeGraph& graph,
    rhi::Device& device,
    const std::function<Result<ResolvedReadbackTexture>(OffscreenTargetHandle)>& resolve_offscreen
) {
    auto& impl = *service.impl_;
    std::lock_guard lock(impl.mutex);
    impl.active.clear();
    for (auto& [_, entry] : impl.entries) {
        if (entry->state.load(std::memory_order_acquire) != ReadbackState::Queued)
            continue;
        Result<ResolvedReadbackBuffer> source_buffer = Err(ErrorCode::InvalidState, "readback source is not a buffer");
        Result<ResolvedReadbackTexture> source_texture = Err(
            ErrorCode::InvalidState,
            "readback source is not a texture"
        );
        u64 source_offset{};
        TextureSubresource subresource;
        if (const auto* buffer_source = std::get_if<BufferReadback>(&entry->source)) {
            source_buffer = ResolveBuffer(compute, device, buffer_source->buffer);
            source_offset = buffer_source->offset;
            entry->staging_size = buffer_source->size;
            if (source_buffer
                && (buffer_source->size == 0 || buffer_source->offset > source_buffer->buffer->GetSize()
                    || buffer_source->size > source_buffer->buffer->GetSize() - buffer_source->offset))
                source_buffer = Err(ErrorCode::ValidationOutOfRange, "readback buffer range is invalid");
        } else if (const auto* texture_source = std::get_if<TextureReadback>(&entry->source)) {
            source_texture = ResolveTexture(compute, device, texture_source->texture);
            subresource = texture_source->subresource;
        } else {
            const auto& offscreen_source = std::get<OffscreenReadback>(entry->source);
            source_texture = resolve_offscreen(offscreen_source.target);
            subresource = offscreen_source.subresource;
        }
        if ((!source_buffer && !source_texture)
            || (std::holds_alternative<BufferReadback>(entry->source) && !source_buffer)) {
            const auto& error = std::holds_alternative<BufferReadback>(entry->source) ? source_buffer.error()
                                                                                      : source_texture.error();
            entry->error = std::string(error.Message());
            entry->state.store(ReadbackState::Failed, std::memory_order_release);
            continue;
        }

        if (source_texture) {
            if (source_texture->descriptor.sample_count != 1
                || subresource.mip_level >= source_texture->descriptor.mip_levels
                || subresource.array_layer >= source_texture->descriptor.depth_or_layers) {
                entry->error = "readback texture subresource is not directly copyable";
                entry->state.store(ReadbackState::Failed, std::memory_order_release);
                continue;
            }
            const auto extent = source_texture->descriptor.extent;
            const u32 mip_width = std::max(1U, extent.width >> subresource.mip_level);
            const u32 mip_height = std::max(1U, extent.height >> subresource.mip_level);
            entry->width = subresource.width == 0 ? mip_width - std::min(subresource.x, mip_width) : subresource.width;
            entry->height = subresource.height == 0 ? mip_height - std::min(subresource.y, mip_height)
                                                    : subresource.height;
            if (entry->width == 0 || entry->height == 0 || subresource.x > mip_width
                || entry->width > mip_width - subresource.x || subresource.y > mip_height
                || entry->height > mip_height - subresource.y) {
                entry->error = "readback texture subresource is out of range";
                entry->state.store(ReadbackState::Failed, std::memory_order_release);
                continue;
            }
            entry->format = source_texture->format;
            entry->row_bytes = entry->width * PixelBytes(entry->format);
            entry->padded_row_bytes = detail::AlignUp(entry->row_bytes, 256U);
            entry->staging_size = static_cast<u64>(entry->padded_row_bytes) * entry->height;
        }
        scope<rhi::Buffer> staging;
        auto created = device.CreateBuffer(
            {.size = entry->staging_size,
                .usage = rhi::BufferUsage::MapRead | rhi::BufferUsage::CopyDst,
                .label = "Runtime readback staging"}
        );
        if (!created) {
            entry->error = std::string(created.error().Message());
            entry->state.store(ReadbackState::Failed, std::memory_order_release);
            continue;
        }
        entry->staging = ref<rhi::Buffer>(std::move(*created));
        const GraphBufferDesc staging_descriptor{.label = "Runtime readback staging",
            .size = entry->staging_size,
            .usage = rhi::BufferUsage::MapRead | rhi::BufferUsage::CopyDst};
        entry->graph_staging = graph.builder.ImportBuffer(
            {staging_descriptor, ExternalState::Undefined, ExternalState::HostRead, true}
        );
        auto pass = graph.builder.AddPass("Runtime externally observable readback", PassKind::Copy);
        for (const auto& dependency : ComputeDependencies(compute))
            pass.DependsOn(dependency);
        const auto staging_version = pass.Write(entry->graph_staging, GraphAccess::CopyDestination);
        if (source_buffer) {
            entry->source_buffer = source_buffer->buffer;
            entry->source_buffer_descriptor = source_buffer->descriptor;
            entry->graph_source_buffer = graph.builder.ImportBuffer(
                {source_buffer->descriptor, ExternalState::ShaderRead, ExternalState::ShaderRead, true}
            );
            const auto source_version = graph.builder.Initial(*entry->graph_source_buffer);
            pass.Read(source_version, GraphAccess::CopySource)
                .SideEffect("readback-root")
                .Execute(
                    [source_version, staging_version, source_offset, size = entry->staging_size](
                        RenderGraphContext& context
                    ) -> Result<void> {
                        auto source = context.Buffer(source_version);
                        auto destination = context.Buffer(staging_version);
                        if (!source)
                            return Err(std::move(source).error());
                        if (!destination)
                            return Err(std::move(destination).error());
                        return context.CommandEncoder()
                            .CopyBufferToBuffer(source->get(), source_offset, destination->get(), 0, size);
                    }
                );
        } else {
            entry->source_texture = source_texture->texture;
            entry->source_texture_descriptor = source_texture->descriptor;
            ExternalTextureContract source_contract;
            source_contract.descriptor = source_texture->descriptor;
            source_contract.initial_state = ExternalState::ShaderRead;
            source_contract.final_state = ExternalState::ShaderRead;
            source_contract.frame_bound = true;
            entry->graph_source_texture = graph.builder.ImportTexture(std::move(source_contract));
            const TextureSubresourceRange range{
                .base_mip_level = subresource.mip_level,
                .mip_level_count = 1,
                .base_array_layer = subresource.array_layer,
                .array_layer_count = 1,
                .aspect = rhi::TextureAspect::All,
            };
            const auto source_version = graph.builder.Initial(*entry->graph_source_texture);
            pass.Read(source_version, GraphAccess::CopySource, range)
                .SideEffect("readback-root")
                .Execute(
                    [source_version,
                        staging_version,
                        subresource,
                        width = entry->width,
                        height = entry->height,
                        padded = entry->padded_row_bytes](RenderGraphContext& context) -> Result<void> {
                        auto source = context.Texture(source_version);
                        auto destination = context.Buffer(staging_version);
                        if (!source)
                            return Err(std::move(source).error());
                        if (!destination)
                            return Err(std::move(destination).error());
                        return context.CommandEncoder().CopyTextureToBuffer(
                            {.texture = &source->get(),
                                .mip_level = subresource.mip_level,
                                .origin = {subresource.x, subresource.y, subresource.array_layer}},
                            {{.offset = 0, .bytes_per_row = padded, .rows_per_image = height}, &destination->get()},
                            {width, height, 1}
                        );
                    }
                );
        }
        impl.active.push_back(entry);
        graph.has_work = true;
    }
    return Ok();
}

Result<void> RuntimeFacadeAccess::BindReadbacks(ReadbackService& service, GraphFrame& frame) {
    std::vector<std::shared_ptr<ReadbackService::Impl::Entry>> active;
    {
        std::lock_guard lock(service.impl_->mutex);
        active = service.impl_->active;
    }
    for (const auto& entry : active) {
        TRY_VOID(frame.Bind(
            {entry->graph_staging,
                entry->staging,
                {.label = "Runtime readback staging",
                    .size = entry->staging_size,
                    .usage = rhi::BufferUsage::MapRead | rhi::BufferUsage::CopyDst}}
        ));
        if (entry->graph_source_buffer)
            TRY_VOID(frame.Bind({*entry->graph_source_buffer, entry->source_buffer, entry->source_buffer_descriptor}));
        if (entry->graph_source_texture) {
            GraphTextureBinding binding;
            binding.resource = *entry->graph_source_texture;
            binding.texture = entry->source_texture;
            binding.view = ref<rhi::TextureView>(entry->source_texture->CreateView());
            binding.signature = entry->source_texture_descriptor;
            TRY_VOID(frame.Bind(std::move(binding)));
        }
    }
    return Ok();
}

void RuntimeFacadeAccess::PublishReadbacks(ReadbackService& service, const rhi::SubmissionTicket submission) {
    std::lock_guard lock(service.impl_->mutex);
    for (const auto& entry : service.impl_->active) {
        std::lock_guard entry_lock(entry->mutex);
        entry->submission = submission;
        entry->state.store(ReadbackState::Submitted, std::memory_order_release);
    }
    service.impl_->active.clear();
}

void RuntimeFacadeAccess::FailReadbacks(ReadbackService& service, const Error& error) {
    std::lock_guard lock(service.impl_->mutex);
    for (const auto& entry : service.impl_->active) {
        std::lock_guard entry_lock(entry->mutex);
        entry->error = std::string(error.Message());
        entry->state.store(ReadbackState::Failed, std::memory_order_release);
    }
    service.impl_->active.clear();
}

void RuntimeFacadeAccess::PollReadbacks(ReadbackService& service, const rhi::SubmissionEpoch completed) {
    auto& impl = *service.impl_;
    std::lock_guard lock(impl.mutex);
    for (auto& [_, entry] : impl.entries) {
        std::unique_lock entry_lock(entry->mutex);
        if (entry->release_requested.load(std::memory_order_acquire)
            && entry->state.load(std::memory_order_acquire) == ReadbackState::Consumed
            && completed.HasReached(entry->submission)) {
            entry->staging.reset();
            entry->source_buffer.reset();
            entry->source_texture.reset();
            continue;
        }
        ReadbackState expected = ReadbackState::Submitted;
        if (!completed.HasReached(entry->submission)
            || !entry->state.compare_exchange_strong(expected, ReadbackState::Mapping, std::memory_order_acq_rel))
            continue;
        const auto retained = entry;
        entry_lock.unlock();
        const auto future = entry->staging->MapAsync(
            rhi::MapMode::Read,
            0,
            static_cast<size_t>(entry->staging_size),
            rhi::CallbackMode::AllowSpontaneous,
            [retained](const rhi::MapAsyncStatus status, const std::string_view message) {
                std::lock_guard entry_lock(retained->mutex);
                if (retained->state.load(std::memory_order_acquire) == ReadbackState::DeviceLost)
                    return;
                if (status != rhi::MapAsyncStatus::Success) {
                    retained->error = message.empty() ? "asynchronous readback mapping failed" : std::string(message);
                    retained->state.store(ReadbackState::Failed, std::memory_order_release);
                    return;
                }
                if (retained->release_requested.load(std::memory_order_acquire)) {
                    retained->staging->Unmap();
                    retained->staging.reset();
                    retained->state.store(ReadbackState::Consumed, std::memory_order_release);
                    return;
                }
                std::vector<std::byte> mapped(static_cast<size_t>(retained->staging_size));
                const auto read = retained->staging->ReadMappedRange(0, mapped.data(), mapped.size());
                retained->staging->Unmap();
                if (!read) {
                    retained->error = std::string(read.error().Message());
                    retained->state.store(ReadbackState::Failed, std::memory_order_release);
                    return;
                }
                if (std::holds_alternative<BufferReadback>(retained->source)) {
                    retained->result = createRef<const ReadbackResult>(std::move(mapped));
                } else {
                    ReadbackImage image;
                    image.width = retained->width;
                    image.height = retained->height;
                    image.row_bytes = retained->row_bytes;
                    image.format = retained->format;
                    image.pixels.resize(static_cast<size_t>(image.row_bytes) * image.height);
                    for (u32 row = 0; row < image.height; ++row)
                        std::memcpy(
                            image.pixels.data() + static_cast<size_t>(row) * image.row_bytes,
                            mapped.data() + static_cast<size_t>(row) * retained->padded_row_bytes,
                            image.row_bytes
                        );
                    retained->result = createRef<const ReadbackResult>(std::move(image));
                }
                retained->staging.reset();
                retained->state.store(ReadbackState::Ready, std::memory_order_release);
            }
        );
        (void)future;
    }
    std::erase_if(impl.entries, [](const auto& item) {
        const auto entry = item.second;
        std::lock_guard entry_lock(entry->mutex);
        return entry->release_requested.load(std::memory_order_acquire)
               && entry->state.load(std::memory_order_acquire) == ReadbackState::Consumed && entry->staging == nullptr;
    });
}

void RuntimeFacadeAccess::DeviceLostReadbacks(ReadbackService& service) noexcept {
    std::lock_guard lock(service.impl_->mutex);
    for (auto& [_, entry] : service.impl_->entries) {
        std::lock_guard entry_lock(entry->mutex);
        const auto state = entry->state.load(std::memory_order_acquire);
        if (state == ReadbackState::Queued || state == ReadbackState::Submitted || state == ReadbackState::Mapping)
            entry->state.store(ReadbackState::DeviceLost, std::memory_order_release);
    }
}

void RuntimeFacadeAccess::CancelReadbacks(ReadbackService& service) noexcept {
    std::lock_guard lock(service.impl_->mutex);
    for (auto& [_, entry] : service.impl_->entries) {
        std::lock_guard entry_lock(entry->mutex);
        if (entry->state.load(std::memory_order_acquire) == ReadbackState::Queued)
            entry->state.store(ReadbackState::Cancelled, std::memory_order_release);
    }
}

} // namespace woki::gfx
