#pragma once

#include <unordered_map>
#include <unordered_set>

#include <woki/core.hpp>

#include "material_product.hpp"
#include "texture_library.hpp"
#include "../handles.hpp"

namespace woki::gfx {

struct MaterialDefinitionTag;
struct MaterialGpuTag;
using MaterialDefinitionHandle = Handle<MaterialDefinitionTag>;
using MaterialGpuHandle = Handle<MaterialGpuTag>;

enum class MaterialState : u8 { Empty, Loading, Ready, Failed, Reloading };

struct MaterialDefinitionGeneration final {
    MaterialDefinitionProduct product;
    u64 version{};
    ContentHash product_hash;
};

struct BorrowedMaterialDefinition final {
    ref<const MaterialDefinitionGeneration> generation;

    [[nodiscard]] const MaterialDefinitionProduct& Get() const noexcept {
        return generation->product;
    }

    [[nodiscard]] u64 Version() const noexcept {
        return generation->version;
    }
};

struct MaterialInstanceRecord final {
    asset::AssetId asset_id;
    MaterialDefinitionHandle definition;
    std::map<MaterialPropertyId, MaterialValue> authored_values;
    std::map<MaterialPropertyId, MaterialValue> runtime_overrides;
    std::map<MaterialPropertyId, MaterialValue> values;
    ContentHash source_product_hash;
    u64 source_generation{};
    u64 content_version{1};
    bool dirty{true};
    std::string diagnostic;
};

class MaterialLibrary final {
public:
    using DefinitionReloadObserver = std::function<void(asset::AssetId, ContentHash)>;
    explicit MaterialLibrary(asset::AssetManager& assets);
    [[nodiscard]] Result<MaterialDefinitionHandle> RequestDefinition(asset::AssetId id);
    [[nodiscard]] Result<MaterialInstanceHandle> RequestInstance(asset::AssetId id);
    [[nodiscard]] Result<MaterialDefinitionHandle> PublishDefinition(const asset::Product& product);
    [[nodiscard]] Result<MaterialInstanceHandle> PublishInstance(const asset::Product& product);
    [[nodiscard]] Result<void> QueueDefinitionReload(MaterialDefinitionHandle handle, const asset::Product& candidate);
    [[nodiscard]] Result<void> PublishReloads();
    [[nodiscard]] Result<void> Pump();
    [[nodiscard]] Result<void> Set(MaterialInstanceHandle instance, MaterialPropertyId property, MaterialValue value);
    [[nodiscard]] Result<BorrowedMaterialDefinition> Borrow(MaterialDefinitionHandle handle) const;
    [[nodiscard]] const MaterialInstanceRecord* TryGet(MaterialInstanceHandle handle) const noexcept;
    [[nodiscard]] MaterialInstanceRecord* TryGetMutable(MaterialInstanceHandle handle) noexcept;
    [[nodiscard]] std::vector<MaterialInstanceHandle> DirtyInstances() const;
    [[nodiscard]] std::optional<MaterialState> State(MaterialDefinitionHandle handle) const noexcept;
    [[nodiscard]] std::string_view Diagnostic(MaterialDefinitionHandle handle) const noexcept;
    void MarkPrepared(MaterialInstanceHandle handle, u64 content_version);
    [[nodiscard]] std::optional<MaterialDefinitionHandle> FindDefinition(asset::AssetId id) const noexcept;

    void SetDefinitionReloadObserver(DefinitionReloadObserver observer) {
        reload_observer_ = std::move(observer);
    }

private:
    struct DefinitionSlot {
        MaterialState state{MaterialState::Empty};
        ref<const MaterialDefinitionGeneration> current;
        std::string diagnostic;
    };

    struct InstanceSlot {
        MaterialInstanceRecord record;
    };

    struct Reload {
        MaterialDefinitionHandle handle;
        MaterialDefinitionProduct product;
        ContentHash hash;
    };

    [[nodiscard]] bool Valid(MaterialDefinitionHandle handle) const noexcept;
    [[nodiscard]] bool Valid(MaterialInstanceHandle handle) const noexcept;
    [[nodiscard]] MaterialDefinitionHandle EnsureDefinition(asset::AssetId id);
    [[nodiscard]] MaterialInstanceHandle EnsureInstance(asset::AssetId id);
    [[nodiscard]] Result<MaterialInstanceHandle> ApplyInstance(
        const MaterialInstanceProduct& parsed,
        ContentHash product_hash,
        u64 source_generation
    );
    asset::AssetManager& assets_;
    SlotMap<DefinitionSlot, MaterialDefinitionHandle> definitions_;
    SlotMap<InstanceSlot, MaterialInstanceHandle> instances_;
    std::unordered_map<asset::AssetId, MaterialDefinitionHandle> definition_assets_;
    std::unordered_map<asset::AssetId, MaterialInstanceHandle> instance_assets_;
    std::vector<Reload> reloads_;
    DefinitionReloadObserver reload_observer_;
};

} // namespace woki::gfx
