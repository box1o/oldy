#pragma once

#include <array>
#include <atomic>
#include <mutex>
#include <optional>
#include <variant>

#include "../scene.hpp"
#include "../view.hpp"

namespace woki::gfx {

enum class SceneChangeKind : u8 { Create, Update, Destroy };

struct RenderObjectChange final {
    SceneChangeKind kind{SceneChangeKind::Update};
    RenderObjectId id;
    u64 version{};
    std::optional<RenderObjectData> create;
    RenderObjectPatch patch;
};

struct RenderLightChange final {
    SceneChangeKind kind{SceneChangeKind::Update};
    RenderLightId id;
    u64 version{};
    std::optional<RenderLightData> value;
};

struct RenderViewChange final {
    SceneChangeKind kind{SceneChangeKind::Update};
    ViewId id;
    u64 version{};
    std::optional<RenderView> value;
};

using RenderChangePayload = std::variant<RenderObjectChange, RenderLightChange, RenderViewChange>;

struct RenderChange final {
    SceneHandle scene;
    u64 sequence{};
    u64 epoch{};
    RenderChangePayload payload;
};

class RenderChangeJournal final {
public:
    struct Limits final {
        size_t writer_changes{4096};
        size_t pending_changes{65536};
    };

    class Writer final {
    public:
        Writer() = default;
        Writer(Writer&&) noexcept;
        Writer& operator=(Writer&&) noexcept;
        ~Writer();
        Writer(const Writer&) = delete;
        Writer& operator=(const Writer&) = delete;

        [[nodiscard]] Result<void> Push(RenderChangePayload change);
        [[nodiscard]] Result<void> Flush();
        void Cancel() noexcept;

        [[nodiscard]] size_t Buffered() const noexcept {
            return changes_.size();
        }

    private:
        friend class RenderChangeJournal;
        explicit Writer(RenderChangeJournal& journal);
        RenderChangeJournal* journal_{};
        u64 epoch_{};
        std::vector<RenderChange> changes_;
    };

    RenderChangeJournal();
    explicit RenderChangeJournal(Limits limits);
    [[nodiscard]] Writer CreateWriter();
    [[nodiscard]] Result<std::vector<RenderChange>> FreezeAndDrain();

    [[nodiscard]] u64 Epoch() const noexcept {
        return epoch_.load(std::memory_order_acquire);
    }

private:
    [[nodiscard]] Result<void> Merge(std::vector<RenderChange>& changes);
    Limits limits_;
    std::atomic<u64> epoch_{1};
    std::atomic<u64> sequence_{1};
    std::mutex merge_mutex_;
    std::vector<RenderChange> pending_;
};

class RenderScene final {
public:
    RenderScene();
    explicit RenderScene(RenderChangeJournal::Limits limits);
    RenderScene(SceneHandle scene, u32 id_namespace, RenderChangeJournal::Limits limits = {});

    [[nodiscard]] RenderChangeJournal::Writer CreateWriter() {
        return journal_.CreateWriter();
    }

    [[nodiscard]] Result<RenderObjectId> CreateObject(RenderChangeJournal::Writer& writer, RenderObjectData data);
    [[nodiscard]] Result<void> UpdateObject(RenderChangeJournal::Writer& writer, RenderObjectId id, u64 version, RenderObjectPatch patch);
    [[nodiscard]] Result<void> DestroyObject(RenderChangeJournal::Writer& writer, RenderObjectId id, u64 version);
    [[nodiscard]] Result<void> QueueDestroyObject(RenderChangeJournal::Writer& writer, RenderObjectId id, u64 version);
    [[nodiscard]] Result<void> ReleaseObject(RenderObjectId id);
    [[nodiscard]] Result<RenderLightId> CreateLight(RenderChangeJournal::Writer& writer, RenderLightData data);
    [[nodiscard]] Result<void> UpdateLight(RenderChangeJournal::Writer& writer, RenderLightId id, u64 version, RenderLightData data);
    [[nodiscard]] Result<void> DestroyLight(RenderChangeJournal::Writer& writer, RenderLightId id, u64 version);
    [[nodiscard]] Result<void> QueueDestroyLight(RenderChangeJournal::Writer& writer, RenderLightId id, u64 version);
    [[nodiscard]] Result<void> ReleaseLight(RenderLightId id);
    [[nodiscard]] Result<ViewId> CreateView(RenderChangeJournal::Writer& writer, RenderView view);
    [[nodiscard]] Result<void> UpdateView(RenderChangeJournal::Writer& writer, ViewId id, u64 version, RenderView view);
    [[nodiscard]] Result<void> DestroyView(RenderChangeJournal::Writer& writer, ViewId id, u64 version);

    [[nodiscard]] Result<std::vector<RenderChange>> FreezeAndDrain();

private:
    struct IdSlot {
        u32 generation{1};
        bool live{};
    };

    template <typename Tag>
    Handle<Tag> Allocate(std::vector<IdSlot>& slots, std::vector<u32>& free);
    template <typename Tag>
    Result<void> Release(Handle<Tag> id, std::vector<IdSlot>& slots, std::vector<u32>& free);
    RenderChangeJournal journal_;
    SceneHandle scene_;
    std::mutex ids_mutex_;
    std::vector<IdSlot> objects_;
    std::vector<IdSlot> lights_;
    std::vector<IdSlot> views_;
    std::vector<u32> free_objects_;
    std::vector<u32> free_lights_;
    std::vector<u32> free_views_;
    u32 id_base_{};
};

} // namespace woki::gfx
