#pragma once

#include <functional>
#include <unordered_map>

#include <woki/core.hpp>
#include <woki/platform.hpp>

#include "context.hpp"
#include "layer_stack.hpp"

namespace woki {

struct ApplicationSettings {
    std::string title{"studio"};
    u32 width{1280};
    u32 height{720};
    bool floating{false};
    bool fullscreen{false};
    bool resizable{true};
    bool decorated{true};
};

class Application {
public:
    using EventCallback = std::function<void(events::Event&)>;

    explicit Application(ApplicationSettings settings = {});
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    void Run();
    [[nodiscard]] bool Tick();

    CallbackId AddEventCallback(EventCallback callback);
    void RemoveEventCallback(CallbackId id);

private:
    void Initialize();
    void Shutdown() noexcept;
    [[nodiscard]] bool IsReady() const noexcept;
    [[nodiscard]] Context BuildContext();
    void ConfigureLayers();
    void AttachLayers();
    void DetachLayers() noexcept;
    void Start();
    void Stop() noexcept;
    void EmitEvent(events::Event& event);

    template <typename T, typename... Args>
    void EmitEvent(Args&&... args) {
        T event(std::forward<Args>(args)...);
        EmitEvent(event);
    }

    ApplicationSettings settings_{};
    ref<Window> window_;
    scope<LayerStack> layers_;
    std::unordered_map<CallbackId, EventCallback> event_callbacks_;
    CallbackId next_callback_id_{1};
    CallbackId window_event_callback_id_{0};
    FrameTimer frame_timer_{};
    bool layers_configured_{false};
    bool is_running_{false};
};

} // namespace woki
