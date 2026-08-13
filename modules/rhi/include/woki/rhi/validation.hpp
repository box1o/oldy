#pragma once

#include <functional>
#include <string>
#include <vector>

#include "device.hpp"

namespace woki::rhi {

enum class RhiDiagnosticCode : u16 {
    NullObject = 1,
    DescriptorLimit = 2,
    TextureDescriptor = 3,
    UsageMismatch = 4,
    BufferBounds = 5,
    BindingRange = 6,
    LayoutMismatch = 7,
    DynamicOffset = 8,
    CopyAlignment = 11,
    CopyBounds = 12,
    FormatMismatch = 13,
    ShaderInvalid = 14,
    PipelineTarget = 15,
    QueryIllegal = 17,
    EncoderScope = 20,
    PassState = 21,
    ThreadOwnership = 25,
    DeviceOwnership = 30,
    AlreadySubmitted = 31,
    UseAfterRetire = 32,
};

struct RhiDiagnostic final {
    RhiDiagnosticCode code{RhiDiagnosticCode::NullObject};
    std::string message;
    std::vector<std::string> breadcrumbs;
};

struct ValidationRhiDescriptor final {
    bool check_thread_ownership{true};
    bool full{false};
    std::function<void(const RhiDiagnostic&)> diagnostic;
};

[[nodiscard]] ref<Device> CreateValidationDevice(ref<Device> device, ValidationRhiDescriptor descriptor = {});

} // namespace woki::rhi
