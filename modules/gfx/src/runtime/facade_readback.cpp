#include "../internal/runtime_state.hpp"

namespace woki::gfx {

ReadbackService& RenderRuntime::Readbacks() noexcept {
    return *impl_->readback_service;
}

const Capabilities& RenderRuntime::GetCapabilities() const noexcept {
    return impl_->public_capabilities;
}

const Diagnostics& RenderRuntime::GetDiagnostics() const noexcept {
    return impl_->diagnostics;
}

RenderRecoveryState RenderRuntime::RecoveryState() const noexcept {
    return impl_->recovery_state;
}

} // namespace woki::gfx
