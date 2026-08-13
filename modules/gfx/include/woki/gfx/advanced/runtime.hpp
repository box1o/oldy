#pragma once

#include <functional>

#include <woki/asset.hpp>
#include <woki/task.hpp>

#include "material_prepare.hpp"
#include "mesh_product.hpp"
#include "pipeline_cache.hpp"
#include "runtime_services.hpp"
#include "standard_features.hpp"
#include "texture_library.hpp"

namespace woki::rhi {
class Adapter;
class Device;
class Instance;
} // namespace woki::rhi

namespace woki::gfx {

class RenderRuntime;

enum class PreparationFailurePolicy : u8 { Required, Optional };

struct RenderDeviceBundle final {
    scope<rhi::Instance> instance;
    scope<rhi::Adapter> adapter;
    ref<rhi::Device> device;
};

class RenderDeviceFactory {
public:
    virtual ~RenderDeviceFactory() = default;
    [[nodiscard]] virtual Result<RenderDeviceBundle> CreateDevice() = 0;

    [[nodiscard]] virtual bool DeviceLost() const noexcept {
        return false;
    }
};

// Explicit integration seam for custom asset systems and renderer development.
// Applications using packaged products do not need to include this header.
struct RenderRuntimeAdvancedDescriptor final {
    asset::AssetServices assets;
    task::Scheduler* scheduler{};
    task::IoExecutor* io{};
    task::CompletionQueue* completions{};
    RenderRuntimeDesc resources;
    TextureBudget texture_budget;
    CapabilitySet feature_capabilities;
    PreparationFailurePolicy upload_failure_policy{PreparationFailurePolicy::Required};
    PreparationFailurePolicy material_failure_policy{PreparationFailurePolicy::Required};
    MaterialPreparation::ResolveShader resolve_material_shader;
    ref<const StandardFeaturePrograms> standard_feature_programs;
    asset::Product diagnostic_shader_product;
    std::function<Result<ResolvedEnvironment>(EnvironmentHandle)> resolve_environment;
    TextureHandle grading_lut;
    std::function<void()> stop_requests;
    std::function<void()> join_workers;
    std::function<void()> drain_publications;
    std::function<Result<void>()> flush_asset_manifest;
    std::function<Result<void>(rhi::Device&, rhi::SubmissionTicket)> wait_for_shutdown_submission;
};

} // namespace woki::gfx
