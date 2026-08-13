#pragma once

#include <deque>
#include <functional>
#include <mutex>
#include <variant>

#include <woki/rhi/objects.hpp>

#include "resource.hpp"

namespace woki::gfx {

struct UploadBudget final {
    u64 queued_bytes{64ull * 1024ull * 1024ull};
    u32 queued_requests{4096};
    u64 batch_bytes{16ull * 1024ull * 1024ull};
    u32 batch_requests{256};
};

class UploadPublicationToken final {
public:
    using Completion = std::function<void(bool, ResidencyRecord, std::string)>;
    using CompletionQueue = std::function<void(std::function<void()>)>;

    [[nodiscard]] static std::shared_ptr<UploadPublicationToken> Create(ResidencyRecord initial, u32 required_uploads, Completion completion = {}, CompletionQueue completion_queue = {});
    [[nodiscard]] ResidencyRecord Snapshot() const noexcept;
    [[nodiscard]] bool Expected() const noexcept;
    void MarkPending() noexcept;
    void Complete(u64 bytes);
    void Fail(std::string diagnostic);
    void MarkDeviceLost() noexcept;

private:
    UploadPublicationToken(ResidencyRecord initial, u32 required_uploads, Completion completion, CompletionQueue completion_queue);
    void Notify(bool success, ResidencyRecord snapshot, std::string diagnostic);

    mutable std::mutex mutex_;
    ResidencyRecord record_;
    const ContentVersion expected_content_;
    const ResidencyVersion expected_residency_;
    u32 remaining_{};
    bool terminal_{};
    Completion completion_;
    CompletionQueue completion_queue_;
};

struct BufferUploadRequest final {
    ref<rhi::Buffer> target;
    u64 offset{};
    std::vector<std::byte> bytes;
    std::shared_ptr<UploadPublicationToken> publication;
};

struct TextureUploadRequest final {
    ref<rhi::Texture> target;
    u32 mip_level{};
    rhi::Origin3D origin;
    rhi::TextureAspect aspect{rhi::TextureAspect::All};
    rhi::TexelCopyBufferLayout layout;
    rhi::Extent3D extent;
    std::vector<std::byte> bytes;
    std::shared_ptr<UploadPublicationToken> publication;
};

struct UploadTicket final {
    WorkVersion work;
    rhi::SubmissionTicket submission;
};

struct UploadStats final {
    u64 queued_bytes{};
    u32 queued_requests{};
    u64 in_flight_bytes{};
    u32 in_flight_requests{};
    u64 rejected_requests{};
    u64 stale_requests{};
    u64 published_requests{};
};

class UploadScheduler final {
public:
    UploadScheduler(ref<rhi::Device> device, UploadBudget budget = {});

    [[nodiscard]] Result<WorkVersion> Enqueue(BufferUploadRequest request);
    [[nodiscard]] Result<std::vector<WorkVersion>> EnqueueBatch(std::vector<BufferUploadRequest> requests);
    [[nodiscard]] Result<WorkVersion> Enqueue(TextureUploadRequest request);
    [[nodiscard]] Result<std::vector<UploadTicket>> PrepareAndSubmit();
    size_t PublishCompleted(rhi::SubmissionEpoch completed);
    void MarkDeviceLost() noexcept;

    [[nodiscard]] UploadStats Stats() const noexcept;

private:
    using RequestData = std::variant<BufferUploadRequest, TextureUploadRequest>;

    struct Request final {
        WorkVersion work;
        RequestData data;
    };

    struct Batch final {
        rhi::SubmissionTicket submission;
        std::vector<Request> requests;
        std::vector<scope<rhi::Buffer>> staging;
        u64 bytes{};
    };

    [[nodiscard]] Result<WorkVersion> Enqueue(RequestData request);
    [[nodiscard]] static u64 ByteSize(const RequestData& request) noexcept;
    [[nodiscard]] static std::shared_ptr<UploadPublicationToken> Publication(const RequestData& request) noexcept;
    [[nodiscard]] static bool Expected(const RequestData& request) noexcept;

    ref<rhi::Device> device_;
    UploadBudget budget_;
    std::deque<Request> queued_;
    std::vector<Batch> in_flight_;
    u64 queued_bytes_{};
    u64 in_flight_bytes_{};
    WorkVersion next_work_{1};
    u64 rejected_{};
    u64 stale_{};
    u64 published_{};
    bool device_lost_{};
};

} // namespace woki::gfx
