#pragma once

#include <woki/platform.hpp>

#include "core/layer.hpp"
#include "renderer/rhi_renderer.hpp"

namespace woki {

class RenderLayer final : public Layer {
public:
    void OnAttach(Context& ctx) override;
    void OnDetach(Context& ctx) override;
    void OnUpdate(Context& ctx, f64 delta_ms) override;
    void OnEvent(Context& ctx, events::Event& event) override;

private:
    RhiRenderer renderer_;
};

} // namespace woki
