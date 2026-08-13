#pragma once

#include <string>
#include <vector>

#include "instance.hpp"

namespace woki::rhi {

struct NullRhiDescriptor final {
    DeviceCapabilities capabilities{};
    bool complete_submissions_immediately{true};
    std::string adapter_name{"Woki NullRHI"};
};

[[nodiscard]] Result<scope<Instance>> CreateNullInstance(NullRhiDescriptor descriptor = {});
[[nodiscard]] std::span<const std::string> NullCommandLog(const Device& device) noexcept;
[[nodiscard]] Result<void> LoseNullDevice(Device& device, DeviceLostReason reason, std::string_view message);

} // namespace woki::rhi
