#pragma once

#include "adapter.hpp"
#include "descriptors.hpp"
#include "device.hpp"
#include "surface.hpp"
#include "types.hpp"

#include <span>
#include <string_view>

#include <woki/core.hpp>

namespace woki {
class Window;
}

namespace woki::rhi {

class Instance {
public:
    virtual ~Instance() = default;

    [[nodiscard]] static Result<scope<Instance>> Create(InstanceDesc desc = {});

    [[nodiscard]] static SupportedInstanceFeatures GetInstanceFeatures();
    static void GetInstanceFeatures(SupportedInstanceFeatures& features);
    [[nodiscard]] static Result<InstanceLimits> GetInstanceLimits();
    [[nodiscard]] static bool HasInstanceFeature(InstanceFeatureName feature) noexcept;
    [[nodiscard]] static Proc GetProcAddress(std::string_view proc_name) noexcept;

    [[nodiscard]] virtual bool IsValid() const noexcept = 0;
    [[nodiscard]] virtual const InstanceDesc& GetDesc() const noexcept = 0;

    [[nodiscard]] virtual Result<scope<Surface>> CreateSurface(const SurfaceDescriptor& desc) = 0;
    [[nodiscard]] virtual Result<scope<Surface>> CreateSurface(
        Window& window, SurfaceDesc desc = {}) = 0;
    virtual void GetWGSLLanguageFeatures(SupportedWGSLLanguageFeatures& features) const = 0;
    [[nodiscard]] SupportedWGSLLanguageFeatures GetWGSLLanguageFeatures() const;
    [[nodiscard]] virtual bool HasWGSLLanguageFeature(
        WGSLLanguageFeatureName feature) const noexcept = 0;

    virtual void ProcessEvents() const noexcept = 0;

    [[nodiscard]] virtual WaitStatus WaitAny(Future& future, u64 timeout_ns) = 0;
    [[nodiscard]] virtual WaitStatus WaitAny(FutureWaitInfo& wait_info, u64 timeout_ns) = 0;
    [[nodiscard]] virtual WaitStatus WaitAny(
        std::span<FutureWaitInfo> wait_infos, u64 timeout_ns) = 0;

    [[nodiscard]] virtual Result<scope<Adapter>> RequestAdapter(RequestAdapterDesc desc = {}) = 0;
    [[nodiscard]] virtual Future RequestAdapter(
        RequestAdapterDesc desc, CallbackMode callback_mode, RequestAdapterCallback callback) = 0;

    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    Instance() = default;
};

inline SupportedWGSLLanguageFeatures Instance::GetWGSLLanguageFeatures() const {
    SupportedWGSLLanguageFeatures features{};
    GetWGSLLanguageFeatures(features);
    return features;
}

} // namespace woki::rhi
