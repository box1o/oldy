#include <utility>
#include <optional>

#include "woki/ext/limits.hpp"
#include "woki/ext/manager.hpp"
#include "woki/ext/runtime.hpp"
#include "woki/ext/registry.hpp"
#include "woki/ext/wasm/web_engine.hpp"
#include "woki/ext/internal/command_index.hpp"
#include "woki/ext/internal/event_service.hpp"

namespace woki::ext {

struct ExtensionManager::Impl {
    explicit Impl(scope<RuntimeEngine> engine)
        : event_service(std::make_shared<host::EventService>()),
          runtime(std::move(engine)),
          capability_policy(createScope<PermissiveCapabilityPolicy>()) {
        runtime.SetEventService(event_service);
    }

    void DrainEmittedEvents() {
        event_service->Drain();
    }

    std::shared_ptr<host::EventService> event_service;
    Registry registry;
    Runtime runtime;
    scope<CapabilityPolicy> capability_policy;
    CommandIndex commands;
    Roots roots;
};

ExtensionManager::ExtensionManager(scope<RuntimeEngine> engine) noexcept
    : impl_(createScope<Impl>(std::move(engine))) {}

ExtensionManager::~ExtensionManager() {
    impl_->runtime.UnloadAll();
    try {
        impl_->DrainEmittedEvents();
    } catch (...) {
        // Destruction cannot report publisher failures.
    }
}

scope<ExtensionManager> CreateExtensionManager(HostOptions options) {
    auto manager = scope<ExtensionManager>(new ExtensionManager(wasm::CreateEngine(options.allow_trusted_synchronous_web)));
    manager->SetEventBus(options.event_bus);
    return manager;
}

void ExtensionManager::SetCapabilityPolicy(scope<CapabilityPolicy> policy) {
    impl_->runtime.UnloadAll();
    impl_->DrainEmittedEvents();
    impl_->capability_policy = policy == nullptr ? createScope<PermissiveCapabilityPolicy>() : std::move(policy);
}

void ExtensionManager::SetEventBus(host::EventBus* bus) noexcept {
    impl_->event_service->SetBus(bus);
}

void ExtensionManager::SetRoots(Roots roots) {
    impl_->runtime.UnloadAll();
    impl_->DrainEmittedEvents();
    if (roots.config.empty() && !roots.data.empty())
        roots.config = roots.data.parent_path() / "ext-config";
    impl_->roots = std::move(roots);
    impl_->registry.Clear();
    impl_->commands.Clear();
}

Result<PackageLayout> ExtensionManager::Install(const std::filesystem::path& package_path) {
    if (impl_->roots.extensions.empty() || impl_->roots.data.empty() || impl_->roots.cache.empty() || impl_->roots.config.empty()) {
        auto defaults = DefaultRoots();
        if (!defaults)
            return Err(defaults.error());
        Roots completed = impl_->roots;
        if (completed.extensions.empty())
            completed.extensions = defaults->extensions;
        if (completed.data.empty())
            completed.data = defaults->data;
        if (completed.cache.empty())
            completed.cache = defaults->cache;
        if (completed.config.empty())
            completed.config = defaults->config;
        SetRoots(std::move(completed));
    }
    std::error_code error;
    const bool is_directory = std::filesystem::is_directory(package_path, error);
    if (error == std::errc::no_such_file_or_directory)
        return Err(ErrorCode::FileNotFound, "Extension package does not exist: " + package_path.string());
    if (error)
        return Err(ErrorCode::FileReadError, "Failed to inspect extension package '" + package_path.string() + "': " + error.message());
    if (is_directory)
        return InstallUnpacked(package_path);
    return InstallArchive(package_path, impl_->roots);
}

Result<PackageLayout> ExtensionManager::InstallUnpacked(const std::filesystem::path& source_root) {
    if (impl_->roots.extensions.empty() || impl_->roots.data.empty() || impl_->roots.cache.empty() || impl_->roots.config.empty()) {
        auto defaults = DefaultRoots();
        if (!defaults)
            return Err(defaults.error());
        Roots completed = impl_->roots;
        if (completed.extensions.empty())
            completed.extensions = defaults->extensions;
        if (completed.data.empty())
            completed.data = defaults->data;
        if (completed.cache.empty())
            completed.cache = defaults->cache;
        if (completed.config.empty())
            completed.config = defaults->config;
        SetRoots(std::move(completed));
    }
    return InstallUnpackedPackage(source_root, impl_->roots);
}

Result<void> ExtensionManager::Scan() {
    Roots next_roots = impl_->roots;
    if (next_roots.extensions.empty() || next_roots.data.empty() || next_roots.cache.empty() || next_roots.config.empty()) {
        auto defaults = DefaultRoots();
        if (!defaults)
            return Err(defaults.error());
        if (next_roots.extensions.empty())
            next_roots.extensions = defaults->extensions;
        if (next_roots.data.empty())
            next_roots.data = defaults->data;
        if (next_roots.cache.empty())
            next_roots.cache = defaults->cache;
        if (next_roots.config.empty())
            next_roots.config = defaults->config;
    }
    Registry next_registry;
    if (auto scanned = next_registry.Scan(next_roots); !scanned)
        return Err(scanned.error());
    CommandIndex next_commands;
    for (const ExtensionPackage& package : next_registry.Packages()) {
        if (auto indexed = next_commands.Add(package.Id(), package.GetManifest().commands); !indexed)
            return Err(indexed.error());
    }
    impl_->runtime.UnloadAll();
    impl_->DrainEmittedEvents();
    impl_->roots = std::move(next_roots);
    impl_->registry = std::move(next_registry);
    impl_->commands = std::move(next_commands);
    return Ok();
}

Result<void> ExtensionManager::ScanSource(const std::filesystem::path& source_root) {
    Roots next_roots = impl_->roots;
    if (next_roots.extensions.empty() || next_roots.data.empty() || next_roots.cache.empty() || next_roots.config.empty()) {
        auto defaults = DefaultRoots();
        if (!defaults)
            return Err(defaults.error());
        if (next_roots.extensions.empty())
            next_roots.extensions = defaults->extensions;
        if (next_roots.data.empty())
            next_roots.data = defaults->data;
        if (next_roots.cache.empty())
            next_roots.cache = defaults->cache;
        if (next_roots.config.empty())
            next_roots.config = defaults->config;
    }
    auto validated_roots = ValidateRoots(next_roots);
    if (!validated_roots)
        return Err(validated_roots.error());
    next_roots = std::move(*validated_roots);
    Registry next_registry;
    if (auto scanned = next_registry.ScanSource(source_root, next_roots); !scanned)
        return Err(scanned.error());
    CommandIndex next_commands;
    for (const ExtensionPackage& package : next_registry.Packages()) {
        if (auto indexed = next_commands.Add(package.Id(), package.GetManifest().commands); !indexed)
            return Err(indexed.error());
    }
    impl_->runtime.UnloadAll();
    impl_->DrainEmittedEvents();
    impl_->roots = std::move(next_roots);
    impl_->registry = std::move(next_registry);
    impl_->commands = std::move(next_commands);
    return Ok();
}

Result<void> ExtensionManager::Load(std::string_view id) {
    const ExtensionPackage* package = Find(id);
    if (package == nullptr)
        return Err(ErrorCode::FileNotFound, "Extension '" + std::string(id) + "' is not registered. Run Scan() first.");
    auto grants = impl_->capability_policy->Grant(package->GetManifest());
    if (!grants) {
        impl_->runtime.RecordFailure(package->Id(), grants.error());
        return Err(grants.error());
    }
    auto loaded = impl_->runtime.Load(*package, std::move(*grants));
    impl_->DrainEmittedEvents();
    return loaded;
}

Result<void> ExtensionManager::LoadAll() {
    std::string failures;
    std::optional<ErrorCode> code;
    for (const ExtensionPackage& package : impl_->registry.Packages()) {
        if (impl_->runtime.IsActive(package.Id()))
            continue;
        auto grants = impl_->capability_policy->Grant(package.GetManifest());
        if (!grants)
            impl_->runtime.RecordFailure(package.Id(), grants.error());
        auto loaded = grants ? impl_->runtime.Load(package, std::move(*grants)) : Result<void>(Err(grants.error()));
        if (loaded)
            continue;
        if (!failures.empty())
            failures += "; ";
        failures += package.Id() + ": " + std::string(loaded.error().Message());
        if (!code)
            code = loaded.error().Code();
    }
    impl_->DrainEmittedEvents();
    return code ? Err(*code, "One or more extensions failed to load: " + failures) : Ok();
}

Result<void> ExtensionManager::ActivateStartup() {
    std::string failures;
    std::optional<ErrorCode> code;
    for (const ExtensionPackage& package : impl_->registry.Packages()) {
        if (!package.GetManifest().activation.startup || impl_->runtime.IsActive(package.Id()))
            continue;
        auto loaded = Load(package.Id());
        if (loaded)
            continue;
        if (!failures.empty())
            failures += "; ";
        failures += package.Id() + ": " + std::string(loaded.error().Message());
        if (!code)
            code = loaded.error().Code();
    }
    return code ? Err(*code, "One or more startup extensions failed to activate: " + failures) : Ok();
}

void ExtensionManager::Tick(f64 delta_ms) {
    for (const ExtensionPackage& package : impl_->registry.Packages()) {
        if (package.GetManifest().activation.tick && impl_->runtime.IsActive(package.Id()))
            impl_->runtime.Tick(package.Id(), delta_ms);
    }
    impl_->DrainEmittedEvents();
}

void ExtensionManager::DispatchEvent(u32 event_type, std::span<const u8> payload) {
    if (payload.size() > limits::kMaxEventBytes)
        return;
    std::optional<ApplicationEventType> known;
    for (const ApplicationEventType event : AllApplicationEventTypes()) {
        if (static_cast<u32>(event) == event_type) {
            known = event;
            break;
        }
    }
    if (!known && (event_type & host::kExtensionEventNamespace) == 0)
        return;
    for (const ExtensionPackage& package : impl_->registry.Packages()) {
        if (known) {
            if (!ActivatesOn(package.GetManifest(), *known))
                continue;
            if (!impl_->runtime.IsActive(package.Id()) && !Load(package.Id()))
                continue;
        } else if (!impl_->runtime.IsActive(package.Id()))
            continue;
        if (impl_->runtime.HasGrant(package.Id(), Permission::Events) && impl_->runtime.IsSubscribed(package.Id(), event_type))
            impl_->runtime.DispatchEvent(package.Id(), event_type, payload);
    }
    impl_->DrainEmittedEvents();
}

void ExtensionManager::DispatchNamedEvent(std::string_view topic, std::span<const u8> payload) {
    if (!host::IsValidEventTopic(topic) || payload.size() > limits::kMaxEventBytes)
        return;
    for (const ExtensionPackage& package : impl_->registry.Packages()) {
        if (impl_->runtime.IsActive(package.Id()) && impl_->runtime.HasGrant(package.Id(), Permission::Events) && impl_->runtime.IsSubscribed(package.Id(), topic))
            impl_->runtime.DispatchNamedEvent(package.Id(), topic, payload);
    }
    impl_->DrainEmittedEvents();
}

Result<void> ExtensionManager::ExecuteCommand(std::string_view command_id, std::span<const u8> payload) {
    const CommandRecord* command = impl_->commands.Find(command_id);
    if (command == nullptr)
        return Err(ErrorCode::FileNotFound, "Extension command '" + std::string(command_id) + "' is not registered.");
    if (!impl_->runtime.IsActive(command->extension_id)) {
        if (auto loaded = Load(command->extension_id); !loaded)
            return Err(loaded.error());
    }
    auto dispatched = impl_->runtime.DispatchCommand(command->extension_id, command_id, payload);
    impl_->DrainEmittedEvents();
    return dispatched;
}

void ExtensionManager::Unload(std::string_view id) {
    impl_->runtime.Unload(id);
    impl_->DrainEmittedEvents();
}

void ExtensionManager::UnloadAll() {
    impl_->runtime.UnloadAll();
    impl_->DrainEmittedEvents();
}

std::span<const ExtensionPackage> ExtensionManager::Packages() const noexcept {
    return impl_->registry.Packages();
}

std::span<const DiscoveryFailure> ExtensionManager::Failures() const noexcept {
    return impl_->registry.Failures();
}

std::span<const ExtensionStatus> ExtensionManager::Statuses() const noexcept {
    return impl_->runtime.Statuses();
}

std::span<const CommandRecord> ExtensionManager::Commands() const noexcept {
    return impl_->commands.Records();
}

const ExtensionPackage* ExtensionManager::Find(std::string_view id) const noexcept {
    return impl_->registry.Find(id);
}

} // namespace woki::ext
