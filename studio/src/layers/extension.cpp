#include "woki/events/format.hpp"

#include "extension.hpp"

namespace woki {

namespace {

[[nodiscard]] std::filesystem::path SourceExtensionsRoot() {
#ifdef WOKI_SOURCE_EXTENSIONS_DIR
    return std::filesystem::path{WOKI_SOURCE_EXTENSIONS_DIR};
#elif defined(WOKI_SOURCE_DIR)
    return std::filesystem::path{WOKI_SOURCE_DIR} / "extensions";
#else
    return std::filesystem::current_path() / "extensions";
#endif
}

[[nodiscard]] scope<ext::Manager> CreateExtensionManager() {
    return createScope<ext::Manager>(ext::wasm::Backend::Create());
}

} // namespace

void ExtensionLayer::OnAttach(Context& ctx) {
    (void)ctx;
    LoadInstalledExtensions();
}

void ExtensionLayer::OnDetach(Context& ctx) {
    (void)ctx;
    if (extensions_ != nullptr) {
        extensions_->UnloadAll();
        extensions_.reset();
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
    extensions_ = CreateExtensionManager();
    if (extensions_ == nullptr) {
        slog::Warn("Extension manager could not be created");
        return;
    }

    if (auto scanned = extensions_->Scan(); !scanned) {
        slog::Warn("Extension scan failed: {}", scanned.error().Message());
        return;
    }

    if (auto loaded = extensions_->LoadAll(); !loaded) {
        slog::Warn("Extension load failed: {}", loaded.error().Message());
    }
}

void ExtensionLayer::LoadSourceExtensions() {
    if (extensions_ == nullptr) {
        extensions_ = CreateExtensionManager();
        if (extensions_ == nullptr) {
            slog::Warn("Extension manager could not be created");
            return;
        }
    }

    auto roots = ext::DefaultRoots();
    if (!roots) {
        slog::Warn("Failed to resolve extension roots: {}", roots.error().Message());
        return;
    }

    const std::filesystem::path source_root = SourceExtensionsRoot();
    roots->extensions = source_root;

    extensions_->UnloadAll();
    extensions_->SetRoots(std::move(*roots));

    if (auto scanned = extensions_->ScanSource(source_root); !scanned) {
        slog::Warn("Source extension scan failed: {}", scanned.error().Message());
        return;
    }

    if (auto loaded = extensions_->LoadAll(); !loaded) {
        slog::Warn("Source extension load failed: {}", loaded.error().Message());
        return;
    }

    u32 active_count = 0;
    for (const ext::Record& record : extensions_->Records()) {
        if (record.state == ext::State::Active) {
            ++active_count;
            continue;
        }
        if (record.state == ext::State::Failed) {
            slog::Warn("Extension '{}' failed: {}", record.id, record.error);
        }
    }

    slog::Info("Loaded {} source extension(s) from {}", active_count, source_root.string());
}

void ExtensionLayer::ExecuteRegisteredCommands() {
    if (extensions_ == nullptr) {
        return;
    }

    const auto& commands = extensions_->Commands().Records();
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

    if (!events::ShouldForwardToExtensions(event.GetEventType())) {
        return;
    }

    const std::string payload = events::ToJson(event);
    extensions_->DispatchEvent(static_cast<u32>(event.GetEventType()), std::span<const u8>(reinterpret_cast<const u8*>(payload.data()), payload.size()));
}

} // namespace woki
