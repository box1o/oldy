#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "editor.hpp"
#include "editor/scene_panel.hpp"

namespace woki {

EditorLayer::EditorLayer(std::filesystem::path user_config, std::string dock_layout)
    : renderer_(false),
      workspace_(dock_layout),
      adapter_({*this, font_}),
      user_config_(std::move(user_config)) {
    panels_.Register(studio::kScenePanel, "Scene", [this] { return studio::ScenePanel(AssetStatus()); });
    panels_.Register(ui::Key::From("studio.panel.inspector"), "Inspector", [] {
        return ui::Column(ui::Text("Selection"), ui::Text("climbing.fbx")).Padding({12, 12, 12, 12}).Gap(8);
    });
    panels_.Register(ui::Key::From("studio.panel.assets"), "Assets", [] {
        return ui::Column(ui::Text("Assets"), ui::Text("anim/climbing.fbx")).Padding({12, 12, 12, 12}).Gap(8);
    });
    panels_.Register(ui::Key::From("studio.panel.diagnostics"), "Diagnostics", [this] {
        return ui::Column(ui::Text("Renderer"), ui::Text(AssetStatus())).Padding({12, 12, 12, 12}).Gap(8);
    });
}

void EditorLayer::OnAttach(Context& ctx) {
    renderer_.OnAttach(ctx);
}

void EditorLayer::OnDetach(Context& ctx) {
    ui_.CancelCapture();
    renderer_.OnDetach(ctx);
    if (user_config_.empty())
        return;
    std::ifstream input(user_config_, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    const std::string value = "  dock: '" + workspace_.Serialize() + "'";
    const size_t key = text.find("  dock:");
    if (key != std::string::npos) {
        const size_t end = text.find('\n', key);
        text.replace(key, end == std::string::npos ? text.size() - key : end - key, value);
    } else {
        text += "\nui:\n" + value + "\n";
    }
    const std::filesystem::path temporary = user_config_.string() + ".tmp";
    std::error_code error;
    std::filesystem::create_directories(user_config_.parent_path(), error);
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    output << text;
    output.close();
    if (output)
        std::filesystem::rename(temporary, user_config_, error);
    if (error)
        slog::Warn("Could not persist dock layout: {}", error.message());
}

ui::View EditorLayer::BuildUi() {
    return workspace_.Build(panels_, {0, 0, static_cast<f32>(canvas_.width), static_cast<f32>(canvas_.height)});
}

void EditorLayer::OnUpdate(Context& ctx, f64 delta_ms) {
    const f32 scale = std::max(ctx.window->GetContentScaleX(), 0.01f);
    const u32 pixel_width = ctx.window->GetWidth(), pixel_height = ctx.window->GetHeight();
    canvas_.width = static_cast<u32>(static_cast<f32>(pixel_width) / scale);
    canvas_.height = static_cast<u32>(static_cast<f32>(pixel_height) / scale);
    ui_.SetContent(BuildUi());
    ui_.Prepare(
        {.viewport = {static_cast<f32>(canvas_.width), static_cast<f32>(canvas_.height)},
            .scale = scale,
            .time = std::chrono::steady_clock::now()}
    );
    if (const auto bounds = ui_.Bounds(studio::kSceneContent))
        scene_bounds_ = *bounds;
    renderer_.SetSceneOrigin(scene_bounds_.x, scene_bounds_.y);
    const bool visible = workspace_.PanelVisible(studio::kScenePanel) && ui_.Visible(studio::kSceneContent);
    static_cast<void>(renderer_.SetSceneVisible(visible));
    if (visible) {
        const u32 width = std::max(
            1u,
            static_cast<u32>(std::ceil(scene_bounds_.width * ctx.window->GetContentScaleX()))
        );
        const u32 height = std::max(
            1u,
            static_cast<u32>(std::ceil(scene_bounds_.height * ctx.window->GetContentScaleY()))
        );
        if (width != scene_width_ || height != scene_height_) {
            scene_width_ = width;
            scene_height_ = height;
            static_cast<void>(renderer_.ResizeScene(width, height));
        }
    }
    canvas_ = adapter_.Convert(ui_.Display(), renderer_.Surface(), pixel_width, pixel_height, scale);
    renderer_.SetCanvas(canvas_);
    renderer_.OnUpdate(ctx, delta_ms);
}

void EditorLayer::OnEvent(Context& ctx, events::Event& event) {
    if (auto converted = events_.Convert(event); converted && ui_.HandleEvent(*converted)) {
        event.handled = true;
        return;
    }
    if (event.GetEventType() == events::EventType::kWindowLostFocus
        || event.GetEventType() == events::EventType::kPointerCancel
        || event.GetEventType() == events::EventType::kPointerLeft) {
        scene_pointer_capture_ = false;
        events_.Reset();
        ui_.CancelCapture();
        renderer_.OnEvent(ctx, event);
        return;
    }
    bool in_scene = false;
    if (auto converted = events_.Convert(event); converted)
        if (const auto* pointer = std::get_if<ui::PointerEvent>(&*converted))
            in_scene = scene_bounds_.Contains(pointer->position);
    if (event.GetEventType() == events::EventType::kPointerDown) {
        const auto& pointer = static_cast<const events::PointerDownEvent&>(event).pointer_data;
        scene_pointer_capture_ = in_scene && pointer.button == events::PointerButton::kMiddle;
    }
    if (event.GetEventType() == events::EventType::kPointerUp)
        scene_pointer_capture_ = false;
    const bool scene_focused = ui_.Focused(studio::kSceneContent) || scene_pointer_capture_;
    const bool scene_input = scene_pointer_capture_ || in_scene || (scene_focused && !ui_.HasTextOrModalFocus());
    if (workspace_.PanelVisible(studio::kScenePanel) && scene_input)
        renderer_.OnEvent(ctx, event);
}

std::optional<gfx::CanvasImageSource> EditorLayer::Resolve(ui::ImageId image) const {
    if (image == studio::kSceneImage && renderer_.SceneTarget().IsValid())
        return gfx::CanvasImageSource{renderer_.SceneTarget()};
    return std::nullopt;
}

std::string EditorLayer::AssetStatus() const {
    switch (renderer_.AssetStatus()) {
        case gfx::MeshState::Resident:
            return "climbing.fbx: animated / resident";
        case gfx::MeshState::Failed:
            return "climbing.fbx: load failed";
        case gfx::MeshState::Uploading:
            return "climbing.fbx: uploading";
        default:
            return "climbing.fbx: loading";
    }
}

} // namespace woki
