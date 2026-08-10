#include <map>
#include <set>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <filesystem>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif !defined(__EMSCRIPTEN__)
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#endif

#include "woki/ext/package.hpp"
#include <woki/logger/logger.hpp>
#include "woki/ext/path_safety.hpp"

#ifndef __EMSCRIPTEN__
#include <archive.h>
#include <archive_entry.h>
#endif

namespace woki::ext {

namespace {

namespace fs = std::filesystem;

inline constexpr std::uintmax_t kMaxSingleFileBytes = 64u * 1024u * 1024u;
inline constexpr std::uintmax_t kMaxTotalPackageBytes = 256u * 1024u * 1024u;
inline constexpr std::uintmax_t kMaxWasmBytes = 32u * 1024u * 1024u;
inline constexpr std::size_t kMaxPackageEntries = 10'000u;

[[nodiscard]] bool PathsOverlap(const fs::path& first, const fs::path& second) {
    const auto is_prefix = [](const fs::path& prefix, const fs::path& path) {
        if (std::distance(prefix.begin(), prefix.end()) > std::distance(path.begin(), path.end())) {
            return false;
        }
#ifdef _WIN32
        return std::equal(prefix.begin(), prefix.end(), path.begin(), [](const fs::path& left, const fs::path& right) {
            std::wstring first = left.native();
            std::wstring second = right.native();
            std::ranges::transform(first, first.begin(), [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
            std::ranges::transform(second, second.begin(), [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
            return first == second;
        });
#else
        return std::equal(prefix.begin(), prefix.end(), path.begin());
#endif
    };
    return is_prefix(first, second) || is_prefix(second, first);
}

[[nodiscard]] bool SamePath(const fs::path& first, const fs::path& second) {
#ifdef _WIN32
    std::wstring left = first.native();
    std::wstring right = second.native();
    const auto normalize = [](wchar_t ch) { return ch == L'/' ? L'\\' : static_cast<wchar_t>(std::towlower(ch)); };
    std::ranges::transform(left, left.begin(), normalize);
    std::ranges::transform(right, right.begin(), normalize);
    return left == right;
#else
    return first == second;
#endif
}

#ifdef _WIN32
[[nodiscard]] bool ContainsRootSymlink(const fs::path& path) {
    fs::path current;
    std::error_code error;
    for (const fs::path& component : path) {
        current /= component;
        const fs::file_status status = fs::symlink_status(current, error);
        if (!error && fs::is_symlink(status))
            return true;
        error.clear();
    }
    return false;
}
#endif

[[nodiscard]] Result<fs::path> CanonicalRoot(const fs::path& path, std::string_view name) {
    if (path.empty()) {
        return Err(ErrorCode::InvalidArgument, "Extension " + std::string(name) + " root must not be empty.");
    }
    std::error_code error;
    if (!path.is_absolute()) {
        return Err(ErrorCode::InvalidArgument, "Extension " + std::string(name) + " root must be absolute and canonical: " + path.string());
    }
    if (!SamePath(path, path.lexically_normal())) {
        return Err(ErrorCode::FileAccessDenied, "Extension " + std::string(name) + " root must be canonical and must not use a symbolic-link alias: " + path.string());
    }
    const fs::path supplied = fs::absolute(path, error);
    if (error) {
        return Err(ErrorCode::FileReadError, "Failed to resolve extension " + std::string(name) + " root: " + error.message());
    }
    const fs::path absolute = supplied.lexically_normal();
    const fs::path canonical = fs::weakly_canonical(absolute, error);
    if (error) {
        return Err(ErrorCode::FileReadError, "Failed to canonicalize extension " + std::string(name) + " root: " + error.message());
    }
    if (!SamePath(supplied, absolute)) {
        return Err(ErrorCode::FileAccessDenied, "Extension " + std::string(name) + " root must be canonical and must not use a symbolic-link alias: " + path.string());
    }
#ifdef _WIN32
    if (ContainsRootSymlink(absolute)) {
        return Err(ErrorCode::FileAccessDenied, "Extension " + std::string(name) + " root must be canonical and must not use a symbolic-link alias: " + path.string());
    }
#else
    if (!SamePath(canonical, absolute)) {
        return Err(ErrorCode::FileAccessDenied, "Extension " + std::string(name) + " root must be canonical and must not use a symbolic-link alias: " + path.string());
    }
#endif
    return Ok(canonical);
}

class PackageRootLock final {
public:
    PackageRootLock(const PackageRootLock&) = delete;
    PackageRootLock& operator=(const PackageRootLock&) = delete;

    ~PackageRootLock() {
#ifdef _WIN32
        if (handle_ != INVALID_HANDLE_VALUE) {
            OVERLAPPED overlapped{};
            UnlockFileEx(handle_, 0, MAXDWORD, MAXDWORD, &overlapped);
            CloseHandle(handle_);
        }
#elif !defined(__EMSCRIPTEN__)
        if (file_ >= 0) {
            ::flock(file_, LOCK_UN);
            ::close(file_);
        }
#endif
    }

    [[nodiscard]] static Result<std::unique_ptr<PackageRootLock>> Acquire(const fs::path& root) {
        std::error_code error;
        fs::create_directories(root, error);
        if (error) {
            return Err(ErrorCode::FileWriteError, "Failed to create extension install root before locking: " + error.message());
        }
        auto lock = std::unique_ptr<PackageRootLock>(new PackageRootLock());
        const fs::path path = root / ".woki-install.lock";
#ifdef _WIN32
        lock->handle_ = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_HIDDEN | FILE_FLAG_OPEN_REPARSE_POINT,
            nullptr);
        if (lock->handle_ == INVALID_HANDLE_VALUE) {
            return Err(ErrorCode::FileWriteError, "Failed to open extension install lock.");
        }
        FILE_ATTRIBUTE_TAG_INFO attributes{};
        if (GetFileInformationByHandleEx(lock->handle_, FileAttributeTagInfo, &attributes, sizeof(attributes)) == 0 || (attributes.FileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) != 0) {
            return Err(ErrorCode::FileAccessDenied, "Extension install lock must be a regular non-reparse file.");
        }
        OVERLAPPED overlapped{};
        if (LockFileEx(lock->handle_, LOCKFILE_EXCLUSIVE_LOCK, 0, MAXDWORD, MAXDWORD, &overlapped) == 0) {
            return Err(ErrorCode::FileWriteError, "Failed to acquire extension install lock.");
        }
#elif !defined(__EMSCRIPTEN__)
        int flags = O_RDWR | O_CREAT | O_CLOEXEC;
#ifdef O_NOFOLLOW
        flags |= O_NOFOLLOW;
#endif
        lock->file_ = ::open(path.c_str(), flags, 0600);
        if (lock->file_ < 0 || ::flock(lock->file_, LOCK_EX) != 0) {
            const int lock_error = errno;
            return Err(ErrorCode::FileWriteError, "Failed to acquire extension install lock: " + std::error_code(lock_error, std::generic_category()).message());
        }
#endif
        return Ok(std::move(lock));
    }

private:
    PackageRootLock() = default;
#ifdef _WIN32
    HANDLE handle_ = INVALID_HANDLE_VALUE;
#elif !defined(__EMSCRIPTEN__)
    int file_ = -1;
#endif
};

[[nodiscard]] fs::path UniqueSiblingPath(const fs::path& parent, std::string_view name, std::string_view suffix) {
    static std::atomic_uint64_t sequence{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return parent / ("." + std::string(name) + "." + std::to_string(stamp) + "." + std::to_string(sequence.fetch_add(1)) + "." + std::string(suffix));
}

[[nodiscard]] Result<void> RemovePath(const fs::path& path, std::string_view description) {
    std::error_code error;
    fs::remove_all(path, error);
    if (error) {
        return Err(ErrorCode::FileWriteError, "Failed to remove " + std::string(description) + " '" + path.string() + "': " + error.message());
    }
    return Ok();
}

[[nodiscard]] Result<void> CleanupStaleArtifacts(const fs::path& extensions_root, std::string_view name) {
    std::vector<fs::path> staging;
    std::vector<fs::path> backups;
    const std::string prefix = "." + std::string(name) + ".";
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator(extensions_root, fs::directory_options::none, error)) {
        if (error) {
            return Err(ErrorCode::FileReadError, "Failed to inspect stale extension install artifacts: " + error.message());
        }
        const std::string filename = entry.path().filename().string();
        if (!filename.starts_with(prefix)) {
            continue;
        }
        const bool is_staging = filename.ends_with(".installing");
        const bool is_backup = filename.ends_with(".replaced");
        if (!is_staging && !is_backup)
            continue;
        const fs::file_status status = entry.symlink_status(error);
        if (error)
            return Err(ErrorCode::FileReadError, "Failed to inspect stale extension install artifact: " + error.message());
        if (fs::is_symlink(status) || !fs::is_directory(status))
            return Err(ErrorCode::FileAccessDenied, "Stale extension install artifact must be a real directory: " + entry.path().string());
        if (is_staging) {
            staging.push_back(entry.path());
        } else if (is_backup) {
            backups.push_back(entry.path());
        }
    }
    std::ranges::sort(staging);
    std::ranges::sort(backups);
    for (const fs::path& path : staging) {
        auto removed = RemovePath(path, "stale extension staging directory");
        if (!removed) {
            return removed;
        }
    }

    const fs::path install_root = extensions_root / name;
    const bool installed = fs::exists(install_root, error);
    if (error) {
        return Err(ErrorCode::FileReadError, "Failed to inspect installed extension during stale cleanup: " + error.message());
    }
    if (!installed && !backups.empty()) {
        const fs::path restore = backups.back();
        fs::rename(restore, install_root, error);
        if (error) {
            return Err(ErrorCode::FileWriteError, "Failed to restore stale extension backup '" + restore.string() + "': " + error.message());
        }
        backups.pop_back();
    }
    for (const fs::path& path : backups) {
        auto removed = RemovePath(path, "stale extension backup");
        if (!removed) {
            return removed;
        }
    }
    return Ok();
}

[[nodiscard]] std::unexpected<Error> ErrorWithCleanup(Error primary, const fs::path& staging_root) {
    auto cleaned = RemovePath(staging_root, "extension staging directory after failure");
    if (cleaned) {
        return Err(std::move(primary));
    }
    return Err(primary.Code(), std::string(primary.Message()) + "; cleanup also failed: " + std::string(cleaned.error().Message()));
}

[[nodiscard]] Result<fs::path> CreateStagingRoot(const Roots& roots, std::string_view name) {
    std::error_code error;
    if (fs::is_symlink(fs::symlink_status(roots.extensions, error))) {
        return Err(ErrorCode::FileAccessDenied, "Extension install root must not be a symbolic link: " + roots.extensions.string());
    }
    fs::create_directories(roots.extensions, error);
    if (error) {
        return Err(ErrorCode::FileWriteError, error.message());
    }
    for (int attempt = 0; attempt < 32; ++attempt) {
        fs::path candidate = UniqueSiblingPath(roots.extensions, name, "installing");
        if (fs::create_directory(candidate, error)) {
            fs::permissions(candidate, fs::perms::owner_all, fs::perm_options::replace, error);
            if (error) {
                std::error_code cleanup_error;
                fs::remove(candidate, cleanup_error);
                return Err(ErrorCode::FileWriteError, "Failed to secure extension staging directory: " + error.message());
            }
            return Ok(std::move(candidate));
        }
        if (error && error != std::errc::file_exists) {
            return Err(ErrorCode::FileWriteError, error.message());
        }
        error.clear();
    }
    return Err(ErrorCode::FileWriteError, "Failed to allocate a unique extension staging directory.");
}

[[nodiscard]] Result<void> CommitStagedPackage(const fs::path& staging_root, const fs::path& install_root, InstallPolicy policy) {
    std::error_code error;
    const bool exists = fs::exists(install_root, error);
    if (error) {
        return Err(ErrorCode::FileReadError, error.message());
    }
    if (exists && policy == InstallPolicy::FailIfExists) {
        return Err(ErrorCode::ValidationInvalidState, "Extension '" + install_root.filename().string() + "' is already installed. Use ReplaceExisting for an explicit update.");
    }
    if (!exists) {
        fs::rename(staging_root, install_root, error);
        if (error)
            return Err(ErrorCode::FileWriteError, error.message());
        return Ok();
    }

    const fs::path backup = UniqueSiblingPath(install_root.parent_path(), install_root.filename().string(), "replaced");
    fs::rename(install_root, backup, error);
    if (error) {
        return Err(ErrorCode::FileWriteError, "Failed to preserve the installed extension before replacement: " + error.message());
    }
    fs::rename(staging_root, install_root, error);
    if (error) {
        std::error_code rollback_error;
        fs::rename(backup, install_root, rollback_error);
        if (rollback_error) {
            return Err(ErrorCode::FileWriteError, "Extension replacement failed and rollback also failed: " + error.message() + "; " + rollback_error.message());
        }
        return Err(ErrorCode::FileWriteError, "Extension replacement failed; the previous package was restored: " + error.message());
    }
    fs::remove_all(backup, error);
    if (error) {
        slog::Warn("Extension replacement committed, but backup cleanup failed for '{}': {}. A later install will retry cleanup.", backup.string(), error.message());
    }
    return Ok();
}

[[nodiscard]] Result<fs::path> Normalize(const fs::path& path) {
    std::error_code error;
    fs::path normalized = fs::weakly_canonical(path, error);
    if (error) {
        normalized = path.lexically_normal();
    }
    if (normalized.empty()) {
        return Err(ErrorCode::ValidationInvalidState, "Resolved package path is empty");
    }
    return Ok(std::move(normalized));
}

[[nodiscard]] Result<void> ValidateSourceEntry(const fs::directory_entry& entry, const fs::path& source_root, const Manifest& manifest, std::uintmax_t* total_bytes) {
    std::error_code error;
    const fs::path relative = fs::relative(entry.path(), source_root, error);
    if (error) {
        return Err(ErrorCode::FileReadError, error.message());
    }

    if (!IsAllowedArchiveEntry(relative, manifest.wasm_path)) {
        return Err(ErrorCode::ValidationInvalidState,
            "Package contains unsupported entry '" + relative.string() + "'. This build accepts only manifest.yaml, runtime.wasm, and assets/**; native payloads and signatures are unsupported.");
    }

    if (entry.is_symlink(error)) {
        return Err(ErrorCode::ValidationInvalidState, "Package entry must not be a symlink: " + relative.string());
    }
    if (entry.is_directory(error)) {
        return Ok();
    }
    if (!entry.is_regular_file(error)) {
        return Err(ErrorCode::ValidationInvalidState, "Package entry must be a regular file or directory: " + relative.string());
    }

    const auto size = entry.file_size(error);
    if (error) {
        return Err(ErrorCode::FileReadError, error.message());
    }
    if (size > kMaxSingleFileBytes) {
        return Err(ErrorCode::ValidationOutOfRange, "Package file exceeds 64 MiB limit: " + relative.string());
    }
    *total_bytes += size;
    if (*total_bytes > kMaxTotalPackageBytes) {
        return Err(ErrorCode::ValidationOutOfRange, "Package exceeds 256 MiB unpacked size limit.");
    }

    return Ok();
}

[[nodiscard]] Result<void> CopyPackageTree(const fs::path& source_root, const fs::path& destination_root, const Manifest& manifest) {
    std::uintmax_t total_bytes = 0;
    std::size_t entry_count = 0;
    std::error_code error;
    std::set<std::string> portable_entries;

    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(source_root, fs::directory_options::none, error)) {
        if (error) {
            return Err(ErrorCode::FileReadError, error.message());
        }
        if (++entry_count > kMaxPackageEntries) {
            return Err(ErrorCode::ValidationOutOfRange, "Package exceeds 10000 entry limit.");
        }

        auto valid = ValidateSourceEntry(entry, source_root, manifest, &total_bytes);
        if (!valid) {
            return Err(valid.error());
        }

        const fs::path relative = fs::relative(entry.path(), source_root, error);
        if (error) {
            return Err(ErrorCode::FileReadError, error.message());
        }
        std::string collision_key = relative.generic_string();
        std::ranges::transform(collision_key, collision_key.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (!portable_entries.insert(collision_key).second) {
            return Err(ErrorCode::ValidationInvalidState, "Package contains a case-colliding path: " + relative.generic_string());
        }

        const fs::path destination = destination_root / relative;
        if (entry.is_directory(error)) {
            fs::create_directories(destination, error);
            if (error) {
                return Err(ErrorCode::FileWriteError, error.message());
            }
            continue;
        }

        fs::create_directories(destination.parent_path(), error);
        if (error) {
            return Err(ErrorCode::FileWriteError, error.message());
        }

        fs::copy_file(entry.path(), destination, fs::copy_options::none, error);
        if (error) {
            return Err(ErrorCode::FileWriteError, error.message());
        }
    }

    return Ok();
}

[[nodiscard]] Result<void> ValidateWasmSize(const fs::path& path) {
    std::error_code error;
    const auto size = fs::file_size(path, error);
    if (error) {
        return Err(ErrorCode::FileReadError, error.message());
    }
    if (size > kMaxWasmBytes) {
        return Err(ErrorCode::ValidationOutOfRange, "Extension wasm module exceeds 32 MiB limit.");
    }
    return Ok();
}

[[nodiscard]] bool ContainsSymlink(const fs::path& root, const fs::path& path) {
    const fs::path relative = path.lexically_relative(root);
    if (!IsSafeRelativePath(relative)) {
        return true;
    }
    fs::path current = root;
    std::error_code error;
    if (fs::is_symlink(fs::symlink_status(current, error))) {
        return true;
    }
    for (const fs::path& part : relative) {
        current /= part;
        if (fs::is_symlink(fs::symlink_status(current, error))) {
            return true;
        }
    }
    return false;
}

#ifndef __EMSCRIPTEN__

[[nodiscard]] Result<fs::path> SanitizeArchiveEntryPath(std::string_view entry_path, bool directory) {
    if (entry_path.empty()) {
        return Err(ErrorCode::ValidationInvalidState, "Archive entry path is empty");
    }
    if (entry_path.contains('\0') || entry_path.contains('\\') || entry_path.contains(':') || entry_path.starts_with('/') || entry_path.contains("//")) {
        return Err(ErrorCode::ValidationInvalidState, "Archive entry is not a portable relative path: " + std::string(entry_path));
    }
    if (directory && entry_path.ends_with('/')) {
        entry_path.remove_suffix(1);
    } else if (entry_path.ends_with('/')) {
        return Err(ErrorCode::ValidationInvalidState, "Archive file entry must not end with '/': " + std::string(entry_path));
    }

    fs::path relative{entry_path};
    if (!IsSafeRelativePath(relative)) {
        return Err(ErrorCode::ValidationInvalidState, "Archive entry path is invalid: " + std::string(entry_path));
    }
    return Ok(relative.lexically_normal());
}

[[nodiscard]] std::string PortableCollisionKey(const fs::path& path) {
    std::string key = path.generic_string();
    std::ranges::transform(key, key.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return key;
}

[[nodiscard]] Result<void> RegisterArchiveEntry(std::map<std::string, bool>& entries, const fs::path& path, bool directory) {
    fs::path current;
    for (auto part = path.begin(); part != path.end(); ++part) {
        current /= *part;
        const bool final = std::next(part) == path.end();
        const std::string key = PortableCollisionKey(current);
        const auto existing = entries.find(key);
        if (existing != entries.end()) {
            if (final || !existing->second) {
                return Err(ErrorCode::ValidationInvalidState, "Archive contains a duplicate, case collision, or file/directory collision: " + path.generic_string());
            }
            continue;
        }
        entries.emplace(key, final ? directory : true);
    }
    return Ok();
}

[[nodiscard]] Result<void> ExtractArchive(const fs::path& archive_path, const fs::path& destination_root) {
    std::error_code error;
    if (!fs::is_regular_file(archive_path, error)) {
        return Err(ErrorCode::FileNotFound, "Extension archive is not a regular file: " + archive_path.string());
    }

    struct archive* reader = archive_read_new();
    if (reader == nullptr) {
        return Err(ErrorCode::InvalidState, "Failed to allocate libarchive read handle");
    }

    archive_read_support_format_zip(reader);
    archive_read_support_filter_all(reader);

    if (archive_read_open_filename(reader, archive_path.string().c_str(), 10240) != ARCHIVE_OK) {
        const std::string message = archive_error_string(reader);
        archive_read_free(reader);
        return Err(ErrorCode::ParseInvalidFormat, "Failed to open extension archive '" + archive_path.string() + "': " + message);
    }

    fs::create_directories(destination_root, error);
    if (error) {
        archive_read_close(reader);
        archive_read_free(reader);
        return Err(ErrorCode::FileWriteError, error.message());
    }

    std::map<std::string, bool> entries;
    std::uintmax_t total_bytes = 0;
    std::uintmax_t extracted_bytes = 0;
    struct archive_entry* entry = nullptr;

    while (true) {
        const int status = archive_read_next_header(reader, &entry);
        if (status == ARCHIVE_EOF) {
            break;
        }
        if (status != ARCHIVE_OK) {
            const std::string message = archive_error_string(reader);
            archive_read_close(reader);
            archive_read_free(reader);
            return Err(ErrorCode::ParseInvalidFormat, "Failed to read extension archive header: " + message);
        }

        const char* pathname = archive_entry_pathname(entry);
        if (pathname == nullptr) {
            archive_read_close(reader);
            archive_read_free(reader);
            return Err(ErrorCode::ParseInvalidFormat, "Archive entry is missing a path");
        }
        if (entries.size() >= kMaxPackageEntries) {
            archive_read_close(reader);
            archive_read_free(reader);
            return Err(ErrorCode::ValidationOutOfRange, "Archive exceeds 10000 entry limit.");
        }

        const auto file_type = archive_entry_filetype(entry);
        auto relative = SanitizeArchiveEntryPath(pathname, file_type == AE_IFDIR);
        if (!relative) {
            archive_read_close(reader);
            archive_read_free(reader);
            return Err(relative.error());
        }

        auto registered = RegisterArchiveEntry(entries, *relative, file_type == AE_IFDIR);
        if (!registered) {
            archive_read_close(reader);
            archive_read_free(reader);
            return Err(registered.error());
        }

        if (archive_entry_hardlink(entry) != nullptr || archive_entry_symlink(entry) != nullptr) {
            archive_read_close(reader);
            archive_read_free(reader);
            return Err(ErrorCode::ValidationInvalidState, "Archive entry must not be a link: " + relative->string());
        }

        const fs::path destination = destination_root / *relative;

        if (file_type == AE_IFDIR) {
            fs::create_directories(destination, error);
            if (error) {
                archive_read_close(reader);
                archive_read_free(reader);
                return Err(ErrorCode::FileWriteError, error.message());
            }
            continue;
        }

        if (file_type != AE_IFREG) {
            archive_read_close(reader);
            archive_read_free(reader);
            return Err(ErrorCode::ValidationInvalidState, "Archive entry must be a regular file or directory: " + relative->string());
        }

        {
            const la_int64_t entry_size = archive_entry_size(entry);
            if (entry_size < 0) {
                archive_read_close(reader);
                archive_read_free(reader);
                return Err(ErrorCode::ParseInvalidFormat, "Archive entry has invalid size: " + relative->string());
            }

            const auto size = static_cast<std::uintmax_t>(entry_size);
            if (size > kMaxSingleFileBytes) {
                archive_read_close(reader);
                archive_read_free(reader);
                return Err(ErrorCode::ValidationOutOfRange, "Archive file exceeds 64 MiB limit: " + relative->string());
            }

            total_bytes += size;
            if (total_bytes > kMaxTotalPackageBytes) {
                archive_read_close(reader);
                archive_read_free(reader);
                return Err(ErrorCode::ValidationOutOfRange, "Archive exceeds 256 MiB unpacked size limit.");
            }
        }

        fs::create_directories(destination.parent_path(), error);
        if (error) {
            archive_read_close(reader);
            archive_read_free(reader);
            return Err(ErrorCode::FileWriteError, error.message());
        }

        std::ofstream output(destination, std::ios::binary | std::ios::trunc);
        if (!output.good()) {
            archive_read_close(reader);
            archive_read_free(reader);
            return Err(ErrorCode::FileWriteError, "Failed to create extracted archive file: " + destination.string());
        }

        const void* buffer = nullptr;
        std::size_t buffer_size = 0;
        la_int64_t offset = 0;
        std::uintmax_t file_bytes = 0;
        while (true) {
            const int block_status = archive_read_data_block(reader, &buffer, &buffer_size, &offset);
            if (block_status == ARCHIVE_EOF) {
                break;
            }
            if (block_status != ARCHIVE_OK) {
                const std::string message = archive_error_string(reader);
                archive_read_close(reader);
                archive_read_free(reader);
                return Err(ErrorCode::FileReadError, "Failed to read archive entry '" + relative->string() + "': " + message);
            }

            if (offset < 0 || static_cast<std::uintmax_t>(offset) > kMaxSingleFileBytes || buffer_size > kMaxSingleFileBytes - static_cast<std::uintmax_t>(offset)) {
                archive_read_close(reader);
                archive_read_free(reader);
                return Err(ErrorCode::ValidationOutOfRange, "Extracted archive file exceeds 64 MiB limit: " + relative->string());
            }
            const std::uintmax_t block_end = static_cast<std::uintmax_t>(offset) + buffer_size;
            if (block_end > file_bytes) {
                const std::uintmax_t growth = block_end - file_bytes;
                if (growth > kMaxTotalPackageBytes - extracted_bytes) {
                    archive_read_close(reader);
                    archive_read_free(reader);
                    return Err(ErrorCode::ValidationOutOfRange, "Archive exceeds 256 MiB unpacked size limit.");
                }
                extracted_bytes += growth;
                file_bytes = block_end;
            }
            output.seekp(static_cast<std::streamoff>(offset));
            output.write(static_cast<const char*>(buffer), static_cast<std::streamsize>(buffer_size));
            if (!output.good()) {
                archive_read_close(reader);
                archive_read_free(reader);
                return Err(ErrorCode::FileWriteError, "Failed to write extracted archive file: " + destination.string());
            }
        }
    }

    archive_read_close(reader);
    archive_read_free(reader);
    return Ok();
}

#endif // __EMSCRIPTEN__

} // namespace

Result<Roots> ValidateRoots(const Roots& roots) {
    Roots validated;
    auto extensions = CanonicalRoot(roots.extensions, "install");
    if (!extensions)
        return Err(extensions.error());
    auto data = CanonicalRoot(roots.data, "data");
    if (!data)
        return Err(data.error());
    auto cache = CanonicalRoot(roots.cache, "cache");
    if (!cache)
        return Err(cache.error());
    const fs::path config_path = roots.config.empty() ? roots.data.parent_path() / "ext-config" : roots.config;
    auto config = CanonicalRoot(config_path, "config");
    if (!config)
        return Err(config.error());
    validated = Roots{*extensions, *data, *cache, *config};

    const std::array<std::pair<std::string_view, fs::path>, 4> named{{
        {"install", validated.extensions},
        {"data", validated.data},
        {"cache", validated.cache},
        {"config", validated.config},
    }};
    for (std::size_t first = 0; first < named.size(); ++first) {
        for (std::size_t second = first + 1; second < named.size(); ++second) {
            if (PathsOverlap(named[first].second, named[second].second)) {
                return Err(ErrorCode::InvalidArgument, "Extension roots must be distinct and non-overlapping; " + std::string(named[first].first) + " overlaps " + std::string(named[second].first) + ".");
            }
        }
    }
    return Ok(std::move(validated));
}

Result<PackageLayout> ResolvePackageLayout(const Manifest& manifest, const fs::path& extensions_root, const fs::path& data_root, const fs::path& cache_root) {
    return ResolvePackageLayout(manifest, Roots{extensions_root, data_root, cache_root, data_root.parent_path() / "ext-config"});
}

Result<PackageLayout> ResolvePackageLayout(const Manifest& manifest, const Roots& roots) {
    auto valid = ValidateManifest(manifest);
    if (!valid) {
        return Err(valid.error());
    }
    auto validated_roots = ValidateRoots(roots);
    if (!validated_roots)
        return Err(validated_roots.error());

    PackageLayout layout;

    auto install_root = Normalize(validated_roots->extensions / manifest.id);
    if (!install_root) {
        return Err(install_root.error());
    }
    layout.install_root = std::move(*install_root);

    auto data = Normalize(validated_roots->data / manifest.id);
    if (!data) {
        return Err(data.error());
    }

    auto cache = Normalize(validated_roots->cache / manifest.id);
    if (!cache) {
        return Err(cache.error());
    }

    layout.manifest = layout.install_root / "manifest.yaml";
    layout.wasm = (layout.install_root / manifest.wasm_path).lexically_normal();
    layout.data_root = std::move(*data);
    auto config = Normalize(validated_roots->config / manifest.id);
    if (!config) {
        return Err(config.error());
    }
    layout.config_root = std::move(*config);
    layout.cache_root = std::move(*cache);

    return Ok(std::move(layout));
}

Result<void> ValidatePackageLayout(const PackageLayout& layout) {
    std::error_code error;
    if (ContainsSymlink(layout.install_root, layout.manifest) || ContainsSymlink(layout.install_root, layout.wasm)) {
        return Err(ErrorCode::FileAccessDenied, "Extension package paths must not be symbolic links.");
    }
    if (!fs::is_directory(layout.install_root, error)) {
        return Err(ErrorCode::FileNotFound, "Extension install directory is missing: " + layout.install_root.string() + ". Install or extract the package before scanning.");
    }
    if (!fs::is_regular_file(layout.manifest, error)) {
        return Err(ErrorCode::FileNotFound, "Extension manifest is missing: " + layout.manifest.string()
                                                + ". Add manifest.yaml with id, name, version, apiVersion, runtime.wasm, and "
                                                  "permissions.");
    }
    if (!fs::is_regular_file(layout.wasm, error)) {
        return Err(ErrorCode::FileNotFound, "Extension wasm module is missing: " + layout.wasm.string() + ". Build the extension wasm or set runtime.wasm to the correct relative path.");
    }
    return ValidateWasmSize(layout.wasm);
}

Result<PackageLayout> InstallUnpackedPackage(const fs::path& source_root, const Roots& roots, InstallPolicy policy) {
    std::error_code error;
    if (fs::is_symlink(fs::symlink_status(source_root, error))) {
        return Err(ErrorCode::FileAccessDenied, "Unpacked extension package source must not be a symbolic link: " + source_root.string());
    }
    if (!fs::is_directory(source_root, error)) {
        return Err(ErrorCode::FileNotFound, "Unpacked extension package source is not a directory: " + source_root.string());
    }

    auto manifest = LoadManifest(source_root / "manifest.yaml");
    if (!manifest) {
        return Err(manifest.error());
    }

    auto layout = ResolvePackageLayout(*manifest, roots);
    if (!layout) {
        return Err(layout.error());
    }

    const fs::path canonical_source = fs::weakly_canonical(source_root, error);
    if (error) {
        return Err(ErrorCode::FileReadError, "Failed to canonicalize unpacked extension source: " + error.message());
    }
    if (PathsOverlap(canonical_source, layout->install_root) || PathsOverlap(canonical_source, layout->install_root.parent_path())) {
        return Err(ErrorCode::InvalidArgument, "Unpacked extension source must not overlap the extension install root.");
    }
    const fs::path supplied_source = fs::absolute(source_root, error);
    if (error || !SamePath(source_root, source_root.lexically_normal())) {
        return Err(ErrorCode::FileAccessDenied, "Unpacked extension source must be canonical and must not use a symbolic-link alias.");
    }
#ifdef _WIN32
    if (ContainsRootSymlink(supplied_source.lexically_normal())) {
        return Err(ErrorCode::FileAccessDenied, "Unpacked extension source must be canonical and must not use a symbolic-link alias.");
    }
#else
    if (!SamePath(supplied_source.lexically_normal(), canonical_source)) {
        return Err(ErrorCode::FileAccessDenied, "Unpacked extension source must be canonical and must not use a symbolic-link alias.");
    }
#endif
    const PackageLayout source_layout{
        .install_root = canonical_source,
        .manifest = canonical_source / "manifest.yaml",
        .wasm = (canonical_source / manifest->wasm_path).lexically_normal(),
        .data_root = layout->data_root,
        .config_root = layout->config_root,
        .cache_root = layout->cache_root,
    };

    auto valid_source = ValidatePackageLayout(source_layout);
    if (!valid_source) {
        return Err(valid_source.error());
    }

    auto root_lock = PackageRootLock::Acquire(layout->install_root.parent_path());
    if (!root_lock)
        return Err(root_lock.error());
    auto stale = CleanupStaleArtifacts(layout->install_root.parent_path(), manifest->id);
    if (!stale)
        return Err(stale.error());
    auto validated_roots = ValidateRoots(roots);
    if (!validated_roots)
        return Err(validated_roots.error());
    auto staging = CreateStagingRoot(*validated_roots, manifest->id);
    if (!staging) {
        return Err(staging.error());
    }
    const fs::path staging_root = std::move(*staging);

    auto copied = CopyPackageTree(source_root, staging_root, *manifest);
    if (!copied) {
        return ErrorWithCleanup(copied.error(), staging_root);
    }

    auto valid_staged = ValidatePackageLayout(PackageLayout{
        .install_root = staging_root,
        .manifest = staging_root / "manifest.yaml",
        .wasm = (staging_root / manifest->wasm_path).lexically_normal(),
        .data_root = layout->data_root,
        .config_root = layout->config_root,
        .cache_root = layout->cache_root,
    });
    if (!valid_staged) {
        return ErrorWithCleanup(valid_staged.error(), staging_root);
    }

    auto committed = CommitStagedPackage(staging_root, layout->install_root, policy);
    if (!committed) {
        return ErrorWithCleanup(committed.error(), staging_root);
    }

    return Ok(std::move(*layout));
}

Result<PackageLayout> InstallArchive(const fs::path& archive_path, const Roots& roots, InstallPolicy policy) {
#ifndef __EMSCRIPTEN__
    std::error_code error;
    auto validated_roots = ValidateRoots(roots);
    if (!validated_roots)
        return Err(validated_roots.error());
    auto root_lock = PackageRootLock::Acquire(validated_roots->extensions);
    if (!root_lock)
        return Err(root_lock.error());
    auto stale_archive = CleanupStaleArtifacts(validated_roots->extensions, "archive");
    if (!stale_archive)
        return Err(stale_archive.error());
    auto staging = CreateStagingRoot(*validated_roots, "archive");
    if (!staging) {
        return Err(staging.error());
    }
    const fs::path staging_root = std::move(*staging);

    auto extracted = ExtractArchive(archive_path, staging_root);
    if (!extracted) {
        return ErrorWithCleanup(extracted.error(), staging_root);
    }

    auto manifest = LoadManifest(staging_root / "manifest.yaml");
    if (!manifest) {
        return ErrorWithCleanup(manifest.error(), staging_root);
    }

    std::uintmax_t total_bytes = 0;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(staging_root, fs::directory_options::none, error)) {
        if (error) {
            return ErrorWithCleanup(MakeError(ErrorCode::FileReadError, error.message()), staging_root);
        }

        auto valid = ValidateSourceEntry(entry, staging_root, *manifest, &total_bytes);
        if (!valid) {
            return ErrorWithCleanup(valid.error(), staging_root);
        }
    }

    auto layout = ResolvePackageLayout(*manifest, roots);
    if (!layout) {
        return ErrorWithCleanup(layout.error(), staging_root);
    }
    auto stale = CleanupStaleArtifacts(validated_roots->extensions, manifest->id);
    if (!stale)
        return ErrorWithCleanup(stale.error(), staging_root);

    auto valid_installed_layout = ValidatePackageLayout(PackageLayout{
        .install_root = staging_root,
        .manifest = staging_root / "manifest.yaml",
        .wasm = (staging_root / manifest->wasm_path).lexically_normal(),
        .data_root = layout->data_root,
        .config_root = layout->config_root,
        .cache_root = layout->cache_root,
    });
    if (!valid_installed_layout) {
        return ErrorWithCleanup(valid_installed_layout.error(), staging_root);
    }

    auto committed = CommitStagedPackage(staging_root, layout->install_root, policy);
    if (!committed) {
        return ErrorWithCleanup(committed.error(), staging_root);
    }

    return Ok(std::move(*layout));
#else
    (void)archive_path;
    (void)roots;
    (void)policy;
    return Err(ErrorCode::InvalidState, "Extension archives are not supported on this platform.");
#endif
}

} // namespace woki::ext
