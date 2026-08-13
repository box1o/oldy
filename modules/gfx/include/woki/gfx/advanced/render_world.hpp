#pragma once

#include <memory>

#include <woki/math.hpp>

#include "render_scene.hpp"

namespace woki::gfx {

class RenderWorldSnapshot final {
public:
    struct Objects final {
        std::vector<SceneHandle> scenes;
        std::vector<RenderObjectId> ids;
        std::vector<u64> versions;
        std::vector<math::mat4f> transforms;
        std::vector<math::mat4f> previous_transforms;
        std::vector<RenderBounds> bounds;
        std::vector<MeshHandle> meshes;
        std::vector<MaterialInstanceHandle> materials;
        std::vector<SkinPaletteHandle> palettes;
        std::vector<u64> visibility_masks;
        std::vector<u64> layers;
        std::vector<RenderObjectFlags> flags;
        std::vector<MaterialPhase> material_phases;
        std::vector<RenderLodState> lods;
        std::vector<std::array<u32, 4>> feature_payload_ids;
    };

    [[nodiscard]] u64 Generation() const noexcept {
        return generation_;
    }

    [[nodiscard]] SceneHandle Scene() const noexcept {
        return scene_;
    }

    [[nodiscard]] const Objects& ObjectData() const noexcept {
        return objects_;
    }

    [[nodiscard]] std::span<const RenderLightData> Lights() const noexcept {
        return lights_;
    }

    [[nodiscard]] std::span<const RenderView> Views() const noexcept {
        return views_;
    }

    [[nodiscard]] std::optional<u32> Resolve(RenderObjectId id) const noexcept;

private:
    friend class RenderWorldBuilder;

    struct Slot {
        u32 generation{};
        u32 dense{};
        bool live{};
    };

    u64 generation_{};
    SceneHandle scene_;
    Objects objects_;
    std::vector<Slot> object_slots_;
    std::vector<RenderLightId> light_ids_;
    std::vector<SceneHandle> light_scenes_;
    std::vector<u32> light_generations_;
    std::vector<u64> light_versions_;
    std::vector<RenderLightData> lights_;
    std::vector<ViewId> view_ids_;
    std::vector<u32> view_generations_;
    std::vector<u64> view_versions_;
    std::vector<RenderView> views_;
};

class RenderWorldBuilder final {
public:
    RenderWorldBuilder();
    explicit RenderWorldBuilder(SceneHandle scene);
    [[nodiscard]] Result<std::shared_ptr<const RenderWorldSnapshot>> Apply(std::span<const RenderChange> changes);

    [[nodiscard]] std::shared_ptr<const RenderWorldSnapshot> Current() const noexcept {
        return current_;
    }

private:
    std::shared_ptr<const RenderWorldSnapshot> current_;
};

class RenderExtractionAdapter {
public:
    virtual ~RenderExtractionAdapter() = default;
    [[nodiscard]] virtual Result<void> Extract(RenderScene&, RenderChangeJournal::Writer&) = 0;
};

struct RenderScenePreparationContext final {
    const RenderWorldSnapshot& world;
    std::span<const RenderView> views;
};

} // namespace woki::gfx
