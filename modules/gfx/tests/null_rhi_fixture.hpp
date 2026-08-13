#pragma once

#include <algorithm>
#include <stdexcept>

#include <woki/rhi.hpp>

namespace woki::gfx::test {

struct NullDevice final {
    NullDevice() {
        auto created_instance = rhi::CreateNullInstance();
        if (!created_instance)
            throw std::runtime_error(std::string(created_instance.error().Message()));
        instance = std::move(*created_instance);
        auto created_adapter = instance->RequestAdapter();
        if (!created_adapter)
            throw std::runtime_error(std::string(created_adapter.error().Message()));
        adapter = std::move(*created_adapter);
        auto created_device = adapter->CreateDevice();
        if (!created_device)
            throw std::runtime_error(std::string(created_device.error().Message()));
        null_device = std::move(*created_device);
        device = rhi::CreateValidationDevice(null_device, {.full = true, .diagnostic = [this](const rhi::RhiDiagnostic& value) { diagnostics.push_back(value); }});
    }

    scope<rhi::Instance> instance;
    scope<rhi::Adapter> adapter;
    ref<rhi::Device> null_device;
    ref<rhi::Device> device;
    std::vector<rhi::RhiDiagnostic> diagnostics;
};

inline bool LogContains(const rhi::Device& device, const std::string_view text) {
    return std::ranges::any_of(rhi::NullCommandLog(device), [&](const std::string& event) { return event.find(text) != std::string::npos; });
}

} // namespace woki::gfx::test
