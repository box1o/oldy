#pragma once

// IWYU pragma: private, include "woki/asset.hpp"

#include <cstddef>
#include <filesystem>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "path.hpp"

namespace woki::asset {

inline constexpr std::size_t kDefaultReadLimit = 64U * 1024U * 1024U;

class Mount {
public:
    virtual ~Mount() = default;

    [[nodiscard]] virtual Result<std::vector<std::byte>> Read(const AssetPath& path, std::size_t max_bytes) const = 0;
};

class DirectoryMount final : public Mount {
public:
    // DirectoryMount assumes a trusted root. Canonical path and symlink checks prevent
    // ordinary traversal, but this interface is not an openat-style hostile-root sandbox.
    [[nodiscard]] static Result<ref<DirectoryMount>> Create(const std::filesystem::path& root);

    [[nodiscard]] Result<std::vector<std::byte>> Read(const AssetPath& path, std::size_t max_bytes) const override;
    [[nodiscard]] const std::filesystem::path& Root() const noexcept;

private:
    explicit DirectoryMount(std::filesystem::path root)
        : root_(std::move(root)) {}

    std::filesystem::path root_;
};

class MemoryMount final : public Mount {
public:
    void Put(const AssetPath& path, std::span<const std::byte> bytes);
    void PutText(const AssetPath& path, std::string_view text);

    [[nodiscard]] Result<std::vector<std::byte>> Read(const AssetPath& path, std::size_t max_bytes) const override;

private:
    std::map<std::string, std::vector<std::byte>, std::less<>> files_;
};

class Vfs {
public:
    void AddMount(ref<Mount> mount);

    [[nodiscard]] Result<std::vector<std::byte>> ReadBinary(const AssetPath& path, std::size_t max_bytes = kDefaultReadLimit) const;
    [[nodiscard]] Result<std::string> ReadText(const AssetPath& path, std::size_t max_bytes = kDefaultReadLimit) const;

private:
    std::vector<ref<Mount>> mounts_;
};

} // namespace woki::asset
