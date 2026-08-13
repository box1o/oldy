#include <algorithm>
#include <utility>

#include <woki/gfx/advanced/render_scene.hpp>

namespace woki::gfx {

RenderChangeJournal::Writer::Writer(RenderChangeJournal& journal)
    : journal_(&journal),
      epoch_(journal.Epoch()) {
    changes_.reserve(std::min<size_t>(journal.limits_.writer_changes, 256));
}

RenderChangeJournal::Writer::Writer(Writer&& other) noexcept
    : journal_(std::exchange(other.journal_, nullptr)),
      epoch_(other.epoch_),
      changes_(std::move(other.changes_)) {}

RenderChangeJournal::Writer& RenderChangeJournal::Writer::operator=(Writer&& other) noexcept {
    if (this != &other) {
        Cancel();
        journal_ = std::exchange(other.journal_, nullptr);
        epoch_ = other.epoch_;
        changes_ = std::move(other.changes_);
    }
    return *this;
}

RenderChangeJournal::Writer::~Writer() {
    Cancel();
}

Result<void> RenderChangeJournal::Writer::Push(RenderChangePayload payload) {
    if (journal_ == nullptr)
        return Err(ErrorCode::InvalidState, "render journal writer has no journal");
    const u64 current_epoch = journal_->Epoch();
    if (current_epoch != epoch_) {
        TRY_VOID(Flush());
        epoch_ = current_epoch;
    }
    if (changes_.size() >= journal_->limits_.writer_changes)
        TRY_VOID(Flush());
    changes_.push_back(
        {
            .scene = {},
            .sequence = journal_->sequence_.fetch_add(1, std::memory_order_relaxed),
            .epoch = epoch_,
            .payload = std::move(payload),
        }
    );
    return Ok();
}

Result<void> RenderChangeJournal::Writer::Flush() {
    if (journal_ == nullptr || changes_.empty())
        return Ok();
    TRY_VOID(journal_->Merge(changes_));
    changes_.clear();
    epoch_ = journal_->Epoch();
    return Ok();
}

void RenderChangeJournal::Writer::Cancel() noexcept {
    changes_.clear();
    journal_ = nullptr;
}

RenderChangeJournal::RenderChangeJournal()
    : RenderChangeJournal(Limits{}) {}

RenderChangeJournal::RenderChangeJournal(const Limits limits)
    : limits_(limits) {
    limits_.writer_changes = std::max<size_t>(1, limits_.writer_changes);
    limits_.pending_changes = std::max(limits_.writer_changes, limits_.pending_changes);
}

RenderChangeJournal::Writer RenderChangeJournal::CreateWriter() {
    return Writer(*this);
}

Result<void> RenderChangeJournal::Merge(std::vector<RenderChange>& changes) {
    std::lock_guard lock(merge_mutex_);
    if (changes.size() > limits_.pending_changes - std::min(limits_.pending_changes, pending_.size()))
        return Err(
            ErrorCode::FailedToAcquireResource,
            "render change journal is full; freeze and drain it before retrying"
        );
    pending_.insert(pending_.end(), std::make_move_iterator(changes.begin()), std::make_move_iterator(changes.end()));
    return Ok();
}

Result<std::vector<RenderChange>> RenderChangeJournal::FreezeAndDrain() {
    epoch_.fetch_add(1, std::memory_order_acq_rel);
    std::vector<RenderChange> result;
    {
        std::lock_guard lock(merge_mutex_);
        result.swap(pending_);
    }
    std::sort(result.begin(), result.end(), [](const RenderChange& left, const RenderChange& right) {
        return left.sequence < right.sequence;
    });
    return Ok(std::move(result));
}

RenderScene::RenderScene()
    : RenderScene(RenderChangeJournal::Limits{}) {}

RenderScene::RenderScene(const RenderChangeJournal::Limits limits)
    : journal_(limits) {}

RenderScene::RenderScene(const SceneHandle scene, const u32 id_namespace, const RenderChangeJournal::Limits limits)
    : journal_(limits),
      scene_(scene),
      id_base_(id_namespace << 20U) {}

Result<std::vector<RenderChange>> RenderScene::FreezeAndDrain() {
    auto changes = journal_.FreezeAndDrain();
    if (!changes)
        return changes;
    for (auto& change : *changes)
        change.scene = scene_;
    return changes;
}

template <typename Tag>
Handle<Tag> RenderScene::Allocate(std::vector<IdSlot>& slots, std::vector<u32>& free) {
    std::lock_guard lock(ids_mutex_);
    u32 index{};
    if (free.empty()) {
        index = static_cast<u32>(slots.size());
        slots.push_back({});
    } else {
        index = free.back();
        free.pop_back();
    }
    slots[index].live = true;
    return Handle<Tag>::Create(id_base_ + index, slots[index].generation);
}

template <typename Tag>
Result<void> RenderScene::Release(const Handle<Tag> id, std::vector<IdSlot>& slots, std::vector<u32>& free) {
    std::lock_guard lock(ids_mutex_);
    if (!id.IsValid() || id.Index() < id_base_)
        return Err(ErrorCode::ValidationInvalidState, "render scene handle is stale");
    const u32 index = id.Index() - id_base_;
    if (index >= slots.size() || !slots[index].live || slots[index].generation != id.Generation())
        return Err(ErrorCode::ValidationInvalidState, "render scene handle is stale");
    auto& slot = slots[index];
    slot.live = false;
    ++slot.generation;
    if (slot.generation == 0)
        slot.generation = 1;
    free.push_back(index);
    return Ok();
}

Result<RenderObjectId> RenderScene::CreateObject(RenderChangeJournal::Writer& writer, RenderObjectData data) {
    const auto id = Allocate<RenderObjectTag>(objects_, free_objects_);
    auto pushed = writer.Push(RenderObjectChange{SceneChangeKind::Create, id, 1, std::move(data), {}});
    if (!pushed) {
        static_cast<void>(Release(id, objects_, free_objects_));
        return Err(std::move(pushed).error());
    }
    return Ok(id);
}

Result<void> RenderScene::UpdateObject(
    RenderChangeJournal::Writer& writer,
    const RenderObjectId id,
    const u64 version,
    RenderObjectPatch patch
) {
    return writer.Push(RenderObjectChange{SceneChangeKind::Update, id, version, std::nullopt, std::move(patch)});
}

Result<void> RenderScene::DestroyObject(
    RenderChangeJournal::Writer& writer,
    const RenderObjectId id,
    const u64 version
) {
    TRY_VOID(QueueDestroyObject(writer, id, version));
    return ReleaseObject(id);
}

Result<void> RenderScene::QueueDestroyObject(
    RenderChangeJournal::Writer& writer,
    const RenderObjectId id,
    const u64 version
) {
    return writer.Push(RenderObjectChange{SceneChangeKind::Destroy, id, version, std::nullopt, {}});
}

Result<void> RenderScene::ReleaseObject(const RenderObjectId id) {
    return Release(id, objects_, free_objects_);
}

Result<RenderLightId> RenderScene::CreateLight(RenderChangeJournal::Writer& writer, RenderLightData data) {
    const auto id = Allocate<RenderLightTag>(lights_, free_lights_);
    auto pushed = writer.Push(RenderLightChange{SceneChangeKind::Create, id, 1, std::move(data)});
    if (!pushed) {
        static_cast<void>(Release(id, lights_, free_lights_));
        return Err(std::move(pushed).error());
    }
    return Ok(id);
}

Result<void> RenderScene::UpdateLight(
    RenderChangeJournal::Writer& writer,
    const RenderLightId id,
    const u64 version,
    RenderLightData data
) {
    return writer.Push(RenderLightChange{SceneChangeKind::Update, id, version, std::move(data)});
}

Result<void> RenderScene::DestroyLight(RenderChangeJournal::Writer& writer, const RenderLightId id, const u64 version) {
    TRY_VOID(QueueDestroyLight(writer, id, version));
    return ReleaseLight(id);
}

Result<void> RenderScene::QueueDestroyLight(
    RenderChangeJournal::Writer& writer,
    const RenderLightId id,
    const u64 version
) {
    return writer.Push(RenderLightChange{SceneChangeKind::Destroy, id, version, std::nullopt});
}

Result<void> RenderScene::ReleaseLight(const RenderLightId id) {
    return Release(id, lights_, free_lights_);
}

Result<ViewId> RenderScene::CreateView(RenderChangeJournal::Writer& writer, RenderView view) {
    const auto id = Allocate<RenderViewTag>(views_, free_views_);
    view.id = id;
    if (!view.history.IsValid())
        view.history = ViewHistoryId::Create(id.Index(), id.Generation());
    auto pushed = writer.Push(RenderViewChange{SceneChangeKind::Create, id, 1, std::move(view)});
    if (!pushed) {
        static_cast<void>(Release(id, views_, free_views_));
        return Err(std::move(pushed).error());
    }
    return Ok(id);
}

Result<void> RenderScene::UpdateView(
    RenderChangeJournal::Writer& writer,
    const ViewId id,
    const u64 version,
    RenderView view
) {
    view.id = id;
    return writer.Push(RenderViewChange{SceneChangeKind::Update, id, version, std::move(view)});
}

Result<void> RenderScene::DestroyView(RenderChangeJournal::Writer& writer, const ViewId id, const u64 version) {
    TRY_VOID(writer.Push(RenderViewChange{SceneChangeKind::Destroy, id, version, std::nullopt}));
    return Release(id, views_, free_views_);
}

} // namespace woki::gfx
