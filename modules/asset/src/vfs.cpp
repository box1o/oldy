#include <algorithm>
#include <fstream>
#include <limits>
#include <mutex>
#include <set>
#include <system_error>

#include <woki/asset/vfs.hpp>

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
    const auto canonical = std::filesystem::canonical(root, error);
    if (error || !std::filesystem::is_directory(canonical, error) || error)
        return Err(ErrorCode::FileNotFound, "asset mount root is not an accessible directory");
    return Ok(ref<DirectoryMount>(new DirectoryMount(canonical)));
}

Result<std::filesystem::path> DirectoryMount::Resolve(const AssetPath& path) const {
    std::error_code error;
    const auto candidate = std::filesystem::weakly_canonical(root_ / std::filesystem::path(path.String()), error);
    if (error || !IsWithin(root_, candidate))
        return Err(ErrorCode::FileAccessDenied, "asset path escapes its mounted root");
    return Ok(candidate);
}

Result<VfsStat> DirectoryMount::Stat(const AssetPath& path) const {
    auto candidate = Resolve(path);
    if (!candidate)
        return Err(std::move(candidate).error());
    std::error_code error;
    const auto status = std::filesystem::status(*candidate, error);
    if (error || !std::filesystem::exists(status))
        return Err(ErrorCode::FileNotFound, "asset was not found in directory mount");
    if (std::filesystem::is_directory(status))
        return Ok(VfsStat{VfsEntryType::Directory, 0});
    if (!std::filesystem::is_regular_file(status))
        return Err(ErrorCode::FileAccessDenied, "asset is not a regular file");
    const auto size = std::filesystem::file_size(*candidate, error);
    if (error)
        return Err(ErrorCode::FileReadError, "failed to determine asset file size");
    return Ok(VfsStat{VfsEntryType::File, static_cast<u64>(size)});
}

Result<std::vector<std::byte>> DirectoryMount::Read(const AssetPath& path, const u64 offset, const std::size_t max_bytes) const {
    auto candidate = Resolve(path);
    if (!candidate)
        return Err(std::move(candidate).error());
    auto stat = Stat(path);
    if (!stat)
        return Err(std::move(stat).error());
    if (stat->type != VfsEntryType::File)
        return Err(ErrorCode::FileReadError, "cannot read an asset directory");
    if (offset > stat->size)
        return Err(ErrorCode::FileEndOfFile, "asset read offset is past end of file");
    const u64 available = stat->size - offset;
    const std::size_t size = static_cast<std::size_t>(std::min<u64>(available, max_bytes));
    if (offset > static_cast<u64>(std::numeric_limits<std::streamoff>::max()) || size > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
        return Err(ErrorCode::OutOfRange, "asset read range is not representable");
    std::ifstream stream(*candidate, std::ios::binary);
    if (!stream)
        return Err(ErrorCode::FileAccessDenied, "failed to open asset file");
    stream.seekg(static_cast<std::streamoff>(offset));
    std::vector<std::byte> bytes(size);
    if (size != 0 && !stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size)))
        return Err(ErrorCode::FileReadError, "failed to read requested asset range");
    return Ok(std::move(bytes));
}

Result<std::vector<AssetPath>> DirectoryMount::Enumerate() const {
    std::vector<AssetPath> result;
    std::error_code error;
    std::filesystem::recursive_directory_iterator iterator(root_, std::filesystem::directory_options::skip_permission_denied, error), end;
    for (; !error && iterator != end; iterator.increment(error)) {
        if (!iterator->is_regular_file(error) || error)
            continue;
        const auto canonical = std::filesystem::weakly_canonical(iterator->path(), error);
        if (error || !IsWithin(root_, canonical))
            continue;
        auto path = AssetPath::Parse(std::filesystem::relative(canonical, root_, error).generic_string());
        if (!error && path)
            result.push_back(std::move(*path));
    }
    if (error)
        return Err(ErrorCode::FileReadError, "failed to enumerate directory mount");
    std::ranges::sort(result);
    return Ok(std::move(result));
}

void MemoryMount::Put(const AssetPath& path, const std::span<const std::byte> bytes) {
    std::unique_lock lock(mutex_);
    files_.insert_or_assign(path.String(), std::vector(bytes.begin(), bytes.end()));
}

void MemoryMount::PutText(const AssetPath& path, const std::string_view text) {
    Put(path, std::as_bytes(std::span(text)));
}

void MemoryMount::Remove(const AssetPath& path) {
    std::unique_lock lock(mutex_);
    files_.erase(path.String());
}

Result<VfsStat> MemoryMount::Stat(const AssetPath& path) const {
    std::shared_lock lock(mutex_);
    const auto file = files_.find(path.String());
    if (file == files_.end())
        return Err(ErrorCode::FileNotFound, "asset was not found in memory mount");
    return Ok(VfsStat{VfsEntryType::File, static_cast<u64>(file->second.size())});
}

Result<std::vector<std::byte>> MemoryMount::Read(const AssetPath& path, const u64 offset, const std::size_t max_bytes) const {
    std::shared_lock lock(mutex_);
    const auto file = files_.find(path.String());
    if (file == files_.end())
        return Err(ErrorCode::FileNotFound, "asset was not found in memory mount");
    if (offset > file->second.size())
        return Err(ErrorCode::FileEndOfFile, "asset read offset is past end of file");
    const std::size_t begin = static_cast<std::size_t>(offset);
    const std::size_t size = std::min(max_bytes, file->second.size() - begin);
    return Ok(std::vector<std::byte>(file->second.begin() + static_cast<std::ptrdiff_t>(begin), file->second.begin() + static_cast<std::ptrdiff_t>(begin + size)));
}

Result<std::vector<AssetPath>> MemoryMount::Enumerate() const {
    std::shared_lock lock(mutex_);
    std::vector<AssetPath> result;
    result.reserve(files_.size());
    for (const auto& [path, unused] : files_)
        result.push_back(*AssetPath::Parse(path));
    return Ok(std::move(result));
}

Vfs::Vfs(Vfs&& other) noexcept {
    std::unique_lock lock(other.mutex_);
    mounts_ = std::move(other.mounts_);
    next_order_ = other.next_order_;
}

Vfs& Vfs::operator=(Vfs&& other) noexcept {
    if (this == &other)
        return *this;
    std::scoped_lock lock(mutex_, other.mutex_);
    mounts_ = std::move(other.mounts_);
    next_order_ = other.next_order_;
    return *this;
}

Result<void> Vfs::MountAt(std::string name, const AssetScheme scheme, std::string authority, const int priority, ref<Mount> mount) {
    if (name.empty() || !mount)
        return Err(ErrorCode::InvalidArgument, "VFS mount requires a name and backend");
    if (scheme != AssetScheme::Plugin && !authority.empty())
        return Err(ErrorCode::InvalidArgument, "only plugin mounts have an authority");
    std::unique_lock lock(mutex_);
    if (std::ranges::any_of(mounts_, [&](const Entry& entry) { return entry.name == name; }))
        return Err(ErrorCode::InvalidArgument, "VFS mount name is already registered");
    mounts_.push_back({std::move(name), scheme, std::move(authority), priority, next_order_++, std::move(mount)});
    return Ok();
}

void Vfs::Unmount(const std::string_view name) {
    std::unique_lock lock(mutex_);
    std::erase_if(mounts_, [&](const Entry& entry) { return entry.name == name; });
}

void Vfs::AddMount(ref<Mount> mount) {
    if (!mount)
        return;
    std::unique_lock lock(mutex_);
    const u64 order = next_order_++;
    mounts_.push_back({"legacy-engine-" + std::to_string(order), AssetScheme::Engine, {}, 0, order, std::move(mount)});
}

std::vector<Vfs::Entry> Vfs::Matching(const AssetUri& uri) const {
    std::shared_lock lock(mutex_);
    std::vector<Entry> result;
    for (const auto& entry : mounts_)
        if (entry.scheme == uri.Scheme() && entry.authority == uri.Authority())
            result.push_back(entry);
    std::ranges::sort(result, [](const Entry& left, const Entry& right) { return left.priority != right.priority ? left.priority > right.priority : left.order < right.order; });
    return result;
}

Result<ResolvedAsset> Vfs::Resolve(const AssetUri& uri) const {
    for (const auto& entry : Matching(uri)) {
        auto stat = entry.mount->Stat(uri.Path());
        if (stat)
            return Ok(ResolvedAsset{entry.name, entry.mount, uri.Path()});
        if (stat.error().Code() != ErrorCode::FileNotFound)
            return Err(std::move(stat).error());
    }
    return Err(ErrorCode::FileNotFound, "asset URI was not found in a matching mount");
}

Result<VfsStat> Vfs::Stat(const AssetUri& uri) const {
    auto resolved = Resolve(uri);
    return resolved ? resolved->mount->Stat(resolved->path) : Err(std::move(resolved).error());
}

Result<std::vector<std::byte>> Vfs::ReadBinary(const AssetUri& uri, const std::size_t max_bytes) const {
    auto stat = Stat(uri);
    if (!stat)
        return Err(std::move(stat).error());
    if (stat->size > max_bytes)
        return Err(ErrorCode::OutOfRange, "asset exceeds the configured read limit");
    return ReadBinary(uri, 0, static_cast<std::size_t>(stat->size));
}

Result<std::vector<std::byte>> Vfs::ReadBinary(const AssetUri& uri, const u64 offset, const std::size_t max_bytes) const {
    auto resolved = Resolve(uri);
    return resolved ? resolved->mount->Read(resolved->path, offset, max_bytes) : Err(std::move(resolved).error());
}

Result<std::string> Vfs::ReadText(const AssetUri& uri, const std::size_t max_bytes) const {
    auto bytes = ReadBinary(uri, max_bytes);
    if (!bytes)
        return Err(std::move(bytes).error());
    return Ok(std::string(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
}

Result<std::vector<AssetUri>> Vfs::Enumerate(const AssetScheme scheme, const std::string_view authority) const {
    std::set<AssetUri> unique;
    const std::string prefix = scheme == AssetScheme::Engine ? "engine://" : scheme == AssetScheme::Project ? "project://" : scheme == AssetScheme::Cache ? "cache://" : "plugin://" + std::string(authority) + '/';
    auto sentinel = AssetUri::Parse(prefix + "_");
    for (const auto& entry : Matching(*sentinel)) {
        auto paths = entry.mount->Enumerate();
        if (!paths)
            return Err(std::move(paths).error());
        for (const auto& path : *paths)
            unique.insert(*AssetUri::Parse(prefix + path.String()));
    }
    return Ok(std::vector<AssetUri>(unique.begin(), unique.end()));
}

Result<std::vector<std::byte>> Vfs::ReadBinary(const AssetPath& path, const std::size_t max_bytes) const {
    return ReadBinary(AssetUri::Engine(path), max_bytes);
}

Result<std::string> Vfs::ReadText(const AssetPath& path, const std::size_t max_bytes) const {
    return ReadText(AssetUri::Engine(path), max_bytes);
}

} // namespace woki::asset
