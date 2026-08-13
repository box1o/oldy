#include <woki/gfx/advanced/library.hpp>

namespace woki::gfx {

ShaderAssetHandle ShaderLibrary::Create() {
    const u32 index = static_cast<u32>(slots_.size());
    slots_.push_back({});
    return ShaderAssetHandle::Create(index, slots_.back().generation);
}

Result<void> ShaderLibrary::Publish(const ShaderAssetHandle handle, const asset::Product& product) {
    if (!handle.IsValid() || handle.Index() >= slots_.size()
        || slots_[handle.Index()].generation != handle.Generation())
        return Err(ErrorCode::InvalidArgument, "shader handle is stale or invalid");
    Slot& slot = slots_[handle.Index()];
    TRY_VOID(asset::ValidateProduct(product));
    if (product.type != kShaderProductType || product.schema_version != kShaderPayloadVersion)
        return Err(ErrorCode::ParseInvalidFormat, "asset product is not a supported shader product");
    auto payload = ParseShaderPayload(product.payload);
    if (!payload)
        return Err(std::move(payload).error());
    // Shader payload dependencies are source paths used by incremental rebuild
    // and hot reload. Product dependencies are exact runtime product edges.
    // Includes such as WGSL files do not have cooked products, so the two sets
    // are deliberately independent.
    if (!std::ranges::is_sorted(payload->dependencies)
        || std::ranges::adjacent_find(payload->dependencies) != payload->dependencies.end())
        return Err(ErrorCode::ParseInvalidFormat, "shader source dependencies are noncanonical");

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
        modules_.push_back({payload->module_hash, module, {}});
    }
    const u64 version = slot.record.version + 1;
    slot.current = createRef<const ShaderGeneration>(
        ShaderGeneration{std::move(module), std::move(*payload), version, product.product_hash}
    );
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

Result<BorrowedShader> ShaderLibrary::BorrowProduct(const ContentHash product, const u64 generation) const {
    for (const auto& slot : slots_)
        if (slot.current != nullptr && slot.current->product_hash == product && slot.current->version == generation)
            return Ok(BorrowedShader{slot.current});
    return Err(ErrorCode::ValidationInvalidState, "shader product generation is not published");
}

std::optional<ShaderAssetRecord> ShaderLibrary::Record(const ShaderAssetHandle handle) const {
    if (!handle.IsValid() || handle.Index() >= slots_.size()
        || slots_[handle.Index()].generation != handle.Generation())
        return std::nullopt;
    return slots_[handle.Index()].record;
}

void ShaderLibrary::MarkUsed(const BorrowedShader& shader, const rhi::SubmissionTicket submission) {
    if (!submission.IsValid() || !shader.generation)
        return;
    const auto found = std::ranges::find(modules_, shader.generation->payload.module_hash, &ModuleEntry::hash);
    if (found != modules_.end() && found->last_used < submission)
        found->last_used = submission;
}

size_t ShaderLibrary::PruneModules(DeferredReleaseQueue& releases) {
    const size_t before = modules_.size();
    std::erase_if(modules_, [&](ModuleEntry& entry) {
        if (!entry.last_used.IsValid() || entry.module.use_count() != 1)
            return false;
        releases.Retire(std::move(entry.module), entry.last_used);
        return true;
    });
    return before - modules_.size();
}

} // namespace woki::gfx
