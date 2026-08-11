#include "extension.hpp"
#include "extension_event_adapter.hpp"

namespace woki {

namespace {

[[nodiscard]] std::filesystem::path SourceExtensionsRoot() {
#ifdef WOKI_SOURCE_EXTENSIONS_DIR
    std::filesystem::path root{WOKI_SOURCE_EXTENSIONS_DIR};
#elif defined(WOKI_SOURCE_DIR)
    std::filesystem::path root = std::filesystem::path{WOKI_SOURCE_DIR} / "extensions";
#else
    std::filesystem::path root = std::filesystem::current_path() / "extensions";
#endif
    root = std::filesystem::absolute(root).lexically_normal();
    std::error_code error;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(root, error);
    return error ? root : canonical;
}

} // namespace

void StudioExtensionEventBus::Bind(ext::ExtensionManager* manager) noexcept {
    manager_ = manager;
}

void StudioExtensionEventBus::Publish(const ext::host::Event& event) {
    if (manager_ == nullptr)
        return;
    if (event.topic)
        manager_->DispatchNamedEvent(*event.topic, event.payload);
    else
        manager_->DispatchEvent(event.type, event.payload);
}

void ExtensionLayer::OnAttach(Context& ctx) {
    (void)ctx;
    LoadInstalledExtensions();
}

void ExtensionLayer::OnDetach(Context& ctx) {
    (void)ctx;
    if (extensions_ != nullptr) {
        extensions_->UnloadAll();
        extensions_.reset();
        event_bus_.Bind(nullptr);
    }
}

void ExtensionLayer::OnUpdate(Context& ctx, f64 delta_ms) {
    (void)ctx;
    if (extensions_ != nullptr) {
        extensions_->Tick(delta_ms);
    }
}

void ExtensionLayer::OnEvent(Context& ctx, events::Event& event) {
    (void)ctx;
    if (event.GetEventType() == events::EventType::kKeyPressed) {
        const auto& key_event = static_cast<const events::KeyPressedEvent&>(event);
        if (key_event.key == events::KeyCode::kE && key_event.repeat_count == 0) {
            LoadSourceExtensions();
        } else if (key_event.key == events::KeyCode::kF9 && key_event.repeat_count == 0) {
            ExecuteRegisteredCommands();
        }
    }

    DispatchEventToExtensions(event);
}

void ExtensionLayer::LoadInstalledExtensions() {
    ext::HostOptions host_options{.event_bus = &event_bus_};
#if defined(__EMSCRIPTEN__) && defined(WOKI_SOURCE_EXTENSIONS_DIR)
    // This build-time directory contains only Studio's bundled first-party packages.
    host_options.allow_trusted_synchronous_web = true;
#endif
    extensions_ = ext::CreateExtensionManager(host_options);
    event_bus_.Bind(extensions_.get());
    if (extensions_ == nullptr) {
        slog::Warn("Extension manager could not be created");
        return;
    }

#ifdef WOKI_SOURCE_EXTENSIONS_DIR
    auto roots = ext::DefaultRoots();
    if (!roots) {
        slog::Warn("Failed to resolve extension roots: {}", roots.error().Message());
        return;
    }
    const std::filesystem::path bundled_root = SourceExtensionsRoot();
    roots->extensions = bundled_root;
    extensions_->SetRoots(std::move(*roots));
    auto scanned = extensions_->ScanSource(bundled_root);
#else
    auto scanned = extensions_->Scan();
#endif
    if (!scanned) {
        slog::Warn("Extension scan failed: {}", scanned.error().Message());
        return;
    }

    for (const ext::DiscoveryFailure& failure : extensions_->Failures()) {
        slog::Warn("Installed extension '{}' at '{}' failed discovery: {}", failure.CandidateId(), failure.PackageRoot().string(), failure.Cause().Message());
    }

    if (auto loaded = extensions_->ActivateStartup(); !loaded) {
        slog::Warn("Extension startup activation failed: {}", loaded.error().Message());
    }
    for (const ext::ExtensionStatus& status : extensions_->Statuses()) {
        if (status.state == ext::ExtensionState::Failed)
            slog::Warn("Installed extension '{}' failed: {}", status.extension_id, status.error);
    }
}

void ExtensionLayer::LoadSourceExtensions() {
    auto roots = ext::DefaultRoots();
    if (!roots) {
        slog::Warn("Failed to resolve extension roots: {}", roots.error().Message());
        return;
    }

    const std::filesystem::path source_root = SourceExtensionsRoot();
    roots->extensions = source_root;

    auto candidate = ext::CreateExtensionManager({.event_bus = &event_bus_});
    if (candidate == nullptr) {
        slog::Warn("Extension manager could not be created");
        return;
    }
    event_bus_.Bind(candidate.get());
    candidate->SetRoots(std::move(*roots));

    if (auto scanned = candidate->ScanSource(source_root); !scanned) {
        candidate->UnloadAll();
        event_bus_.Bind(extensions_.get());
        slog::Warn("Source extension scan failed: {}", scanned.error().Message());
        return;
    }

    for (const ext::DiscoveryFailure& failure : candidate->Failures()) {
        slog::Warn("Source extension '{}' at '{}' failed discovery: {}", failure.CandidateId(), failure.PackageRoot().string(), failure.Cause().Message());
    }

    if (auto loaded = candidate->ActivateStartup(); !loaded) {
        for (const ext::ExtensionStatus& status : candidate->Statuses()) {
            if (status.state == ext::ExtensionState::Failed)
                slog::Warn("Source extension '{}' failed: {}", status.extension_id, status.error);
        }
        candidate->UnloadAll();
        event_bus_.Bind(extensions_.get());
        slog::Warn("Source extension startup activation failed: {}", loaded.error().Message());
        return;
    }

    for (const ext::ExtensionStatus& status : candidate->Statuses()) {
        if (status.state == ext::ExtensionState::Failed) {
            slog::Warn("Extension '{}' failed: {}", status.extension_id, status.error);
        }
    }

    const std::size_t package_count = candidate->Packages().size();
    event_bus_.Bind(extensions_.get());
    if (extensions_ != nullptr)
        extensions_->UnloadAll();
    event_bus_.Bind(candidate.get());
    extensions_ = std::move(candidate);
    event_bus_.Bind(extensions_.get());
    slog::Info("Loaded {} source extension(s) from {}", package_count, source_root.string());
}

void ExtensionLayer::ExecuteRegisteredCommands() {
    if (extensions_ == nullptr) {
        return;
    }

    const auto commands = extensions_->Commands();
    if (commands.empty()) {
        slog::Info("No extension commands registered");
        return;
    }

    for (const ext::CommandRecord& record : commands) {
        if (auto executed = extensions_->ExecuteCommand(record.command.id); !executed) {
            slog::Warn("Command '{}' failed: {}", record.command.id, executed.error().Message());
            continue;
        }
        slog::Info("Executed extension command: {}", record.command.id);
    }
}

void ExtensionLayer::DispatchEventToExtensions(const events::Event& event) {
    if (extensions_ == nullptr) {
        return;
    }

    const auto encoded = EncodeExtensionEvent(event);
    if (!encoded) {
        return;
    }

    extensions_->DispatchEvent(static_cast<u32>(encoded->type), encoded->Payload());
}

} // namespace woki
