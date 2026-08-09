#pragma once

#include <span>
#include <string_view>

#include <woki/core.hpp>

#include "types.hpp"
#include "descriptors.hpp"

namespace woki::rhi {

class Adapter;

class Surface {
public:
    virtual ~Surface() = default;

    [[nodiscard]] virtual Result<void> GetCapabilities(const Adapter& adapter, SurfaceCapabilities& capabilities) const = 0;
    virtual void GetCurrentTexture(SurfaceTexture& surface_texture) = 0;
    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;
    [[nodiscard]] virtual Result<void> Configure(const SurfaceConfiguration& config) = 0;
    [[nodiscard]] virtual Result<void> Unconfigure() = 0;
    [[nodiscard]] virtual Result<void> Present() = 0;
    virtual void SetLabel(std::string_view label) = 0;

protected:
    Surface() = default;
};

} // namespace woki::rhi
