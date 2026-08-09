#include <GLFW/glfw3.h>

#include "../../include/woki/window/window.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#else
#include "window_icon_data.hpp"
#endif

#include <array>
#include <mutex>
#include <vector>

namespace woki {
namespace {

constexpr u32 kMouseButtonCount = 8;

class GlfwRuntime final {
public:
    [[nodiscard]] static bool Acquire() noexcept {
        const std::scoped_lock lock(mutex_);
        if (reference_count_ == 0) {
            glfwSetErrorCallback(GlfwErrorCallback);
            if (glfwInit() == GLFW_FALSE) {
                slog::Critical("GLFW initialization failed");
                return false;
            }
        }

        ++reference_count_;
        return true;
    }

    static void Release() noexcept {
        const std::scoped_lock lock(mutex_);
        if (reference_count_ == 0) {
            return;
        }

        --reference_count_;
        if (reference_count_ == 0) {
            glfwTerminate();
        }
    }

private:
    static void GlfwErrorCallback(int error, const char* description) {
        slog::Error("GLFW error ({}): {}", error, description != nullptr ? description : "(null)");
    }

    static inline std::mutex mutex_{};
    static inline u32 reference_count_{0};
};

events::MouseButton ToMouseButton(int button) noexcept {
    return static_cast<events::MouseButton>(button);
}

bool IsSupportedMouseButton(int button) noexcept {
    return button >= 0 && button < static_cast<int>(kMouseButtonCount);
}

GLFWcursor* CreateStandardCursor(CursorType type) noexcept {
    switch (type) {
        case CursorType::kArrow:
            return glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
        case CursorType::kIBeam:
            return glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
        case CursorType::kCrosshair:
            return glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);
        case CursorType::kHand:
            return glfwCreateStandardCursor(GLFW_HAND_CURSOR);
        case CursorType::kHResize:
            return glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
        case CursorType::kVResize:
            return glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
    }

    return nullptr;
}

[[nodiscard]] ref<Window> GetWindow(GLFWwindow* window) noexcept {
    auto* observer = static_cast<Window*>(glfwGetWindowUserPointer(window));
    return observer != nullptr ? observer->weak_from_this().lock() : nullptr;
}

#ifndef __EMSCRIPTEN__
void ApplyWindowIcon(GLFWwindow* window) noexcept {
    if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
        return;
    }

    std::array<GLFWimage, 2> images{};
    images[0].width = static_cast<int>(detail::kWindowIcon32Width);
    images[0].height = static_cast<int>(detail::kWindowIcon32Height);
    images[0].pixels = const_cast<unsigned char*>(detail::kWindowIcon32Rgba.data());
    images[1].width = static_cast<int>(detail::kWindowIcon64Width);
    images[1].height = static_cast<int>(detail::kWindowIcon64Height);
    images[1].pixels = const_cast<unsigned char*>(detail::kWindowIcon64Rgba.data());
    glfwSetWindowIcon(window, static_cast<int>(images.size()), images.data());
}
#endif

} // namespace

struct Window::Impl {
    GLFWwindow* window{nullptr};
    GLFWcursor* cursor{nullptr};
    bool owns_runtime{false};

    ~Impl() {
        Destroy();
    }

    void Destroy() noexcept {
        if (cursor != nullptr) {
            glfwDestroyCursor(cursor);
            cursor = nullptr;
        }

        if (window != nullptr) {
            glfwSetWindowUserPointer(window, nullptr);
            glfwDestroyWindow(window);
            window = nullptr;
        }

        if (owns_runtime) {
            GlfwRuntime::Release();
            owns_runtime = false;
        }
    }
};

Result<ref<Window>> Window::Create(const WindowOptions& options) {
    auto window = createRef<Window>(ConstructionKey{});

    if (!window->Initialize(options)) {
        return Err(ErrorCode::FailedToAcquireResource, "Failed to create platform window");
    }

    return Ok(std::move(window));
}

Window::Window(ConstructionKey)
    : impl_(createScope<Impl>()) {}

Window::~Window() {
    if (impl_ != nullptr) {
        impl_->Destroy();
    }
}

bool Window::Initialize(const WindowOptions& options) noexcept {
    if (impl_ == nullptr || !GlfwRuntime::Acquire()) {
        return false;
    }
    impl_->owns_runtime = true;

    return CreateGlfwWindow(options);
}

bool Window::CreateGlfwWindow(const WindowOptions& options) noexcept {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_FLOATING, options.floating ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, options.resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, options.decorated ? GLFW_TRUE : GLFW_FALSE);
#ifdef GLFW_WAYLAND_APP_ID
    glfwWindowHintString(GLFW_WAYLAND_APP_ID, "woki.studio");
#endif
#ifdef GLFW_X11_CLASS_NAME
    glfwWindowHintString(GLFW_X11_CLASS_NAME, "woki-studio");
    glfwWindowHintString(GLFW_X11_INSTANCE_NAME, "studio");
#endif

    GLFWmonitor* monitor = options.fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    impl_->window = glfwCreateWindow(static_cast<int>(options.width), static_cast<int>(options.height), options.title.c_str(), monitor, nullptr);

    if (impl_->window == nullptr) {
        slog::Critical("Failed to create GLFW window");
        return false;
    }

    title_ = options.title;
    fullscreen_ = options.fullscreen;

    UpdateWindowMetrics();
    SetupCallbacks();

#ifndef __EMSCRIPTEN__
    ApplyWindowIcon(impl_->window);
#endif

#ifdef __EMSCRIPTEN__
    SetupEmscriptenResize();
#endif

    return true;
}

void Window::SetupCallbacks() noexcept {
    glfwSetWindowUserPointer(impl_->window, this);

    glfwSetWindowCloseCallback(impl_->window, [](GLFWwindow* window) {
        auto self = GetWindow(window);
        if (self == nullptr) {
            return;
        }

        self->HandleWindowCloseRequested();
    });

    glfwSetFramebufferSizeCallback(impl_->window, [](GLFWwindow* window, int width, int height) {
        auto self = GetWindow(window);
        if (self == nullptr) {
            return;
        }

        self->HandleResize(static_cast<u32>(width), static_cast<u32>(height));
    });

    glfwSetWindowPosCallback(impl_->window, [](GLFWwindow* window, int xpos, int ypos) {
        auto self = GetWindow(window);
        if (self == nullptr) {
            return;
        }

        self->EmitEvent<events::WindowMovedEvent>(xpos, ypos);
    });

    glfwSetWindowFocusCallback(impl_->window, [](GLFWwindow* window, int focused) {
        auto self = GetWindow(window);
        if (self == nullptr) {
            return;
        }

        if (focused != 0) {
            self->EmitEvent<events::WindowFocusEvent>();
        } else {
            self->EmitEvent<events::WindowLostFocusEvent>();
        }
    });

    glfwSetWindowIconifyCallback(impl_->window, [](GLFWwindow* window, int iconified) {
        auto self = GetWindow(window);
        if (self == nullptr) {
            return;
        }

        if (iconified != 0) {
            self->EmitEvent<events::WindowMinimizedEvent>();
        } else {
            self->EmitEvent<events::WindowRestoredEvent>();
        }
    });

    glfwSetWindowMaximizeCallback(impl_->window, [](GLFWwindow* window, int maximized) {
        auto self = GetWindow(window);
        if (self == nullptr) {
            return;
        }

        if (maximized != 0) {
            self->EmitEvent<events::WindowMaximizedEvent>();
        } else {
            self->EmitEvent<events::WindowRestoredEvent>();
        }
    });

    glfwSetWindowContentScaleCallback(impl_->window, [](GLFWwindow* window, float xscale, float yscale) {
        auto self = GetWindow(window);
        if (self == nullptr) {
            return;
        }

        self->HandleContentScaleChanged(xscale, yscale);
    });

    glfwSetKeyCallback(impl_->window, [](GLFWwindow* window, int key, int, int action, int) {
        auto self = GetWindow(window);
        if (self == nullptr || key < 0) {
            return;
        }

        const auto key_code = static_cast<events::KeyCode>(key);
        switch (action) {
            case GLFW_PRESS:
                self->EmitEvent<events::KeyPressedEvent>(key_code, 0u);
                break;
            case GLFW_REPEAT:
                self->EmitEvent<events::KeyPressedEvent>(key_code, 1u);
                break;
            case GLFW_RELEASE:
                self->EmitEvent<events::KeyReleasedEvent>(key_code);
                break;
            default:
                break;
        }
    });

    glfwSetCharCallback(impl_->window, [](GLFWwindow* window, unsigned int codepoint) {
        auto self = GetWindow(window);
        if (self == nullptr) {
            return;
        }

        self->EmitEvent<events::KeyTypedEvent>(static_cast<u32>(codepoint));
    });

    glfwSetCursorPosCallback(impl_->window, [](GLFWwindow* window, double xpos, double ypos) {
        auto self = GetWindow(window);
        if (self == nullptr) {
            return;
        }

        self->HandleCursorMoved(static_cast<f32>(xpos), static_cast<f32>(ypos));
    });

    glfwSetScrollCallback(impl_->window, [](GLFWwindow* window, double xoffset, double yoffset) {
        auto self = GetWindow(window);
        if (self == nullptr) {
            return;
        }

        self->EmitEvent<events::MouseScrolledEvent>(static_cast<f32>(xoffset), static_cast<f32>(yoffset));
    });

    glfwSetMouseButtonCallback(impl_->window, [](GLFWwindow* window, int button, int action, int) {
        auto self = GetWindow(window);
        if (self == nullptr) {
            return;
        }

        self->HandleMouseButton(button, action);
    });

    glfwSetCursorEnterCallback(impl_->window, [](GLFWwindow* window, int entered) {
        auto self = GetWindow(window);
        if (self == nullptr) {
            return;
        }

        if (entered != 0) {
            self->EmitEvent<events::MouseEnteredEvent>();
        } else {
            self->EmitEvent<events::MouseLeftEvent>();
        }
    });
}

void Window::UpdateWindowMetrics() noexcept {
    if (impl_ == nullptr || impl_->window == nullptr) {
        return;
    }

    int framebuffer_width = 0;
    int framebuffer_height = 0;
    glfwGetFramebufferSize(impl_->window, &framebuffer_width, &framebuffer_height);

    width_ = framebuffer_width > 0 ? static_cast<u32>(framebuffer_width) : 0;
    height_ = framebuffer_height > 0 ? static_cast<u32>(framebuffer_height) : 0;
    aspect_ratio_ = height_ > 0 ? static_cast<f32>(width_) / static_cast<f32>(height_) : 1.0f;

    float xscale = 1.0f;
    float yscale = 1.0f;
    glfwGetWindowContentScale(impl_->window, &xscale, &yscale);
    content_scale_x_ = xscale;
    content_scale_y_ = yscale;
}

void Window::HandleResize(u32 width, u32 height) noexcept {
    if (width == 0 || height == 0) {
        return;
    }

    if (width == width_ && height == height_) {
        return;
    }

    width_ = width;
    height_ = height;
    aspect_ratio_ = static_cast<f32>(width_) / static_cast<f32>(height_);

    EmitEvent<events::WindowResizeEvent>(width_, height_);
    EmitEvent<events::ViewportResizeEvent>(width_, height_);

    std::vector<ResizeCallback> callbacks;
    callbacks.reserve(resize_callbacks_.size());
    for (const auto& [id, callback] : resize_callbacks_) {
        (void)id;
        if (callback) {
            callbacks.push_back(callback);
        }
    }

    for (const auto& callback : callbacks) {
        callback(width_, height_);
    }
}

void Window::HandleContentScaleChanged(f32 xscale, f32 yscale) noexcept {
    content_scale_x_ = xscale;
    content_scale_y_ = yscale;
    EmitEvent<events::WindowScaleChangedEvent>(xscale, yscale);
}

void Window::HandleCursorMoved(f32 x, f32 y) noexcept {
    f32 delta_x = 0.0f;
    f32 delta_y = 0.0f;
    if (has_cursor_position_) {
        delta_x = x - cursor_x_;
        delta_y = y - cursor_y_;
    }

    cursor_x_ = x;
    cursor_y_ = y;
    has_cursor_position_ = true;

    EmitEvent<events::MouseMovedEvent>(x, y, delta_x, delta_y);
}

void Window::HandleMouseButton(i32 button, i32 action) noexcept {
    if (!IsSupportedMouseButton(button)) {
        return;
    }

    const auto mouse_button = ToMouseButton(button);
    const auto x = cursor_x_;
    const auto y = cursor_y_;

    if (action == GLFW_PRESS) {
        mouse_buttons_down_[static_cast<std::size_t>(button)] = true;
        EmitEvent<events::MouseButtonPressedEvent>(mouse_button, x, y);
        return;
    }

    if (action != GLFW_RELEASE) {
        return;
    }

    const bool was_pressed = mouse_buttons_down_[static_cast<std::size_t>(button)];
    mouse_buttons_down_[static_cast<std::size_t>(button)] = false;

    EmitEvent<events::MouseButtonReleasedEvent>(mouse_button, x, y);
    if (was_pressed) {
        EmitEvent<events::MouseButtonClickedEvent>(mouse_button, x, y);
    }
}

void Window::HandleWindowCloseRequested() noexcept {
    if (close_event_emitted_) {
        return;
    }

    close_event_emitted_ = true;
    EmitEvent<events::WindowCloseEvent>();
}

#ifdef __EMSCRIPTEN__
void Window::SetupEmscriptenResize() noexcept {
    auto callback = [](int, const EmscriptenUiEvent*, void* user_data) -> EM_BOOL {
        auto* observer = static_cast<Window*>(user_data);
        auto self = observer != nullptr ? observer->weak_from_this().lock() : nullptr;
        if (self == nullptr) {
            return EM_FALSE;
        }

        double css_width = 0.0;
        double css_height = 0.0;
        emscripten_get_element_css_size("canvas", &css_width, &css_height);

        const double device_pixel_ratio = emscripten_get_device_pixel_ratio();
        const u32 pixel_width = static_cast<u32>(css_width * device_pixel_ratio);
        const u32 pixel_height = static_cast<u32>(css_height * device_pixel_ratio);

        if (pixel_width == 0 || pixel_height == 0) {
            return EM_FALSE;
        }

        emscripten_set_canvas_element_size("canvas", static_cast<int>(pixel_width), static_cast<int>(pixel_height));
        self->HandleResize(pixel_width, pixel_height);
        return EM_TRUE;
    };

    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, false, callback);
    UpdateWindowMetrics();
}
#endif

void* Window::GetNativeHandle() const noexcept {
    if (impl_ == nullptr || impl_->window == nullptr) {
        return nullptr;
    }

    return static_cast<void*>(impl_->window);
}

bool Window::ShouldClose() const noexcept {
    if (impl_ == nullptr || impl_->window == nullptr) {
        return true;
    }

    return glfwWindowShouldClose(impl_->window) != 0;
}

void Window::PollEvents() const noexcept {
    if (impl_ == nullptr || !impl_->owns_runtime) {
        return;
    }

    glfwPollEvents();
}

void Window::WaitEvents() const noexcept {
    if (impl_ == nullptr || !impl_->owns_runtime) {
        return;
    }

    glfwWaitEvents();
}

void Window::Close() noexcept {
    if (impl_ == nullptr || impl_->window == nullptr) {
        return;
    }

    [[maybe_unused]] const auto keep_alive = weak_from_this().lock();
    HandleWindowCloseRequested();

    impl_->Destroy();
}

void Window::EmitEvent(events::Event& event) {
    event.timestamp = Clock::Seconds();

    std::vector<EventCallback> callbacks;
    callbacks.reserve(event_callbacks_.size());
    for (const auto& [id, callback] : event_callbacks_) {
        (void)id;
        if (callback) {
            callbacks.push_back(callback);
        }
    }

    for (const auto& callback : callbacks) {
        callback(event);
    }
}

void Window::SetCursorMode(CursorMode mode) noexcept {
    if (impl_ == nullptr || impl_->window == nullptr) {
        return;
    }

    int glfw_cursor_mode = GLFW_CURSOR_NORMAL;
    switch (mode) {
        case CursorMode::kNormal:
            glfw_cursor_mode = GLFW_CURSOR_NORMAL;
            break;
        case CursorMode::kHidden:
            glfw_cursor_mode = GLFW_CURSOR_HIDDEN;
            break;
        case CursorMode::kDisabled:
            glfw_cursor_mode = GLFW_CURSOR_DISABLED;
            break;
    }

    glfwSetInputMode(impl_->window, GLFW_CURSOR, glfw_cursor_mode);
    cursor_mode_ = mode;
}

void Window::SetCursorType(CursorType type) noexcept {
    if (impl_ == nullptr || impl_->window == nullptr) {
        return;
    }

    GLFWcursor* cursor = CreateStandardCursor(type);
    if (cursor == nullptr) {
        slog::Warn("Failed to create GLFW cursor for type {}", static_cast<int>(type));
        return;
    }

    glfwSetCursor(impl_->window, cursor);
    if (impl_->cursor != nullptr) {
        glfwDestroyCursor(impl_->cursor);
    }

    impl_->cursor = cursor;
    cursor_type_ = type;
}

CallbackId Window::AddEventCallback(EventCallback callback) {
    const CallbackId callback_id = next_callback_id_++;
    event_callbacks_.emplace(callback_id, std::move(callback));
    return callback_id;
}

void Window::RemoveEventCallback(CallbackId id) {
    event_callbacks_.erase(id);
}

CallbackId Window::AddResizeCallback(ResizeCallback callback) {
    const CallbackId callback_id = next_callback_id_++;
    resize_callbacks_.emplace(callback_id, std::move(callback));
    return callback_id;
}

void Window::RemoveResizeCallback(CallbackId id) {
    resize_callbacks_.erase(id);
}

const std::string& Window::GetTitle() const noexcept {
    return title_;
}

u32 Window::GetWidth() const noexcept {
    return width_;
}

u32 Window::GetHeight() const noexcept {
    return height_;
}

f32 Window::GetAspectRatio() const noexcept {
    return aspect_ratio_;
}

bool Window::IsFullscreen() const noexcept {
    return fullscreen_;
}

f32 Window::GetContentScaleX() const noexcept {
    return content_scale_x_;
}

f32 Window::GetContentScaleY() const noexcept {
    return content_scale_y_;
}

CursorMode Window::GetCursorMode() const noexcept {
    return cursor_mode_;
}

CursorType Window::GetCursorType() const noexcept {
    return cursor_type_;
}

} // namespace woki
