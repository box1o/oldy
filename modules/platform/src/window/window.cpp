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
#include <algorithm>
#include <exception>

namespace woki {
namespace {

constexpr u32 kMouseButtonCount = 8;

#ifdef __EMSCRIPTEN__
Window* emscripten_resize_window = nullptr;

extern "C" EMSCRIPTEN_KEEPALIVE void woki_web_pointer_event(int phase,
    int kind,
    double pointer_id,
    int primary,
    int button,
    unsigned buttons,
    float x,
    float y,
    float dx,
    float dy,
    float pressure,
    float width,
    float height,
    float tilt_x,
    float tilt_y,
    float twist,
    unsigned modifiers) {
    if (emscripten_resize_window == nullptr)
        return;
    events::PointerData data{.pointer = static_cast<events::PointerId>(pointer_id),
        .kind = static_cast<events::PointerKind>(kind),
        .primary = primary != 0,
        .button = button < 0 ? events::PointerButton::kNone : static_cast<events::PointerButton>(button),
        .buttons = static_cast<events::PointerButtons>(buttons),
        .x = x,
        .y = y,
        .delta_x = dx,
        .delta_y = dy,
        .pressure = pressure,
        .contact_width = width,
        .contact_height = height,
        .tilt_x = tilt_x,
        .tilt_y = tilt_y,
        .twist = twist};
    scope<events::Event> event;
    switch (phase) {
        case 0:
            event = createScope<events::PointerDownEvent>(data);
            break;
        case 1:
            event = createScope<events::PointerMoveEvent>(data);
            break;
        case 2:
            event = createScope<events::PointerUpEvent>(data);
            break;
        case 3:
            event = createScope<events::PointerCancelEvent>(data);
            break;
        case 4:
            event = createScope<events::PointerEnterEvent>(data);
            break;
        case 5:
            event = createScope<events::PointerLeaveEvent>(data);
            break;
        default:
            break;
    }
    if (event != nullptr) {
        event->metadata.modifiers = static_cast<events::ModifierFlags>(modifiers);
        emscripten_resize_window->SubmitPlatformEvent(std::move(event));
    }
}

extern "C" EMSCRIPTEN_KEEPALIVE void woki_web_scroll_event(float dx, float dy, int unit, int precise, float x, float y, unsigned modifiers) {
    if (emscripten_resize_window == nullptr)
        return;
    auto event = createScope<events::ScrollEvent>(dx, dy);
    event->unit = static_cast<events::ScrollUnit>(unit);
    event->precise = precise != 0;
    event->x = x;
    event->y = y;
    event->metadata.modifiers = static_cast<events::ModifierFlags>(modifiers);
    emscripten_resize_window->SubmitPlatformEvent(std::move(event));
}

extern "C" EMSCRIPTEN_KEEPALIVE void woki_web_magnify_event(float scale_delta, float x, float y) {
    if (emscripten_resize_window == nullptr)
        return;
    events::GestureData data{.phase = events::GesturePhase::kUpdate, .center_x = x, .center_y = y, .pointer_count = 0};
    emscripten_resize_window->SubmitPlatformEvent(createScope<events::PinchEvent>(data, scale_delta, scale_delta));
}

extern "C" EMSCRIPTEN_KEEPALIVE void woki_web_composition_event(int phase, const char* text) {
    if (emscripten_resize_window == nullptr)
        return;
    const std::string value = text != nullptr ? text : "";
    switch (phase) {
        case 0:
            emscripten_resize_window->SubmitPlatformEvent(createScope<events::TextCompositionStartedEvent>(value));
            break;
        case 1:
            emscripten_resize_window->SubmitPlatformEvent(createScope<events::TextCompositionUpdatedEvent>(value));
            break;
        case 2:
            emscripten_resize_window->SubmitPlatformEvent(createScope<events::TextCompositionCommittedEvent>(value));
            break;
        case 3:
            emscripten_resize_window->SubmitPlatformEvent(createScope<events::TextCompositionCanceledEvent>(value));
            break;
        default:
            break;
    }
}
#endif

template <typename CallbackMap, typename... Args>
void InvokeCallbacks(const CallbackMap& source, Args&&... args) noexcept {
    try {
        std::vector<typename CallbackMap::mapped_type> callbacks;
        callbacks.reserve(source.size());
        for (const auto& [id, callback] : source) {
            (void)id;
            if (callback) {
                callbacks.push_back(callback);
            }
        }

        for (const auto& callback : callbacks) {
            try {
                callback(args...);
            } catch (const std::exception& error) {
                slog::Error("Window callback failed: {}", error.what());
            } catch (...) {
                slog::Error("Window callback failed with an unknown exception");
            }
        }
    } catch (const std::exception& error) {
        slog::Error("Failed to prepare window callbacks: {}", error.what());
    } catch (...) {
        slog::Error("Failed to prepare window callbacks with an unknown exception");
    }
}

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
            glfwSetMonitorCallback([](GLFWmonitor* monitor, int event) {
                const char* raw_name = glfwGetMonitorName(monitor);
                const std::string name = raw_name != nullptr ? raw_name : "unknown";
                for (Window* window : windows_) {
                    if (window == nullptr)
                        continue;
                    if (event == GLFW_CONNECTED)
                        window->SubmitPlatformEvent(createScope<events::MonitorConnectedEvent>(name));
                    else if (event == GLFW_DISCONNECTED)
                        window->SubmitPlatformEvent(createScope<events::MonitorDisconnectedEvent>(name));
                }
            });
        }

        ++reference_count_;
        return true;
    }

    static void Register(Window* window) {
        const std::scoped_lock lock(mutex_);
        windows_.push_back(window);
    }

    static void Unregister(Window* window) {
        const std::scoped_lock lock(mutex_);
        std::erase(windows_, window);
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
        const std::string message = description != nullptr ? description : "(null)";
        slog::Error("GLFW error ({}): {}", error, message);
        for (Window* window : windows_)
            if (window != nullptr)
                window->SubmitPlatformEvent(createScope<events::PlatformErrorEvent>(error, message));
    }

    static inline std::mutex mutex_{};
    static inline u32 reference_count_{0};
    static inline std::vector<Window*> windows_{};
};

events::PointerButton ToPointerButton(int button) noexcept {
    return static_cast<events::PointerButton>(button);
}

events::ModifierFlags ToModifiers(int mods) noexcept {
    events::ModifierFlags result = 0;
    if ((mods & GLFW_MOD_SHIFT) != 0)
        result |= static_cast<events::ModifierFlags>(events::Modifier::kShift);
    if ((mods & GLFW_MOD_CONTROL) != 0)
        result |= static_cast<events::ModifierFlags>(events::Modifier::kControl);
    if ((mods & GLFW_MOD_ALT) != 0)
        result |= static_cast<events::ModifierFlags>(events::Modifier::kAlt);
    if ((mods & GLFW_MOD_SUPER) != 0)
        result |= static_cast<events::ModifierFlags>(events::Modifier::kSuper);
    if ((mods & GLFW_MOD_CAPS_LOCK) != 0)
        result |= static_cast<events::ModifierFlags>(events::Modifier::kCapsLock);
    if ((mods & GLFW_MOD_NUM_LOCK) != 0)
        result |= static_cast<events::ModifierFlags>(events::Modifier::kNumLock);
    return result;
}

std::string EncodeUtf8(u32 codepoint) {
    std::string result;
    if (codepoint <= 0x7f)
        result.push_back(static_cast<char>(codepoint));
    else if (codepoint <= 0x7ff) {
        result.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
        result.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0x10ffff) {
        result.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
        result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
        result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
    return result;
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
#ifdef __EMSCRIPTEN__
    if (emscripten_resize_window == this) {
        EM_ASM({ Module.removeWokiInput(); });
        emscripten_resize_window = nullptr;
        emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, false, nullptr);
    }
#endif

    GlfwRuntime::Unregister(this);
    if (impl_ != nullptr) {
        impl_->Destroy();
    }
}

bool Window::Initialize(const WindowOptions& options) noexcept {
    if (impl_ == nullptr || !GlfwRuntime::Acquire()) {
        return false;
    }
    impl_->owns_runtime = true;
    GlfwRuntime::Register(this);

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
    input_capabilities_.mouse = true;
    input_capabilities_.gamepads = true;
#ifdef __EMSCRIPTEN__
    input_capabilities_.touch = true;
    input_capabilities_.pen = true;
    input_capabilities_.pressure = true;
    input_capabilities_.precise_scroll = true;
    input_capabilities_.gestures = true;
    input_capabilities_.text_composition = true;
#endif

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

    glfwSetWindowSizeCallback(impl_->window, [](GLFWwindow* window, int width, int height) {
        auto self = GetWindow(window);
        if (self == nullptr || width <= 0 || height <= 0)
            return;
        self->logical_width_ = static_cast<u32>(width);
        self->logical_height_ = static_cast<u32>(height);
        self->QueueEvent<events::WindowResizeEvent>(self->logical_width_, self->logical_height_);
    });

    glfwSetWindowRefreshCallback(impl_->window, [](GLFWwindow* window) {
        if (auto self = GetWindow(window))
            self->QueueEvent<events::WindowRefreshEvent>();
    });

    glfwSetDropCallback(impl_->window, [](GLFWwindow* window, int count, const char** paths) {
        auto self = GetWindow(window);
        if (self == nullptr || paths == nullptr || count <= 0)
            return;
        std::vector<std::string> values;
        values.reserve(static_cast<std::size_t>(count));
        for (int index = 0; index < count; ++index)
            if (paths[index] != nullptr)
                values.emplace_back(paths[index]);
        self->QueueEvent<events::FilesDroppedEvent>(std::move(values));
    });

    glfwSetWindowPosCallback(impl_->window, [](GLFWwindow* window, int xpos, int ypos) {
        auto self = GetWindow(window);
        if (self == nullptr) {
            return;
        }

        self->QueueEvent<events::WindowMovedEvent>(xpos, ypos);
    });

    glfwSetWindowFocusCallback(impl_->window, [](GLFWwindow* window, int focused) {
        auto self = GetWindow(window);
        if (self == nullptr) {
            return;
        }

        if (focused != 0) {
            self->QueueEvent<events::WindowFocusEvent>();
        } else {
            self->QueueEvent<events::WindowLostFocusEvent>();
            self->QueueEvent<events::PointerCancelEvent>(
                events::PointerData{.pointer = 1, .kind = events::PointerKind::kMouse, .primary = true, .buttons = self->pointer_buttons_, .x = self->cursor_x_, .y = self->cursor_y_}
            );
        }
    });

    glfwSetWindowIconifyCallback(impl_->window, [](GLFWwindow* window, int iconified) {
        auto self = GetWindow(window);
        if (self == nullptr) {
            return;
        }

        if (iconified != 0) {
            self->QueueEvent<events::WindowMinimizedEvent>();
        } else {
            self->QueueEvent<events::WindowRestoredEvent>();
        }
    });

    glfwSetWindowMaximizeCallback(impl_->window, [](GLFWwindow* window, int maximized) {
        auto self = GetWindow(window);
        if (self == nullptr) {
            return;
        }

        if (maximized != 0) {
            self->QueueEvent<events::WindowMaximizedEvent>();
        } else {
            self->QueueEvent<events::WindowRestoredEvent>();
        }
    });

    glfwSetWindowContentScaleCallback(impl_->window, [](GLFWwindow* window, float xscale, float yscale) {
        auto self = GetWindow(window);
        if (self == nullptr) {
            return;
        }

        self->HandleContentScaleChanged(xscale, yscale);
    });

    glfwSetKeyCallback(impl_->window, [](GLFWwindow* window, int key, int scan_code, int action, int mods) {
        auto self = GetWindow(window);
        if (self == nullptr || key < 0) {
            return;
        }

        const auto key_code = static_cast<events::KeyCode>(key);
        self->modifiers_ = ToModifiers(mods);
        switch (action) {
            case GLFW_PRESS:
                self->QueueEvent<events::KeyPressedEvent>(key_code, 0u, scan_code);
                break;
            case GLFW_REPEAT:
                self->QueueEvent<events::KeyPressedEvent>(key_code, 1u, scan_code);
                break;
            case GLFW_RELEASE:
                self->QueueEvent<events::KeyReleasedEvent>(key_code, scan_code);
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

        self->QueueEvent<events::TextInputEvent>(EncodeUtf8(static_cast<u32>(codepoint)));
    });

#ifndef __EMSCRIPTEN__
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

        self->QueueEvent<events::ScrollEvent>(static_cast<f32>(xoffset), static_cast<f32>(yoffset));
    });

    glfwSetMouseButtonCallback(impl_->window, [](GLFWwindow* window, int button, int action, int modifiers) {
        auto self = GetWindow(window);
        if (self == nullptr) {
            return;
        }

        self->HandleMouseButton(button, action, modifiers);
    });

    glfwSetCursorEnterCallback(impl_->window, [](GLFWwindow* window, int entered) {
        auto self = GetWindow(window);
        if (self == nullptr) {
            return;
        }

        if (entered != 0) {
            events::PointerData data{.pointer = 1, .kind = events::PointerKind::kMouse, .primary = true, .buttons = self->pointer_buttons_, .x = self->cursor_x_, .y = self->cursor_y_};
            self->QueueEvent<events::PointerEnterEvent>(data);
        } else {
            events::PointerData data{.pointer = 1, .kind = events::PointerKind::kMouse, .primary = true, .buttons = self->pointer_buttons_, .x = self->cursor_x_, .y = self->cursor_y_};
            self->QueueEvent<events::PointerLeaveEvent>(data);
        }
    });
#endif
}

void Window::UpdateWindowMetrics() noexcept {
    if (impl_ == nullptr || impl_->window == nullptr) {
        return;
    }

    int framebuffer_width = 0;
    int framebuffer_height = 0;
    glfwGetFramebufferSize(impl_->window, &framebuffer_width, &framebuffer_height);
    int logical_width = 0;
    int logical_height = 0;
    glfwGetWindowSize(impl_->window, &logical_width, &logical_height);

    width_ = framebuffer_width > 0 ? static_cast<u32>(framebuffer_width) : 0;
    height_ = framebuffer_height > 0 ? static_cast<u32>(framebuffer_height) : 0;
    logical_width_ = logical_width > 0 ? static_cast<u32>(logical_width) : 0;
    logical_height_ = logical_height > 0 ? static_cast<u32>(logical_height) : 0;
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

    QueueEvent<events::FramebufferResizeEvent>(width_, height_);
    QueueEvent<events::ViewportResizeEvent>(width_, height_);

    InvokeCallbacks(resize_callbacks_, width_, height_);
}

void Window::HandleContentScaleChanged(f32 xscale, f32 yscale) noexcept {
    content_scale_x_ = xscale;
    content_scale_y_ = yscale;
    QueueEvent<events::WindowScaleChangedEvent>(xscale, yscale);
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

    QueueEvent<events::PointerMoveEvent>(events::PointerData{.pointer = 1, .kind = events::PointerKind::kMouse, .primary = true, .buttons = pointer_buttons_, .x = x, .y = y, .delta_x = delta_x, .delta_y = delta_y});
}

void Window::HandleMouseButton(i32 button, i32 action, i32 modifiers) noexcept {
    if (!IsSupportedMouseButton(button)) {
        return;
    }

    modifiers_ = ToModifiers(modifiers);
    const auto pointer_button = ToPointerButton(button);
    const auto x = cursor_x_;
    const auto y = cursor_y_;

    if (action == GLFW_PRESS) {
        mouse_buttons_down_[static_cast<std::size_t>(button)] = true;
        pointer_buttons_ |= static_cast<events::PointerButtons>(1u << static_cast<u32>(button));
        QueueEvent<events::PointerDownEvent>(events::PointerData{.pointer = 1, .kind = events::PointerKind::kMouse, .primary = true, .button = pointer_button, .buttons = pointer_buttons_, .x = x, .y = y, .pressure = 1});
        return;
    }

    if (action != GLFW_RELEASE) {
        return;
    }

    const bool was_pressed = mouse_buttons_down_[static_cast<std::size_t>(button)];
    mouse_buttons_down_[static_cast<std::size_t>(button)] = false;
    pointer_buttons_ &= static_cast<events::PointerButtons>(~(1u << static_cast<u32>(button)));
    if (was_pressed)
        QueueEvent<events::PointerUpEvent>(events::PointerData{.pointer = 1, .kind = events::PointerKind::kMouse, .primary = true, .button = pointer_button, .buttons = pointer_buttons_, .x = x, .y = y});
}

void Window::HandleWindowCloseRequested() noexcept {
    if (close_event_emitted_) {
        return;
    }

    close_event_emitted_ = true;
    QueueEvent<events::WindowCloseEvent>();
}

#ifdef __EMSCRIPTEN__
void Window::SetupEmscriptenResize() noexcept {
    auto callback = [](int, const EmscriptenUiEvent*, void*) -> EM_BOOL {
        auto* observer = emscripten_resize_window;
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

    const auto result = emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, false, callback);
    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
        slog::Error("Failed to register browser resize callback: {}", result);
        return;
    }

    emscripten_resize_window = this;
    EM_ASM({ Module.installWokiInput(); });
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

void Window::PollEvents() noexcept {
    if (impl_ == nullptr || !impl_->owns_runtime) {
        return;
    }

    glfwPollEvents();
    PollGamepads();
    for (auto& gesture : gestures_.Update(Clock::Seconds())) {
        gesture->metadata.source = events::EventSource::kSynthetic;
        QueueEvent(std::move(gesture));
    }
    DrainEvents();
}

void Window::WaitEvents() noexcept {
    if (impl_ == nullptr || !impl_->owns_runtime) {
        return;
    }

    glfwWaitEvents();
    PollGamepads();
    for (auto& gesture : gestures_.Update(Clock::Seconds())) {
        gesture->metadata.source = events::EventSource::kSynthetic;
        QueueEvent(std::move(gesture));
    }
    DrainEvents();
}

void Window::Close() noexcept {
    if (impl_ == nullptr || impl_->window == nullptr) {
        return;
    }

    [[maybe_unused]] const auto keep_alive = weak_from_this().lock();
    HandleWindowCloseRequested();
    DrainEvents();

    impl_->Destroy();
}

void Window::QueueEvent(scope<events::Event> event) noexcept {
    if (event == nullptr)
        return;
    event->metadata.timestamp = Clock::Seconds();
    event->metadata.window = 1;
    event->metadata.modifiers = modifiers_;
    if (event->metadata.source == events::EventSource::kUnknown)
        event->metadata.source = events::EventSource::kNative;

    if (!event_queue_.empty() && event_queue_.back()->GetEventType() == event->GetEventType()) {
        auto& previous = *event_queue_.back();
        if (event->GetEventType() == events::EventType::kPointerMoved) {
            auto& before = static_cast<events::PointerMoveEvent&>(previous).pointer_data;
            const auto& after = static_cast<const events::PointerMoveEvent&>(*event).pointer_data;
            if (before.pointer == after.pointer && before.kind == after.kind) {
                const f32 accumulated_x = before.delta_x + after.delta_x;
                const f32 accumulated_y = before.delta_y + after.delta_y;
                before = after;
                before.delta_x = accumulated_x;
                before.delta_y = accumulated_y;
                previous.metadata.timestamp = event->metadata.timestamp;
                return;
            }
        } else if (event->GetEventType() == events::EventType::kScrolled) {
            auto& before = static_cast<events::ScrollEvent&>(previous);
            const auto& after = static_cast<const events::ScrollEvent&>(*event);
            if (before.kind == after.kind && before.unit == after.unit && before.phase == after.phase) {
                before.delta_x += after.delta_x;
                before.delta_y += after.delta_y;
                before.x = after.x;
                before.y = after.y;
                previous.metadata.timestamp = event->metadata.timestamp;
                return;
            }
        } else if (event->GetEventType() == events::EventType::kGamepadAxisChanged && previous.metadata.device == event->metadata.device) {
            auto& before = static_cast<events::GamepadAxisChangedEvent&>(previous);
            const auto& after = static_cast<const events::GamepadAxisChangedEvent&>(*event);
            if (before.axis == after.axis) {
                before.value = after.value;
                previous.metadata.timestamp = event->metadata.timestamp;
                return;
            }
        }
    }

    event->metadata.sequence = next_event_sequence_++;
    event_queue_.push_back(std::move(event));
}

void Window::DrainEvents() noexcept {
    while (!event_queue_.empty()) {
        auto event = std::move(event_queue_.front());
        event_queue_.pop_front();
        input_state_.Apply(*event);
        auto gesture_events = gestures_.Process(*event);
        InvokeCallbacks(event_callbacks_, *event);
        for (auto& gesture : gesture_events) {
            gesture->metadata.source = events::EventSource::kSynthetic;
            gesture->metadata.device = event->metadata.device;
            QueueEvent(std::move(gesture));
        }
    }
}

void Window::PollGamepads() noexcept {
    for (int joystick = GLFW_JOYSTICK_1; joystick <= GLFW_JOYSTICK_LAST; ++joystick) {
        const events::DeviceId id = static_cast<events::DeviceId>(joystick + 1);
        if (glfwJoystickPresent(joystick) == GLFW_FALSE) {
            if (gamepads_.erase(id) != 0)
                QueueEvent<events::GamepadDisconnectedEvent>(id);
            continue;
        }
        auto found = gamepads_.find(id);
        const bool mapped =
#ifdef __EMSCRIPTEN__
            false;
#else
            glfwJoystickIsGamepad(joystick) == GLFW_TRUE;
#endif
        if (!mapped) {
            int axis_count = 0, button_count = 0, hat_count = 0;
            const float* axes = glfwGetJoystickAxes(joystick, &axis_count);
            const unsigned char* buttons = glfwGetJoystickButtons(joystick, &button_count);
            const unsigned char* hats = glfwGetJoystickHats(joystick, &hat_count);
            if (found == gamepads_.end()) {
                events::GamepadState state;
                state.device = id;
                state.connected = true;
                if (const char* name = glfwGetJoystickName(joystick))
                    state.name = name;
                if (const char* guid = glfwGetJoystickGUID(joystick))
                    state.guid = guid;
                if (axes != nullptr && axis_count > 0)
                    state.raw_axes.assign(axes, axes + axis_count);
                state.raw_buttons.reserve(static_cast<std::size_t>(button_count));
                for (int index = 0; index < button_count; ++index)
                    state.raw_buttons.push_back(buttons[index] == GLFW_PRESS);
                if (hats != nullptr && hat_count > 0)
                    state.raw_hats.assign(hats, hats + hat_count);
                found = gamepads_.emplace(id, std::move(state)).first;
                auto connected = createScope<events::GamepadConnectedEvent>(found->second);
                connected->metadata.device = id;
                QueueEvent(std::move(connected));
                continue;
            }
            auto& state = found->second;
            state.raw_axes.resize(static_cast<std::size_t>(axis_count));
            state.raw_buttons.resize(static_cast<std::size_t>(button_count));
            state.raw_hats.resize(static_cast<std::size_t>(hat_count));
            for (int index = 0; index < axis_count; ++index)
                if (state.raw_axes[static_cast<std::size_t>(index)] != axes[index]) {
                    state.raw_axes[static_cast<std::size_t>(index)] = axes[index];
                    auto changed = createScope<events::JoystickAxisChangedEvent>(static_cast<u16>(index), axes[index]);
                    changed->metadata.device = id;
                    QueueEvent(std::move(changed));
                }
            for (int index = 0; index < button_count; ++index) {
                const bool pressed = buttons[index] == GLFW_PRESS;
                if (state.raw_buttons[static_cast<std::size_t>(index)] != pressed) {
                    state.raw_buttons[static_cast<std::size_t>(index)] = pressed;
                    auto changed = createScope<events::JoystickButtonChangedEvent>(static_cast<u16>(index), pressed);
                    changed->metadata.device = id;
                    QueueEvent(std::move(changed));
                }
            }
            for (int index = 0; index < hat_count; ++index)
                if (state.raw_hats[static_cast<std::size_t>(index)] != hats[index]) {
                    state.raw_hats[static_cast<std::size_t>(index)] = hats[index];
                    auto changed = createScope<events::JoystickHatChangedEvent>(static_cast<u16>(index), hats[index]);
                    changed->metadata.device = id;
                    QueueEvent(std::move(changed));
                }
            continue;
        }
        GLFWgamepadstate native{};
        if (glfwGetGamepadState(joystick, &native) == GLFW_FALSE)
            continue;
        if (found == gamepads_.end()) {
            events::GamepadState state;
            state.device = id;
            state.connected = true;
            state.mapped = true;
            if (const char* name = glfwGetGamepadName(joystick))
                state.name = name;
            if (const char* guid = glfwGetJoystickGUID(joystick))
                state.guid = guid;
            found = gamepads_.emplace(id, std::move(state)).first;
            auto connected = createScope<events::GamepadConnectedEvent>(found->second);
            connected->metadata.device = id;
            QueueEvent(std::move(connected));
        }
        auto& state = found->second;
        for (std::size_t index = 0; index < state.buttons.size(); ++index) {
            const bool pressed = native.buttons[index] == GLFW_PRESS;
            if (state.buttons[index] != pressed) {
                state.buttons[index] = pressed;
                auto event = createScope<events::GamepadButtonChangedEvent>(static_cast<events::GamepadButton>(index), pressed, pressed ? 1.0f : 0.0f);
                event->metadata.device = id;
                QueueEvent(std::move(event));
            }
        }
        for (std::size_t index = 0; index < state.axes.size(); ++index) {
            f32 value = native.axes[index];
            if (index >= static_cast<std::size_t>(events::GamepadAxis::kLeftTrigger))
                value = (value + 1.0f) * 0.5f;
            if (state.axes[index] != value) {
                state.axes[index] = value;
                auto event = createScope<events::GamepadAxisChangedEvent>(static_cast<events::GamepadAxis>(index), value);
                event->metadata.device = id;
                QueueEvent(std::move(event));
            }
        }
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

u32 Window::GetLogicalWidth() const noexcept {
    return logical_width_;
}

u32 Window::GetLogicalHeight() const noexcept {
    return logical_height_;
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

const InputCapabilities& Window::GetInputCapabilities() const noexcept {
    return input_capabilities_;
}

const std::map<events::DeviceId, events::GamepadState>& Window::GetGamepads() const noexcept {
    return gamepads_;
}

const InputState& Window::GetInputState() const noexcept {
    return input_state_;
}

void Window::SubmitPlatformEvent(scope<events::Event> event) noexcept {
#ifdef __EMSCRIPTEN__
    if (event != nullptr) {
        event->metadata.source = events::EventSource::kWeb;
        modifiers_ = event->metadata.modifiers;
    }
#endif
    QueueEvent(std::move(event));
}

} // namespace woki
