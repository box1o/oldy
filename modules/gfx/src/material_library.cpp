#include <cstring>

#include <woki/gfx/advanced/material_library.hpp>

namespace woki::gfx {
namespace {
MaterialValue DefaultValue(const MaterialPropertyPlan& property) {
    const auto& bytes = property.default_bytes;
    switch (property.type) {
        case MaterialValueType::Bool: {
            u32 value{};
            std::memcpy(&value, bytes.data(), 4);
            return value != 0;
        }
        case MaterialValueType::I32: {
            i32 value{};
            std::memcpy(&value, bytes.data(), 4);
            return value;
        }
        case MaterialValueType::U32: {
            u32 value{};
            std::memcpy(&value, bytes.data(), 4);
            return value;
        }
        case MaterialValueType::F32: {
            f32 value{};
            std::memcpy(&value, bytes.data(), 4);
            return value;
        }
        case MaterialValueType::Vec2: {
            std::array<f32, 2> value{};
            std::memcpy(value.data(), bytes.data(), bytes.size());
            return value;
        }
        case MaterialValueType::Vec3: {
            std::array<f32, 3> value{};
            std::memcpy(value.data(), bytes.data(), bytes.size());
            return value;
        }
        case MaterialValueType::Vec4: {
            std::array<f32, 4> value{};
            std::memcpy(value.data(), bytes.data(), bytes.size());
            return value;
        }
        case MaterialValueType::Mat4: {
            std::array<f32, 16> value{};
            std::memcpy(value.data(), bytes.data(), bytes.size());
            return value;
        }
        default:
            return 0.0F;
    }
}

Result<void> ValidateDependencies(const asset::Product& source, const MaterialDefinitionProduct& material) {
    std::map<asset::AssetId, ContentHash> expected{{material.shader, material.shader_product_hash}};
    for (const auto& texture : material.binding_plan.textures)
        expected.emplace(
            texture.default_asset,
            texture.default_asset == BuiltinFallbackTextureId(texture.semantic)
                ? BuiltinFallbackTextureProductHash(texture.semantic)
                : ContentHash{}
        );
    if (source.dependencies.size() != expected.size())
        return Err(ErrorCode::ParseInvalidFormat, "material definition dependencies are incomplete or contain extras");
    for (const auto& dependency : source.dependencies) {
        const auto found = expected.find(dependency.asset_id);
        if (found == expected.end() || (found->second != ContentHash{} && found->second != dependency.product_hash))
            return Err(ErrorCode::ParseInvalidFormat, "material definition dependency identity is inconsistent");
    }
    return Ok();
}

Result<void> ValidateDependencies(const asset::Product& source, const MaterialInstanceProduct& material) {
    std::set<asset::AssetId> expected{material.definition};
    for (const auto& [id, value] : material.overrides) {
        const auto type = material.override_types.find(id);
        if (type != material.override_types.end()
            && (type->second == MaterialValueType::Texture2D || type->second == MaterialValueType::TextureCube))
            expected.insert(std::get<asset::AssetId>(value));
    }
    if (source.dependencies.size() != expected.size())
        return Err(ErrorCode::ParseInvalidFormat, "material instance dependencies are incomplete or contain extras");
    for (const auto& dependency : source.dependencies)
        if (!expected.contains(dependency.asset_id)
            || (dependency.asset_id == material.definition
                && dependency.product_hash != material.definition_product_hash))
            return Err(ErrorCode::ParseInvalidFormat, "material instance dependency identity is inconsistent");
    return Ok();
}
} // namespace

MaterialLibrary::MaterialLibrary(asset::AssetManager& assets)
    : assets_(assets) {}

bool MaterialLibrary::Valid(const MaterialDefinitionHandle handle) const noexcept {
    return definitions_.Contains(handle);
}

bool MaterialLibrary::Valid(const MaterialInstanceHandle handle) const noexcept {
    return instances_.Contains(handle);
}

MaterialDefinitionHandle MaterialLibrary::EnsureDefinition(const asset::AssetId id) {
    if (const auto found = definition_assets_.find(id); found != definition_assets_.end())
        return found->second;
    const auto handle = definitions_.Emplace();
    definition_assets_.emplace(id, handle);
    return handle;
}

MaterialInstanceHandle MaterialLibrary::EnsureInstance(const asset::AssetId id) {
    if (const auto found = instance_assets_.find(id); found != instance_assets_.end())
        return found->second;
    const auto handle = instances_.Emplace();
    instances_.Get(handle).record.asset_id = id;
    instance_assets_.emplace(id, handle);
    return handle;
}

Result<MaterialDefinitionHandle> MaterialLibrary::RequestDefinition(const asset::AssetId id) {
    if (!id)
        return Err(ErrorCode::InvalidArgument, "material definition asset ID is invalid");
    const auto handle = EnsureDefinition(id);
    auto request = assets_.Request(id);
    if (!request && !assets_.Borrow(id)) {
        definitions_.Get(handle).state = MaterialState::Failed;
        definitions_.Get(handle).diagnostic = std::string(request.error().Message());
        return Err(std::move(request).error());
    }
    auto& slot = definitions_.Get(handle);
    slot.state = slot.current ? MaterialState::Reloading : MaterialState::Loading;
    return Ok(handle);
}

Result<MaterialInstanceHandle> MaterialLibrary::RequestInstance(const asset::AssetId id) {
    if (!id)
        return Err(ErrorCode::InvalidArgument, "material instance asset ID is invalid");
    const auto handle = EnsureInstance(id);
    if (const auto lease = assets_.Borrow(id); lease && lease->Get().type == kMaterialInstanceProductType) {
        auto parsed = ParseMaterialInstanceProduct(lease->Bytes());
        if (!parsed)
            return Err(std::move(parsed).error());
        TRY_VOID(RequestDefinition(parsed->definition));
    }
    auto request = assets_.Request(id);
    if (!request && !assets_.Borrow(id)) {
        instances_.Get(handle).record.diagnostic = std::string(request.error().Message());
        return Err(std::move(request).error());
    }
    return Ok(handle);
}

Result<MaterialDefinitionHandle> MaterialLibrary::PublishDefinition(const asset::Product& product) {
    TRY_VOID(asset::ValidateProduct(product));
    if (product.type != kMaterialDefinitionProductType || product.schema_version != kMaterialProductVersion)
        return Err(ErrorCode::ParseInvalidFormat, "asset is not a material definition product");
    MaterialDefinitionProduct parsed;
    TRY_ASSIGN(parsed, ParseMaterialDefinition(product.payload));
    if (parsed.id != product.asset_id)
        return Err(ErrorCode::ParseInvalidFormat, "material definition identity mismatch");
    TRY_VOID(ValidateDependencies(product, parsed));
    const auto handle = EnsureDefinition(product.asset_id);
    auto& slot = definitions_.Get(handle);
    const u64 version = slot.current ? slot.current->version + 1 : 1;
    slot.current = createRef<const MaterialDefinitionGeneration>(
        MaterialDefinitionGeneration{std::move(parsed), version, product.product_hash}
    );
    slot.state = MaterialState::Ready;
    slot.diagnostic.clear();
    return Ok(handle);
}

Result<MaterialInstanceHandle> MaterialLibrary::PublishInstance(const asset::Product& product) {
    TRY_VOID(asset::ValidateProduct(product));
    if (product.type != kMaterialInstanceProductType || product.schema_version != kMaterialProductVersion)
        return Err(ErrorCode::ParseInvalidFormat, "asset is not a material instance product");
    MaterialInstanceProduct parsed;
    TRY_ASSIGN(parsed, ParseMaterialInstanceProduct(product.payload));
    if (parsed.id != product.asset_id)
        return Err(ErrorCode::ParseInvalidFormat, "material instance identity mismatch");
    TRY_VOID(ValidateDependencies(product, parsed));
    return ApplyInstance(parsed, product.product_hash, 0);
}

Result<MaterialInstanceHandle> MaterialLibrary::ApplyInstance(
    const MaterialInstanceProduct& parsed,
    const ContentHash product_hash,
    const u64 source_generation
) {
    const auto definition = definition_assets_.find(parsed.definition);
    if (definition == definition_assets_.end())
        return Err(ErrorCode::ValidationInvalidState, "material instance definition is not loaded");
    auto borrowed = Borrow(definition->second);
    if (!borrowed || borrowed->generation->product_hash != parsed.definition_product_hash)
        return Err(ErrorCode::ValidationInvalidState, "material instance requires a different definition product hash");
    std::map<MaterialPropertyId, MaterialValue> values;
    for (const auto& property : borrowed->Get().binding_plan.properties)
        values.emplace(property.id, DefaultValue(property));
    for (const auto& texture : borrowed->Get().binding_plan.textures)
        values.emplace(texture.id, texture.default_asset);
    std::map<MaterialPropertyId, MaterialValue> authored;
    for (const auto& [id, value] : parsed.overrides) {
        const auto property = std::ranges::find(borrowed->Get().binding_plan.properties, id, &MaterialPropertyPlan::id);
        const auto texture = std::ranges::find(borrowed->Get().binding_plan.textures, id, &MaterialTextureBinding::id);
        const auto expected = property != borrowed->Get().binding_plan.properties.end() ? property->type
                              : texture != borrowed->Get().binding_plan.textures.end()  ? texture->type
                                                                                        : MaterialValueType::Sampler;
        if ((property == borrowed->Get().binding_plan.properties.end()
                && texture == borrowed->Get().binding_plan.textures.end())
            || !MaterialValueMatches(expected, value))
            return Err(ErrorCode::ParseInvalidFormat, "material instance override is unknown or has the wrong type");
        authored[id] = value;
        values[id] = value;
    }
    const auto handle = EnsureInstance(parsed.id);
    auto& record = instances_.Get(handle).record;
    std::map<MaterialPropertyId, MaterialValue> runtime;
    for (const auto& [id, value] : record.runtime_overrides) {
        const auto property = std::ranges::find(borrowed->Get().binding_plan.properties, id, &MaterialPropertyPlan::id);
        const auto texture = std::ranges::find(borrowed->Get().binding_plan.textures, id, &MaterialTextureBinding::id);
        const auto expected = property != borrowed->Get().binding_plan.properties.end() ? property->type
                              : texture != borrowed->Get().binding_plan.textures.end()  ? texture->type
                                                                                        : MaterialValueType::Sampler;
        if ((property != borrowed->Get().binding_plan.properties.end()
                || texture != borrowed->Get().binding_plan.textures.end())
            && MaterialValueMatches(expected, value))
            runtime.emplace(id, value);
    }
    for (const auto& [id, value] : runtime)
        values[id] = value;
    record.definition = definition->second;
    record.authored_values = std::move(authored);
    record.runtime_overrides = std::move(runtime);
    record.values = std::move(values);
    record.source_product_hash = product_hash;
    record.source_generation = source_generation;
    ++record.content_version;
    record.dirty = true;
    record.diagnostic.clear();
    return Ok(handle);
}

Result<void> MaterialLibrary::QueueDefinitionReload(
    const MaterialDefinitionHandle handle,
    const asset::Product& candidate
) {
    if (!Valid(handle))
        return Err(ErrorCode::InvalidArgument, "material definition handle is stale");
    TRY_VOID(asset::ValidateProduct(candidate));
    if (candidate.type != kMaterialDefinitionProductType)
        return Err(ErrorCode::ParseInvalidFormat, "reload candidate is not a material definition");
    MaterialDefinitionProduct parsed;
    TRY_ASSIGN(parsed, ParseMaterialDefinition(candidate.payload));
    if (!definitions_.Get(handle).current || definitions_.Get(handle).current->product.id != parsed.id)
        return Err(ErrorCode::ValidationInvalidState, "material reload changes stable identity");
    reloads_.push_back({handle, std::move(parsed), candidate.product_hash});
    return Ok();
}

Result<void> MaterialLibrary::PublishReloads() {
    for (auto& reload : reloads_) {
        if (!Valid(reload.handle))
            continue;
        auto& slot = definitions_.Get(reload.handle);
        const auto previous = slot.current;
        const u64 version = previous->version + 1;
        auto generation = createRef<const MaterialDefinitionGeneration>(
            MaterialDefinitionGeneration{reload.product, version, reload.hash}
        );

        struct Migration final {
            MaterialInstanceRecord* record{};
            std::map<MaterialPropertyId, MaterialValue> authored;
            std::map<MaterialPropertyId, MaterialValue> runtime;
            std::map<MaterialPropertyId, MaterialValue> values;
        };

        std::vector<Migration> migrations;
        for (const auto instance_handle : instances_.Handles()) {
            auto& instance = instances_.Get(instance_handle);
            if (instance.record.definition == reload.handle) {
                Migration migration;
                migration.record = &instance.record;
                for (const auto& property : generation->product.binding_plan.properties) {
                    migration.values.emplace(property.id, DefaultValue(property));
                    if (const auto old = instance.record.authored_values.find(property.id);
                        old != instance.record.authored_values.end()
                        && MaterialValueMatches(property.type, old->second))
                        migration.authored.emplace(property.id, old->second);
                    if (const auto old = instance.record.runtime_overrides.find(property.id);
                        old != instance.record.runtime_overrides.end()
                        && MaterialValueMatches(property.type, old->second))
                        migration.runtime.emplace(property.id, old->second);
                }
                for (const auto& texture : generation->product.binding_plan.textures) {
                    migration.values.emplace(texture.id, texture.default_asset);
                    if (const auto old = instance.record.authored_values.find(texture.id);
                        old != instance.record.authored_values.end() && MaterialValueMatches(texture.type, old->second))
                        migration.authored.emplace(texture.id, old->second);
                    if (const auto old = instance.record.runtime_overrides.find(texture.id);
                        old != instance.record.runtime_overrides.end()
                        && MaterialValueMatches(texture.type, old->second))
                        migration.runtime.emplace(texture.id, old->second);
                }
                for (const auto& [id, value] : migration.authored)
                    migration.values[id] = value;
                for (const auto& [id, value] : migration.runtime)
                    migration.values[id] = value;
                migrations.push_back(std::move(migration));
            }
        }
        for (auto& migration : migrations) {
            migration.record->authored_values = std::move(migration.authored);
            migration.record->runtime_overrides = std::move(migration.runtime);
            migration.record->values = std::move(migration.values);
            ++migration.record->content_version;
            migration.record->dirty = true;
        }
        slot.current = std::move(generation);
        slot.state = MaterialState::Ready;
        slot.diagnostic.clear();
        if (reload_observer_)
            reload_observer_(slot.current->product.id, previous->product.shader_product_hash);
    }
    reloads_.clear();
    return Ok();
}

Result<void> MaterialLibrary::Pump() {
    static_cast<void>(assets_.PumpPublications());
    for (const auto& [id, handle] : definition_assets_)
        if (const auto lease = assets_.Borrow(id);
            lease && lease->Get().type == kMaterialDefinitionProductType
            && (!definitions_.Get(handle).current
                || definitions_.Get(handle).current->product_hash != lease->Get().version.product_hash)) {
            MaterialDefinitionProduct parsed;
            auto result = ParseMaterialDefinition(lease->Bytes());
            if (!result) {
                definitions_.Get(handle)
                    .state = definitions_.Get(handle).current ? MaterialState::Ready : MaterialState::Failed;
                definitions_.Get(handle).diagnostic = std::string(result.error().Message());
            } else if (definitions_.Get(handle).current)
                reloads_.push_back({handle, std::move(*result), lease->Get().version.product_hash});
            else {
                definitions_.Get(handle).current = createRef<const MaterialDefinitionGeneration>(
                    MaterialDefinitionGeneration{std::move(*result), 1, lease->Get().version.product_hash}
                );
                definitions_.Get(handle).state = MaterialState::Ready;
            }
        }
    TRY_VOID(PublishReloads());
    for (const auto& [id, handle] : instance_assets_)
        if (const auto lease = assets_.Borrow(id);
            lease && lease->Get().type == kMaterialInstanceProductType
            && lease->Get().version.generation != instances_.Get(handle).record.source_generation) {
            auto parsed = ParseMaterialInstanceProduct(lease->Bytes());
            if (!parsed) {
                instances_.Get(handle).record.diagnostic = std::string(parsed.error().Message());
                continue;
            }
            if (!definition_assets_.contains(parsed->definition)) {
                static_cast<void>(RequestDefinition(parsed->definition));
                continue;
            }
            auto applied = ApplyInstance(*parsed, lease->Get().version.product_hash, lease->Get().version.generation);
            if (!applied)
                instances_.Get(handle).record.diagnostic = std::string(applied.error().Message());
        }
    return Ok();
}

Result<void> MaterialLibrary::Set(
    const MaterialInstanceHandle instance,
    const MaterialPropertyId property,
    MaterialValue value
) {
    auto* record = TryGetMutable(instance);
    if (!record)
        return Err(ErrorCode::InvalidArgument, "material instance handle is stale");
    auto definition = Borrow(record->definition);
    if (!definition)
        return Err(std::move(definition).error());
    const auto numeric = std::ranges::find(
        definition->Get().binding_plan.properties,
        property,
        &MaterialPropertyPlan::id
    );
    const auto texture = std::ranges::find(
        definition->Get().binding_plan.textures,
        property,
        &MaterialTextureBinding::id
    );
    if (numeric == definition->Get().binding_plan.properties.end()
        && texture == definition->Get().binding_plan.textures.end())
        return Err(ErrorCode::InvalidArgument, "material property is unknown");
    const auto type = numeric != definition->Get().binding_plan.properties.end() ? numeric->type : texture->type;
    if (!MaterialValueMatches(type, value))
        return Err(ErrorCode::InvalidArgument, "material value type mismatch");
    record->values[property] = std::move(value);
    record->runtime_overrides[property] = record->values[property];
    ++record->content_version;
    record->dirty = true;
    return Ok();
}

Result<BorrowedMaterialDefinition> MaterialLibrary::Borrow(const MaterialDefinitionHandle handle) const {
    if (!Valid(handle) || !definitions_.Get(handle).current)
        return Err(ErrorCode::ValidationInvalidState, "material definition is not ready");
    return Ok(BorrowedMaterialDefinition{definitions_.Get(handle).current});
}

const MaterialInstanceRecord* MaterialLibrary::TryGet(const MaterialInstanceHandle handle) const noexcept {
    const auto* slot = instances_.TryGet(handle);
    return slot == nullptr ? nullptr : &slot->record;
}

MaterialInstanceRecord* MaterialLibrary::TryGetMutable(const MaterialInstanceHandle handle) noexcept {
    auto* slot = instances_.TryGet(handle);
    return slot == nullptr ? nullptr : &slot->record;
}

std::vector<MaterialInstanceHandle> MaterialLibrary::DirtyInstances() const {
    std::vector<MaterialInstanceHandle> result;
    for (const auto handle : instances_.Handles())
        if (instances_.Get(handle).record.dirty)
            result.push_back(handle);
    return result;
}

void MaterialLibrary::MarkPrepared(const MaterialInstanceHandle handle, const u64 version) {
    if (auto* record = TryGetMutable(handle); record && record->content_version == version)
        record->dirty = false;
}

std::optional<MaterialDefinitionHandle> MaterialLibrary::FindDefinition(const asset::AssetId id) const noexcept {
    const auto found = definition_assets_.find(id);
    return found == definition_assets_.end() ? std::nullopt : std::optional(found->second);
}

std::optional<MaterialState> MaterialLibrary::State(const MaterialDefinitionHandle handle) const noexcept {
    if (!Valid(handle))
        return std::nullopt;
    return definitions_.Get(handle).state;
}

std::string_view MaterialLibrary::Diagnostic(const MaterialDefinitionHandle handle) const noexcept {
    if (!Valid(handle))
        return {};
    return definitions_.Get(handle).diagnostic;
}

} // namespace woki::gfx
