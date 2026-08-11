#include <vector>

#include "woki/core.hpp"

#include "application.hpp"
#include "layers/render.hpp"
#include "layers/extension.hpp"

namespace woki {

Application::Application(ApplicationSettings settings)
    : settings_(std::move(settings)) {
    Initialize();
}

Application::~Application() {
    Shutdown();
}

void Application::Initialize() {
    WindowOptions options;
    options.title = settings_.title;
    options.width = settings_.width;
    options.height = settings_.height;
    options.floating = settings_.floating;
    options.fullscreen = settings_.fullscreen;
    options.resizable = settings_.resizable;
    options.decorated = settings_.decorated;

    auto window = Window::Create(options);
    if (!window) {
        slog::Critical("Failed to create application window: {}", window.error().Message());
        return;
    }
    window_ = std::move(*window);

    slog::Info("Created window '{}' ({}x{})", window_->GetTitle(), window_->GetWidth(), window_->GetHeight());

    window_event_callback_id_ = window_->AddEventCallback([this](events::Event& event) { EmitEvent(event); });

    ConfigureLayers();
    AttachLayers();
}

void Application::Shutdown() noexcept {
    EmitEvent<events::AppShutdownEvent>();

    DetachLayers();

    if (window_ != nullptr) {
        window_->RemoveEventCallback(window_event_callback_id_);
        window_->Close();
        window_.reset();
    }

    is_running_ = false;
}

bool Application::IsReady() const noexcept {
    return window_ != nullptr && layers_ != nullptr && layers_->IsAttached();
}

Context Application::BuildContext() {
    WOKI_ASSERT(window_ != nullptr);

    return Context{
        .window = window_.get(),
        .running = &is_running_,
    };
}

void Application::ConfigureLayers() {
    if (layers_configured_) {
        return;
    }

    layers_ = createScope<LayerStack>();
    layers_->PushLayer(createScope<ExtensionLayer>());
    layers_->PushLayer(createScope<RenderLayer>());
    layers_configured_ = true;
}

void Application::AttachLayers() {
    if (layers_ == nullptr || layers_->IsAttached() || window_ == nullptr) {
        return;
    }

    auto ctx = BuildContext();
    layers_->AttachAll(ctx);
}

void Application::DetachLayers() noexcept {
    if (layers_ == nullptr || !layers_->IsAttached() || window_ == nullptr) {
        return;
    }

    auto ctx = BuildContext();
    layers_->DetachAll(ctx);
}

void Application::Start() {
    if (!is_running_) {
        slog::Info("Starting application loop");
        frame_timer_.Reset();
        is_running_ = true;
        EmitEvent<events::AppResumeEvent>();
    }
}

void Application::Stop() noexcept {
    if (is_running_) {
        EmitEvent<events::AppSuspendEvent>();
        is_running_ = false;
        slog::Info("Application loop stopped");
    }
}

bool Application::Tick() {
    if (!IsReady()) {
        slog::Critical("Application cannot run without a window");
        return false;
    }

    Start();

    if (!is_running_) {
        return false;
    }

    frame_timer_.Tick();
    const auto delta_time = static_cast<f32>(frame_timer_.DeltaSeconds());

    EmitEvent<events::FrameBeginEvent>(delta_time);
    EmitEvent<events::AppTickEvent>(delta_time);

    window_->PollEvents();

    EmitEvent<events::AppUpdateEvent>(delta_time);
    EmitEvent<events::RenderBeginEvent>();
    EmitEvent<events::AppRenderEvent>();

    if (layers_ != nullptr && layers_->IsAttached()) {
        auto ctx = BuildContext();
        layers_->UpdateAll(ctx, static_cast<f64>(delta_time) * 1000.0);
        layers_->DrawUi(ctx);
    }

    EmitEvent<events::RenderEndEvent>();
    EmitEvent<events::SwapBuffersEvent>();
    EmitEvent<events::FrameEndEvent>();

    // NOTE: Drawing and update logic would go here

    if (window_->ShouldClose()) {
        Stop();
        return false;
    }

    return true;
}

void Application::Run() {
    while (Tick())
        ;
}

CallbackId Application::AddEventCallback(EventCallback callback) {
    const CallbackId callback_id = next_callback_id_++;
    event_callbacks_.emplace(callback_id, std::move(callback));
    return callback_id;
}

void Application::RemoveEventCallback(CallbackId id) {
    event_callbacks_.erase(id);
}

void Application::EmitEvent(events::Event& event) {
    event.timestamp = Clock::Seconds();

    if (layers_ != nullptr && layers_->IsAttached()) {
        auto ctx = BuildContext();
        layers_->DispatchEvent(ctx, event);
    }

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

} // namespace woki
