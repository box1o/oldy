#pragma once

#include <woki/rhi/instance.hpp>

namespace woki::rhi::backend {

[[nodiscard]] Result<scope<Instance>> CreateInstance(InstanceDesc desc);
void GetInstanceFeatures(SupportedInstanceFeatures& features);
[[nodiscard]] Result<InstanceLimits> GetInstanceLimits();
[[nodiscard]] bool HasInstanceFeature(InstanceFeatureName feature) noexcept;
[[nodiscard]] Proc GetProcAddress(std::string_view proc_name) noexcept;

} // namespace woki::rhi::backend
