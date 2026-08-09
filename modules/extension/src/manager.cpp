#include <string>
#include <optional>
#include <algorithm>
#include <filesystem>

#include "woki/ext/manager.hpp"

namespace woki::ext {

namespace {

[[nodiscard]] Result<void> LoadOne(Runtime& runtime, Record& record) {
    if (record.state != State::PermissionChecked) {
        return Ok();
    }

    auto loaded = runtime.Load(record);
    if (!loaded) {
        return Err(loaded.error());
    }
    if (auto initialized = runtime.Initialize(record); !initialized) {
        return Err(initialized.error());
    }
    return Ok();
}

} // namespace

Manager::Manager(RuntimeBackend* backend) noexcept
    : runtime_(backend) {}

Manager::Manager(scope<RuntimeBackend> backend) noexcept
    : runtime_(std::move(backend)) {}

void Manager::SetBackend(RuntimeBackend* backend) noexcept {
    UnloadAll();
    runtime_.SetBackend(backend);
}

void Manager::SetBackend(scope<RuntimeBackend> backend) noexcept {
    UnloadAll();
    runtime_.SetBackend(std::move(backend));
}

void Manager::SetRoots(Roots roots) {
    UnloadAll();
    roots_ = std::move(roots);
    registry_.SetRoots(roots_);
}

Result<PackageLayout> Manager::Install(const std::filesystem::path& package_path) {
    if (roots_.extensions.empty() || roots_.data.empty() || roots_.cache.empty()) {
        auto defaults = DefaultRoots();
        if (!defaults) {
            return Err(defaults.error());
        }
        SetRoots(std::move(*defaults));
    }

    std::error_code error;
    if (std::filesystem::is_directory(package_path, error)) {
        return InstallUnpacked(package_path);
    }

    auto installed = InstallArchive(package_path, roots_);
    if (!installed) {
        return Err(installed.error());
    }

    auto scanned = Scan();
    if (!scanned) {
        return Err(scanned.error());
    }

    return installed;
}

Result<PackageLayout> Manager::InstallUnpacked(const std::filesystem::path& source_root) {
    if (roots_.extensions.empty() || roots_.data.empty() || roots_.cache.empty()) {
        auto defaults = DefaultRoots();
        if (!defaults) {
            return Err(defaults.error());
        }
        SetRoots(std::move(*defaults));
    }

    auto installed = InstallUnpackedPackage(source_root, roots_);
    if (!installed) {
        return Err(installed.error());
    }

    auto scanned = Scan();
    if (!scanned) {
        return Err(scanned.error());
    }

    return installed;
}

Result<void> Manager::Scan() {
    UnloadAll();
    commands_.Clear();
    auto scanned = registry_.Scan();
    if (!scanned) {
        return Err(scanned.error());
    }

    for (const Record& record : registry_.Records()) {
        if (record.state != State::Failed) {
            commands_.Register(record.id, record.manifest.commands);
        }
    }
    return Ok();
}

Result<void> Manager::ScanSource(const std::filesystem::path& source_root) {
    UnloadAll();
    commands_.Clear();
    auto scanned = registry_.ScanSource(source_root);
    if (!scanned) {
        return Err(scanned.error());
    }

    for (const Record& record : registry_.Records()) {
        if (record.state != State::Failed) {
            commands_.Register(record.id, record.manifest.commands);
        }
    }
    return Ok();
}

Result<void> Manager::Load(std::string_view id) {
    Record* record = Find(id);
    if (record == nullptr) {
        return Err(ErrorCode::FileNotFound, "Extension '" + std::string(id) + "' is not registered. Run Scan() first.");
    }
    if (record->state != State::PermissionChecked) {
        return Err(ErrorCode::ValidationInvalidState, "Extension '" + std::string(id) + "' is not loadable from its current state.");
    }

    auto loaded = runtime_.Load(*record);
    if (!loaded) {
        return Err(loaded.error());
    }
    return runtime_.Initialize(*record);
}

Result<void> Manager::LoadAll() {
    std::string failures;
    std::optional<ErrorCode> error_code;
    for (Record& record : registry_.Records()) {
        auto loaded = LoadOne(runtime_, record);
        if (!loaded) {
            if (!failures.empty()) {
                failures += "; ";
            }
            failures += record.id + ": " + std::string(loaded.error().Message());
            if (!error_code) {
                error_code = loaded.error().Code();
            }
        }
    }
    if (error_code) {
        return Err(*error_code, "One or more extensions failed to load: " + failures);
    }
    return Ok();
}

void Manager::Tick(f64 delta_ms) {
    for (Record& record : registry_.Records()) {
        runtime_.Tick(record, delta_ms);
    }
}

void Manager::DispatchEvent(u32 event_type, std::span<const u8> payload) {
    for (Record& record : registry_.Records()) {
        if (!HasPermission(record.manifest, Permission::Events)) {
            continue;
        }
        runtime_.DispatchEvent(record, event_type, payload);
    }
}

Result<void> Manager::ExecuteCommand(std::string_view command_id, std::span<const u8> payload) {
    return command_dispatcher_.Execute(commands_, runtime_, registry_.Records(), command_id, payload);
}

void Manager::Unload(std::string_view id) {
    Record* record = Find(id);
    if (record == nullptr) {
        return;
    }
    runtime_.Unload(*record);
}

void Manager::UnloadAll() {
    for (Record& record : registry_.Records()) {
        runtime_.Unload(record);
    }
}

const std::vector<Record>& Manager::Records() const noexcept {
    return registry_.Records();
}

const CommandRegistry& Manager::Commands() const noexcept {
    return commands_;
}

Record* Manager::Find(std::string_view id) noexcept {
    auto& records = registry_.Records();
    const auto it = std::ranges::find(records, id, &Record::id);
    if (it == records.end()) {
        return nullptr;
    }
    return &*it;
}

const Record* Manager::Find(std::string_view id) const noexcept {
    const auto& records = registry_.Records();
    const auto it = std::ranges::find(records, id, &Record::id);
    if (it == records.end()) {
        return nullptr;
    }
    return &*it;
}

} // namespace woki::ext
