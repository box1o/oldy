#include <woki/gfx/library.hpp>

namespace woki::gfx {

ShaderAssetHandle ShaderLibrary::Create() {
    const u32 index = static_cast<u32>(slots_.size());
    slots_.push_back({});
    return ShaderAssetHandle::Create(index, slots_.back().generation);
}

Result<void> ShaderLibrary::Publish(const ShaderAssetHandle handle, const asset::Product& product) {
    if (!handle.IsValid() || handle.Index() >= slots_.size() || slots_[handle.Index()].generation != handle.Generation())
        return Err(ErrorCode::InvalidArgument, "shader handle is stale or invalid");
    Slot& slot = slots_[handle.Index()];
    TRY_VOID(asset::ValidateProduct(product));
    if (product.type != kShaderProductType || product.schema_version != kShaderPayloadVersion)
        return Err(ErrorCode::ParseInvalidFormat, "asset product is not a supported shader product");
    auto payload = ParseShaderPayload(product.payload);
    if (!payload)
        return Err(std::move(payload).error());
    if (payload->dependencies.size() != product.dependency_hashes.size() || !std::ranges::is_sorted(payload->dependencies) || std::ranges::adjacent_find(payload->dependencies) != payload->dependencies.end())
        return Err(ErrorCode::ParseInvalidFormat, "shader product dependency metadata is inconsistent or noncanonical");

    ref<rhi::ShaderModule> module;
    for (auto& entry : modules_) {
        if (entry.hash == payload->module_hash) {
            module = entry.module;
            break;
        }
    }
    if (!module) {
        rhi::ShaderModuleDesc desc{.code = payload->code, .label = "Shader: " + payload->module_hash.Hex()};
        auto created = create_module_(desc);
        if (!created)
            return Err(std::move(created).error());
        module = std::move(*created);
        modules_.push_back({payload->module_hash, module});
    }
    const u64 version = slot.record.version + 1;
    slot.current = createRef<const ShaderGeneration>(ShaderGeneration{std::move(module), std::move(*payload), version, product.product_hash});
    slot.record.state = ShaderState::Ready;
    slot.record.version = version;
    slot.record.product_hash = product.product_hash;
    slot.record.diagnostics.clear();
    return Ok();
}

Result<BorrowedShader> ShaderLibrary::Borrow(const ShaderAssetHandle handle) const {
    const auto record = Record(handle);
    if (!record || record->state != ShaderState::Ready)
        return Err(ErrorCode::ValidationInvalidState, "shader is not ready");
    const Slot& slot = slots_[handle.Index()];
    return Ok(BorrowedShader{slot.current});
}

std::optional<ShaderAssetRecord> ShaderLibrary::Record(const ShaderAssetHandle handle) const {
    if (!handle.IsValid() || handle.Index() >= slots_.size() || slots_[handle.Index()].generation != handle.Generation())
        return std::nullopt;
    return slots_[handle.Index()].record;
}

} // namespace woki::gfx
