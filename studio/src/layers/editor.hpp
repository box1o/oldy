#pragma once

#include <filesystem>
#include <woki/ui.hpp>
#include <woki/ui/render.hpp>

#include "core/layer.hpp"
#include "editor/dock_workspace.hpp"
#include "editor/platform_ui_adapter.hpp"
#include "render.hpp"

namespace woki {

class EditorLayer final : public Layer, private ui::render::ImageResolver {
public:
    EditorLayer(std::filesystem::path user_config = {}, std::string dock_layout = {});
    void OnAttach(Context& ctx) override;
    void OnDetach(Context& ctx) override;
    void OnUpdate(Context& ctx, f64 delta_ms) override;
    void OnEvent(Context& ctx, events::Event& event) override;

private:
    [[nodiscard]] std::optional<gfx::CanvasImageSource> Resolve(ui::ImageId image) const override;
    [[nodiscard]] ui::View BuildUi();
    [[nodiscard]] std::string AssetStatus() const;

    RenderLayer renderer_;
    ui::Runtime ui_;
    studio::PanelRegistry panels_;
    studio::DockWorkspace workspace_;
    studio::PlatformUiAdapter events_;
    ui::render::BuiltinFontProvider font_;
    ui::render::Adapter adapter_;
    gfx::CanvasFrame canvas_;
    ui::Rect scene_bounds_{};
    u32 scene_width_{}, scene_height_{};
    bool scene_pointer_capture_{};
    std::filesystem::path user_config_;
};

} // namespace woki
