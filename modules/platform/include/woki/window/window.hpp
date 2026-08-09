#pragma once

#include <map>
#include <array>
#include <string>
#include <functional>

#include <woki/core.hpp>
#include <woki/events/events.hpp>

namespace woki {

enum class CursorMode : u8 { kNormal, kHidden, kDisabled };
enum class CursorType : u8 { kArrow, kIBeam, kCrosshair, kHand, kHResize, kVResize };

using CallbackId = u32;

struct WindowOptions {
    std::string title{"woki"};
    u32 width{1280};
    u32 height{720};
    bool floating{false};
    bool fullscreen{false};
    bool resizable{true};
    bool decorated{true};
};

class Window final : public ref_from_this<Window> {
    struct ConstructionKey {};

public:
    using EventCallback = std::function<void(events::Event&)>;
    using ResizeCallback = std::function<void(u32 width, u32 height)>;

    explicit Window(ConstructionKey);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    [[nodiscard]] static Result<ref<Window>> Create(const WindowOptions& options = {});

    [[nodiscard]] const std::string& GetTitle() const noexcept;
    [[nodiscard]] u32 GetWidth() const noexcept;
    [[nodiscard]] u32 GetHeight() const noexcept;
    [[nodiscard]] f32 GetAspectRatio() const noexcept;
    [[nodiscard]] bool IsFullscreen() const noexcept;

    [[nodiscard]] f32 GetContentScaleX() const noexcept;
    [[nodiscard]] f32 GetContentScaleY() const noexcept;

    [[nodiscard]] CursorMode GetCursorMode() const noexcept;
    void SetCursorMode(CursorMode mode) noexcept;

    [[nodiscard]] CursorType GetCursorType() const noexcept;
    void SetCursorType(CursorType type) noexcept;

    [[nodiscard]] void* GetNativeHandle() const noexcept;

    [[nodiscard]] bool ShouldClose() const noexcept;
    void PollEvents() const noexcept;
    void WaitEvents() const noexcept;
    void Close() noexcept;

    CallbackId AddEventCallback(EventCallback callback);
    void RemoveEventCallback(CallbackId id);

    CallbackId AddResizeCallback(ResizeCallback callback);
    void RemoveResizeCallback(CallbackId id);

private:
    bool Initialize(const WindowOptions& options) noexcept;
    bool CreateGlfwWindow(const WindowOptions& options) noexcept;

    void SetupCallbacks() noexcept;
    void UpdateWindowMetrics() noexcept;
    void HandleResize(u32 width, u32 height) noexcept;
    void HandleContentScaleChanged(f32 xscale, f32 yscale) noexcept;
    void HandleCursorMoved(f32 x, f32 y) noexcept;
    void HandleMouseButton(i32 button, i32 action) noexcept;
    void HandleWindowCloseRequested() noexcept;
    void EmitEvent(events::Event& event);

    template <typename T, typename... Args>
    void EmitEvent(Args&&... args) {
        T event(std::forward<Args>(args)...);
        EmitEvent(event);
    }

#ifdef __EMSCRIPTEN__
    void SetupEmscriptenResize() noexcept;
#endif

    struct Impl;
    scope<Impl> impl_;

    std::map<CallbackId, EventCallback> event_callbacks_;
    std::map<CallbackId, ResizeCallback> resize_callbacks_;
    CallbackId next_callback_id_{1};

    std::string title_{"woki"};
    u32 width_{0};
    u32 height_{0};
    f32 aspect_ratio_{1.0f};
    bool fullscreen_{false};

    CursorMode cursor_mode_{CursorMode::kNormal};
    CursorType cursor_type_{CursorType::kArrow};

    f32 content_scale_x_{1.0f};
    f32 content_scale_y_{1.0f};

    f32 cursor_x_{0.0f};
    f32 cursor_y_{0.0f};
    bool has_cursor_position_{false};
    bool close_event_emitted_{false};
    std::array<bool, 8> mouse_buttons_down_{};
};

} // namespace woki
