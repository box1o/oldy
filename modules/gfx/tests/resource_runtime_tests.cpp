#include <atomic>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include <woki/gfx/advanced.hpp>

#include "null_rhi_fixture.hpp"

using namespace woki;

TEST_CASE("Resource registries retain borrowed generations and enforce owner mutation") {
    struct Record final {
        u64 version{};
    };
    struct Tag;
    gfx::ResourceRegistry<Record, Tag> registry;
    const auto asset = asset::AssetId::FromName("engine://tests/resource");
    auto created = registry.Create(asset, {1});
    REQUIRE(created);
    const auto borrowed = registry.Borrow(*created);
    REQUIRE(borrowed);

    REQUIRE(registry.Publish(*created, {2}));
    CHECK(borrowed->version == 1);
    CHECK(registry.Borrow(*created)->version == 2);
    CHECK(registry.Find(asset) == *created);
    CHECK_FALSE(registry.Create(asset, {3}));

    bool foreign_publish_succeeded{};
    std::thread foreign([&] { foreign_publish_succeeded = registry.Publish(*created, {4}).has_value(); });
    foreign.join();
    CHECK_FALSE(foreign_publish_succeeded);
    REQUIRE(registry.Remove(*created));
    CHECK_FALSE(registry.Borrow(*created));
}

TEST_CASE("Frame rings and deferred releases wait for submission proof") {
    gfx::FrameContextRing ring(2, 1024);
    auto first = ring.Acquire(rhi::SubmissionEpoch{});
    REQUIRE(first);
    REQUIRE(first->get().Submit(rhi::SubmissionTicket(2)));
    auto second = ring.Acquire(rhi::SubmissionEpoch{});
    REQUIRE(second);
    REQUIRE(second->get().Submit(rhi::SubmissionTicket(4)));
    CHECK_FALSE(ring.Acquire(rhi::SubmissionEpoch(1)));
    auto retired = ring.Acquire(rhi::SubmissionEpoch(2));
    REQUIRE(retired);
    CHECK(retired->get().Epoch().Value() == 3);

    struct Lifetime final {
        ref<std::atomic_uint> destructions;

        ~Lifetime() {
            ++*destructions;
        }
    };

    auto destructions = createRef<std::atomic_uint>(0);
    gfx::DeferredReleaseQueue releases;
    releases.Retire(createScope<Lifetime>(Lifetime{destructions}), rhi::SubmissionTicket(4));
    CHECK(releases.Collect(rhi::SubmissionEpoch(3)) == 0);
    CHECK(destructions->load() == 0);
    CHECK(releases.Collect(rhi::SubmissionEpoch(4)) == 1);
    CHECK(destructions->load() == 1);
}

TEST_CASE("Buffer pools coalesce ranges and defer reuse until Null submission completes") {
    gfx::test::NullDevice fixture;
    auto releases = createRef<gfx::DeferredReleaseQueue>();
    auto pool = gfx::BufferPool::Create(fixture.device, {.pool_class = gfx::BufferPoolClass::Upload, .size = 256, .alignment = 16, .usage = rhi::BufferUsage::CopySrc | rhi::BufferUsage::CopyDst, .label = "test-pool"},
        releases);
    REQUIRE(pool);
    auto first = (*pool)->Allocate(64);
    auto second = (*pool)->Allocate(64);
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first->offset == 0);
    CHECK(second->offset == 64);
    REQUIRE((*pool)->Free(first->allocation, rhi::SubmissionTicket(3)));
    CHECK((*pool)->Stats().pending_free_bytes == 64);
    CHECK((*pool)->Collect(rhi::SubmissionEpoch(2)) == 0);
    CHECK((*pool)->Collect(rhi::SubmissionEpoch(3)) == 1);
    auto reused = (*pool)->Allocate(64);
    REQUIRE(reused);
    CHECK(reused->offset == 0);
    CHECK(fixture.diagnostics.empty());
}

TEST_CASE("Upload scheduler publishes completed bytes and converts device loss to lost residency") {
    gfx::test::NullDevice fixture;
    auto target = fixture.device->CreateBuffer({.size = 64, .usage = rhi::BufferUsage::CopyDst | rhi::BufferUsage::CopySrc, .label = "upload-target"});
    REQUIRE(target);
    gfx::ResidencyRecord initial{.resource = gfx::ResourceState::Ready, .residency = gfx::ResidencyState::NonResident, .content_version = gfx::ContentVersion(7), .residency_version = gfx::ResidencyVersion(2)};
    auto publication = gfx::UploadPublicationToken::Create(initial, 1);
    gfx::UploadScheduler uploads(fixture.device, {.queued_bytes = 64, .queued_requests = 2, .batch_bytes = 64, .batch_requests = 2});
    REQUIRE(uploads.Enqueue({.target = ref<rhi::Buffer>(std::move(*target)), .bytes = std::vector<std::byte>(16, std::byte{0x2a}), .publication = publication}));
    auto submitted = uploads.PrepareAndSubmit();
    REQUIRE(submitted);
    REQUIRE(submitted->size() == 1);
    CHECK(uploads.PublishCompleted(rhi::SubmissionEpoch(submitted->front().submission.Value())) == 1);
    CHECK(publication->Snapshot().residency == gfx::ResidencyState::Resident);
    CHECK(publication->Snapshot().estimated_bytes == 16);
    CHECK(gfx::test::LogContains(*fixture.null_device, "queue.submit"));

    auto pending = gfx::UploadPublicationToken::Create(initial, 1);
    pending->MarkPending();
    pending->MarkDeviceLost();
    CHECK(pending->Snapshot().resource == gfx::ResourceState::RebuildNeeded);
    CHECK(pending->Snapshot().residency == gfx::ResidencyState::Lost);
}

TEST_CASE("Temporal histories rotate after submission and invalidate on device loss") {
    gfx::test::NullDevice fixture;
    auto releases = createRef<gfx::DeferredReleaseQueue>();
    gfx::RenderHistoryRegistry histories(fixture.device, releases, 3);
    gfx::GraphTextureDesc descriptor{.label = "history",
        .extent = gfx::GraphExtent::Fixed(16, 16),
        .format = rhi::TextureFormat::RGBA8Unorm,
        .usage = rhi::TextureUsage::TextureBinding | rhi::TextureUsage::RenderAttachment};
    const gfx::RenderHistoryKey key{gfx::ViewHistoryId::Create(1, 1), gfx::TemporalSemantic::HdrColor};
    auto first = histories.Acquire(key, descriptor, 1, false);
    REQUIRE(first);
    CHECK_FALSE(first->valid);
    histories.Submitted(key, rhi::SubmissionTicket(1), 1);
    auto second = histories.Acquire(key, descriptor, 2, false);
    REQUIRE(second);
    CHECK(second->valid);
    CHECK(second->previous == first->current);
    histories.MarkDeviceLost();
    auto rebuilt = histories.Acquire(key, descriptor, 3, false);
    REQUIRE(rebuilt);
    CHECK_FALSE(rebuilt->valid);
}

TEST_CASE("Runtime services propagate device loss across frame rings pools and uploads") {
    gfx::test::NullDevice fixture;
    gfx::RenderRuntimeDesc descriptor;
    descriptor.frames_in_flight = 2;
    descriptor.frame_scratch_bytes = 1024;
    descriptor.buffer_pools = {gfx::DefaultBufferPoolDesc(gfx::BufferPoolClass::Dynamic, 4096)};
    auto runtime = gfx::RenderRuntimeServices::Create(fixture.device, descriptor);
    REQUIRE(runtime);
    auto frame = (*runtime)->AcquireFrame();
    REQUIRE(frame);
    REQUIRE(frame->get().Cancel());
    CHECK_FALSE((*runtime)->Diagnostics().device_lost);
    (*runtime)->MarkDeviceLost();
    const auto diagnostics = (*runtime)->Diagnostics();
    CHECK(diagnostics.device_lost);
    REQUIRE(diagnostics.pools.size() == 1);
    CHECK(diagnostics.pools.front().residency_lost);
    CHECK_FALSE((*runtime)->Uploads().PrepareAndSubmit());
}
