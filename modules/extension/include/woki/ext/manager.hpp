#pragma once

// IWYU pragma: private, include "woki/ext/ext.hpp"

#include <filesystem>

#include "runtime.hpp"
#include "registry.hpp"

namespace woki::ext {

class Manager final {
public:
    explicit Manager(RuntimeBackend* backend = nullptr) noexcept;
    explicit Manager(scope<RuntimeBackend> backend) noexcept;

    void SetBackend(RuntimeBackend* backend) noexcept;
    void SetBackend(scope<RuntimeBackend> backend) noexcept;
    void SetRoots(Roots roots);

    [[nodiscard]] Result<PackageLayout> Install(const std::filesystem::path& package_path);
    [[nodiscard]] Result<PackageLayout> InstallUnpacked(const std::filesystem::path& source_root);
    [[nodiscard]] Result<void> Scan();
    [[nodiscard]] Result<void> ScanSource(const std::filesystem::path& source_root);
    [[nodiscard]] Result<void> Load(std::string_view id);
    [[nodiscard]] Result<void> LoadAll();

    void Tick(f64 delta_ms);
    void DispatchEvent(u32 event_type, std::span<const u8> payload);
    [[nodiscard]] Result<void> ExecuteCommand(std::string_view command_id, std::span<const u8> payload = {});
    void Unload(std::string_view id);
    void UnloadAll();

    [[nodiscard]] const std::vector<Record>& Records() const noexcept;
    [[nodiscard]] const CommandRegistry& Commands() const noexcept;
    [[nodiscard]] Record* Find(std::string_view id) noexcept;
    [[nodiscard]] const Record* Find(std::string_view id) const noexcept;

private:
    Registry registry_;
    Runtime runtime_;
    CommandRegistry commands_;
    CommandDispatcher command_dispatcher_;
    Roots roots_;
};

} // namespace woki::ext
