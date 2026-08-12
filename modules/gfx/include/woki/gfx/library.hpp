#pragma once

#include <functional>
#include <optional>

#include <woki/rhi.hpp>

#include "product.hpp"

namespace woki::gfx {

struct ShaderGeneration {
    ref<rhi::ShaderModule> module;
    ShaderPayload payload;
    u64 version{0};
    ContentHash product_hash;
};

struct BorrowedShader {
    ref<const ShaderGeneration> generation;

    [[nodiscard]] rhi::ShaderModule& Module() const noexcept {
        return *generation->module;
    }

    [[nodiscard]] const ShaderInterface& Interface() const noexcept {
        return generation->payload.interface;
    }

    [[nodiscard]] std::span<const asset::AssetPath> Dependencies() const noexcept {
        return generation->payload.dependencies;
    }

    [[nodiscard]] u64 Version() const noexcept {
        return generation->version;
    }
};

class ShaderLibrary {
public:
    using ModuleFactory = std::function<Result<ref<rhi::ShaderModule>>(const rhi::ShaderModuleDesc&)>;

    explicit ShaderLibrary(rhi::Device& device)
        : create_module_([&device](const rhi::ShaderModuleDesc& desc) -> Result<ref<rhi::ShaderModule>> {
              auto module = device.CreateShaderModule(desc);
              if (!module)
                  return Err(std::move(module).error());
              return Ok(ref<rhi::ShaderModule>(std::move(*module)));
          }) {}

    explicit ShaderLibrary(ModuleFactory create_module)
        : create_module_(std::move(create_module)) {}

    [[nodiscard]] ShaderAssetHandle Create();
    [[nodiscard]] Result<void> Publish(ShaderAssetHandle handle, const asset::Product& product);
    [[nodiscard]] Result<BorrowedShader> Borrow(ShaderAssetHandle handle) const;
    [[nodiscard]] std::optional<ShaderAssetRecord> Record(ShaderAssetHandle handle) const;

private:
    struct ModuleEntry {
        ContentHash hash;
        ref<rhi::ShaderModule> module;
    };

    struct Slot {
        u32 generation{1};
        ShaderAssetRecord record;
        ref<const ShaderGeneration> current;
    };

    ModuleFactory create_module_;
    std::vector<Slot> slots_;
    std::vector<ModuleEntry> modules_;
    // The module cache retains superseded modules because RHI has no proven submission epoch.
};

} // namespace woki::gfx
