#include <woki/asset/vfs.hpp>

#include <fstream>
#include <limits>
#include <system_error>

namespace woki::asset {
namespace {

bool IsWithin(const std::filesystem::path& root, const std::filesystem::path& path) {
    auto root_part = root.begin();
    auto path_part = path.begin();
    while (root_part != root.end() && path_part != path.end() && *root_part == *path_part) {
        ++root_part;
        ++path_part;
    }
    return root_part == root.end();
}

} // namespace

Result<ref<DirectoryMount>> DirectoryMount::Create(const std::filesystem::path& root) {
    std::error_code error;
    const std::filesystem::path canonical = std::filesystem::canonical(root, error);
    if (error || !std::filesystem::is_directory(canonical, error) || error) {
        return Err(ErrorCode::FileNotFound, "asset mount root is not an accessible directory");
    }
    return Ok(ref<DirectoryMount>(new DirectoryMount(canonical)));
}

Result<std::vector<std::byte>> DirectoryMount::Read(const AssetPath& path, const std::size_t max_bytes) const {
    std::error_code error;
    const std::filesystem::path candidate = std::filesystem::weakly_canonical(root_ / std::filesystem::path(path.String()), error);
    if (error || !IsWithin(root_, candidate)) {
        return Err(ErrorCode::FileAccessDenied, "asset path escapes its mounted root");
    }
    if (!std::filesystem::is_regular_file(candidate, error) || error) {
        return Err(ErrorCode::FileNotFound, "asset was not found in directory mount");
    }

    const std::uintmax_t file_size = std::filesystem::file_size(candidate, error);
    if (error) {
        return Err(ErrorCode::FileReadError, "failed to determine asset file size");
    }
    if (file_size > max_bytes || file_size > std::numeric_limits<std::size_t>::max() || file_size > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
        return Err(ErrorCode::OutOfRange, "asset exceeds the configured read limit");
    }

    std::ifstream stream(candidate, std::ios::binary);
    if (!stream) {
        return Err(ErrorCode::FileAccessDenied, "failed to open asset file");
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(file_size));
    if (!bytes.empty() && !stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
        return Err(ErrorCode::FileReadError, "failed to read complete asset file");
    }
    return Ok(std::move(bytes));
}

const std::filesystem::path& DirectoryMount::Root() const noexcept {
    return root_;
}

void MemoryMount::Put(const AssetPath& path, const std::span<const std::byte> bytes) {
    files_.insert_or_assign(path.String(), std::vector(bytes.begin(), bytes.end()));
}

void MemoryMount::PutText(const AssetPath& path, const std::string_view text) {
    Put(path, std::as_bytes(std::span(text.data(), text.size())));
}

Result<std::vector<std::byte>> MemoryMount::Read(const AssetPath& path, const std::size_t max_bytes) const {
    const auto file = files_.find(path.String());
    if (file == files_.end()) {
        return Err(ErrorCode::FileNotFound, "asset was not found in memory mount");
    }
    if (file->second.size() > max_bytes) {
        return Err(ErrorCode::OutOfRange, "asset exceeds the configured read limit");
    }
    return Ok(file->second);
}

void Vfs::AddMount(ref<Mount> mount) {
    mounts_.push_back(std::move(mount));
}

Result<std::vector<std::byte>> Vfs::ReadBinary(const AssetPath& path, const std::size_t max_bytes) const {
    for (const ref<Mount>& mount : mounts_) {
        Result<std::vector<std::byte>> result = mount->Read(path, max_bytes);
        if (result || result.error().Code() != ErrorCode::FileNotFound) {
            return result;
        }
    }
    return Err(ErrorCode::FileNotFound, "asset was not found in any mount");
}

Result<std::string> Vfs::ReadText(const AssetPath& path, const std::size_t max_bytes) const {
    Result<std::vector<std::byte>> bytes = ReadBinary(path, max_bytes);
    if (!bytes) {
        return Err(std::move(bytes).error());
    }
    return Ok(std::string(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
}

} // namespace woki::asset
