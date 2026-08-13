#pragma once

// IWYU pragma: private, include "woki/asset.hpp"

#include <filesystem>
#include <map>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "uri.hpp"

namespace woki::asset {

inline constexpr std::size_t kDefaultReadLimit = 64U * 1024U * 1024U;

enum class VfsEntryType : u8 { File, Directory };

struct VfsStat {
    VfsEntryType type{VfsEntryType::File};
    u64 size{};
};

class Mount {
public:
    virtual ~Mount() = default;
    [[nodiscard]] virtual Result<VfsStat> Stat(const AssetPath& path) const = 0;
    [[nodiscard]] virtual Result<std::vector<std::byte>> Read(const AssetPath& path, u64 offset, std::size_t max_bytes) const = 0;
    [[nodiscard]] virtual Result<std::vector<AssetPath>> Enumerate() const = 0;
};

class DirectoryMount final : public Mount {
public:
    // The root is trusted configuration. Canonical/symlink checks contain ordinary
    // traversal, but this is not an openat-style sandbox against a hostile root owner.
    [[nodiscard]] static Result<ref<DirectoryMount>> Create(const std::filesystem::path& root);
    [[nodiscard]] Result<VfsStat> Stat(const AssetPath& path) const override;
    [[nodiscard]] Result<std::vector<std::byte>> Read(const AssetPath& path, u64 offset, std::size_t max_bytes) const override;
    [[nodiscard]] Result<std::vector<AssetPath>> Enumerate() const override;

    [[nodiscard]] const std::filesystem::path& Root() const noexcept {
        return root_;
    }

private:
    explicit DirectoryMount(std::filesystem::path root)
        : root_(std::move(root)) {}

    [[nodiscard]] Result<std::filesystem::path> Resolve(const AssetPath& path) const;
    std::filesystem::path root_;
};

class MemoryMount final : public Mount {
public:
    void Put(const AssetPath& path, std::span<const std::byte> bytes);
    void PutText(const AssetPath& path, std::string_view text);
    void Remove(const AssetPath& path);
    [[nodiscard]] Result<VfsStat> Stat(const AssetPath& path) const override;
    [[nodiscard]] Result<std::vector<std::byte>> Read(const AssetPath& path, u64 offset, std::size_t max_bytes) const override;
    [[nodiscard]] Result<std::vector<AssetPath>> Enumerate() const override;

private:
    mutable std::shared_mutex mutex_;
    std::map<std::string, std::vector<std::byte>, std::less<>> files_;
};

struct ResolvedAsset {
    std::string mount_name;
    ref<const Mount> mount;
    AssetPath path;
};

class Vfs {
public:
    Vfs() = default;
    Vfs(const Vfs&) = delete;
    Vfs& operator=(const Vfs&) = delete;
    Vfs(Vfs&& other) noexcept;
    Vfs& operator=(Vfs&& other) noexcept;
    [[nodiscard]] Result<void> MountAt(std::string name, AssetScheme scheme, std::string authority, int priority, ref<Mount> mount);
    void Unmount(std::string_view name);
    // Migration shim: relative paths resolve against engine:// in insertion order.
    void AddMount(ref<Mount> mount);

    [[nodiscard]] Result<ResolvedAsset> Resolve(const AssetUri& uri) const;
    [[nodiscard]] Result<VfsStat> Stat(const AssetUri& uri) const;
    [[nodiscard]] Result<std::vector<std::byte>> ReadBinary(const AssetUri& uri, std::size_t max_bytes = kDefaultReadLimit) const;
    [[nodiscard]] Result<std::vector<std::byte>> ReadBinary(const AssetUri& uri, u64 offset, std::size_t max_bytes) const;
    [[nodiscard]] Result<std::string> ReadText(const AssetUri& uri, std::size_t max_bytes = kDefaultReadLimit) const;
    [[nodiscard]] Result<std::vector<AssetUri>> Enumerate(AssetScheme scheme, std::string_view authority = {}) const;

    [[nodiscard]] Result<std::vector<std::byte>> ReadBinary(const AssetPath& path, std::size_t max_bytes = kDefaultReadLimit) const;
    [[nodiscard]] Result<std::string> ReadText(const AssetPath& path, std::size_t max_bytes = kDefaultReadLimit) const;

private:
    struct Entry {
        std::string name;
        AssetScheme scheme{};
        std::string authority;
        int priority{};
        u64 order{};
        ref<Mount> mount;
    };

    [[nodiscard]] std::vector<Entry> Matching(const AssetUri& uri) const;
    mutable std::shared_mutex mutex_;
    std::vector<Entry> mounts_;
    u64 next_order_{};
};

} // namespace woki::asset
