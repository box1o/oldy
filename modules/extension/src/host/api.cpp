#include <array>
#include <atomic>
#include <chrono>
#include <string>
#include <fstream>
#include <utility>
#include <iterator>
#include <algorithm>
#include <filesystem>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#endif

#include "woki/ext/perm.hpp"
#include "woki/ext/limits.hpp"
#include "woki/ext/host/api.hpp"
#include <woki/logger/logger.hpp>
#include "woki/ext/path_safety.hpp"
#include "woki/ext/internal/event_service.hpp"

namespace woki::ext::host {

namespace {

namespace fs = std::filesystem;

[[nodiscard]] bool IsSafeConfigKey(std::string_view key) {
    if (key.empty() || key == "." || key == ".." || key.size() > limits::kMaxConfigKeyBytes) {
        return false;
    }
    return std::ranges::all_of(key, [](char ch) { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.'; });
}

[[nodiscard]] Result<void> RejectSymlinks(const fs::path& root, const fs::path& relative_path) {
    fs::path current = root;
    for (auto part = relative_path.begin();;) {
        std::error_code error;
        const fs::file_status status = fs::symlink_status(current, error);
        if (!error && fs::is_symlink(status)) {
            return Err(ErrorCode::FileAccessDenied, "Extension storage path must not traverse symbolic links.");
        }
        if (error && error != std::errc::no_such_file_or_directory) {
            return Err(ErrorCode::FileReadError, error.message());
        }
        if (part == relative_path.end()) {
            break;
        }
        current /= *part++;
    }
    return Ok();
}

#ifdef _WIN32

class WindowsFile final {
public:
    explicit WindowsFile(HANDLE value = INVALID_HANDLE_VALUE) noexcept
        : value_(value) {}

    WindowsFile(const WindowsFile&) = delete;
    WindowsFile& operator=(const WindowsFile&) = delete;

    WindowsFile(WindowsFile&& other) noexcept
        : value_(std::exchange(other.value_, INVALID_HANDLE_VALUE)) {}

    WindowsFile& operator=(WindowsFile&& other) noexcept {
        if (this != &other) {
            if (value_ != INVALID_HANDLE_VALUE)
                CloseHandle(value_);
            value_ = std::exchange(other.value_, INVALID_HANDLE_VALUE);
        }
        return *this;
    }

    ~WindowsFile() {
        if (value_ != INVALID_HANDLE_VALUE)
            CloseHandle(value_);
    }

    [[nodiscard]] HANDLE Get() const noexcept {
        return value_;
    }

private:
    HANDLE value_;
};

class WindowsDirectoryGuard final {
public:
    void Add(WindowsFile handle) {
        handles_.push_back(std::move(handle));
    }

private:
    std::vector<WindowsFile> handles_;
};

[[nodiscard]] Result<WindowsDirectoryGuard> OpenWindowsDirectoryPath(const fs::path& path, bool create) {
    if (path.empty())
        return Err(ErrorCode::InvalidArgument, "Extension storage root must not be empty.");
    std::error_code path_error;
    const fs::path absolute = fs::absolute(path, path_error).lexically_normal();
    if (path_error)
        return Err(ErrorCode::FileReadError, path_error.message());

    WindowsDirectoryGuard guard;
    fs::path current = absolute.root_path();
    for (const fs::path& component : absolute.relative_path()) {
        current /= component;
        if (create && CreateDirectoryW(current.c_str(), nullptr) == 0 && GetLastError() != ERROR_ALREADY_EXISTS)
            return Err(ErrorCode::FileWriteError, "Failed to create extension storage directory component.");
        HANDLE handle = CreateFileW(current.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            const DWORD error = GetLastError();
            return Err(error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? ErrorCode::FileNotFound : ErrorCode::FileReadError, "Failed to securely open extension storage directory component.");
        }
        FILE_ATTRIBUTE_TAG_INFO attributes{};
        if (GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &attributes, sizeof(attributes)) == 0 || (attributes.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0
            || (attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            CloseHandle(handle);
            return Err(ErrorCode::FileAccessDenied, "Extension storage path must not traverse reparse points.");
        }
        guard.Add(WindowsFile(handle));
    }
    return Ok(std::move(guard));
}

[[nodiscard]] Result<void> EnsureDirectory(const fs::path& path) {
    auto directory = OpenWindowsDirectoryPath(path, true);
    return directory ? Ok() : Err(directory.error());
}

[[nodiscard]] Result<WindowsDirectoryGuard> OpenWindowsParent(const fs::path& path, bool create) {
    return OpenWindowsDirectoryPath(path.parent_path(), create);
}

[[nodiscard]] Result<WindowsFile> OpenWindowsFile(const fs::path& path, DWORD access, DWORD creation) {
    HANDLE handle = CreateFileW(path.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, creation, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD open_error = GetLastError();
        const ErrorCode code = open_error == ERROR_FILE_NOT_FOUND || open_error == ERROR_PATH_NOT_FOUND ? ErrorCode::FileNotFound : ErrorCode::FileReadError;
        return Err(code, "Failed to securely open extension file.");
    }
    FILE_ATTRIBUTE_TAG_INFO attributes{};
    if (GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &attributes, sizeof(attributes)) == 0 || (attributes.FileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) != 0) {
        CloseHandle(handle);
        return Err(ErrorCode::FileAccessDenied, "Extension storage entry must be a regular non-reparse file.");
    }
    return Ok(WindowsFile(handle));
}

[[nodiscard]] Result<void> LockWindowsFile(HANDLE handle, bool exclusive) {
    OVERLAPPED overlapped{};
    const DWORD flags = exclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0;
    if (LockFileEx(handle, flags, 0, MAXDWORD, MAXDWORD, &overlapped) == 0)
        return Err(ErrorCode::FileAccessDenied, "Failed to lock extension storage file.");
    return Ok();
}

[[nodiscard]] Result<std::vector<u8>> ReadLockedFile(const fs::path& path, std::size_t limit, std::string_view limit_message) {
    auto parent = OpenWindowsParent(path, false);
    if (!parent)
        return Err(parent.error());
    auto file = OpenWindowsFile(path, GENERIC_READ, OPEN_EXISTING);
    if (!file)
        return Err(file.error());
    auto locked = LockWindowsFile(file->Get(), false);
    if (!locked)
        return Err(locked.error());
    LARGE_INTEGER size{};
    if (GetFileSizeEx(file->Get(), &size) == 0)
        return Err(ErrorCode::FileReadError, "Failed to size extension file.");
    if (size.QuadPart < 0 || static_cast<unsigned long long>(size.QuadPart) > limit)
        return Err(ErrorCode::ValidationOutOfRange, limit_message);
    std::vector<u8> data;
    data.reserve(static_cast<std::size_t>(size.QuadPart));
    std::array<u8, 16 * 1024> buffer{};
    while (true) {
        DWORD count = 0;
        if (ReadFile(file->Get(), buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr) == 0)
            return Err(ErrorCode::FileReadError, "Failed to read extension file.");
        if (count == 0)
            break;
        if (data.size() > limit || count > limit - data.size())
            return Err(ErrorCode::ValidationOutOfRange, limit_message);
        data.insert(data.end(), buffer.begin(), buffer.begin() + count);
    }
    return Ok(std::move(data));
}

[[nodiscard]] Result<void> WriteLockedFile(const fs::path& path, std::span<const u8> data, bool append) {
    auto parent = OpenWindowsParent(path, true);
    if (!parent)
        return Err(parent.error());
    auto file = OpenWindowsFile(path, GENERIC_READ | GENERIC_WRITE, OPEN_ALWAYS);
    if (!file)
        return Err(file.error());
    auto locked = LockWindowsFile(file->Get(), true);
    if (!locked)
        return Err(locked.error());
    LARGE_INTEGER size{};
    if (GetFileSizeEx(file->Get(), &size) == 0)
        return Err(ErrorCode::FileReadError, "Failed to size extension file.");
    if (size.QuadPart < 0 || (append && (static_cast<unsigned long long>(size.QuadPart) > limits::kMaxFileBytes || data.size() > limits::kMaxFileBytes - static_cast<unsigned long long>(size.QuadPart))))
        return Err(ErrorCode::ValidationOutOfRange, "Extension file append would exceed 16 MiB limit.");
    LARGE_INTEGER position{};
    position.QuadPart = append ? size.QuadPart : 0;
    if (SetFilePointerEx(file->Get(), position, nullptr, FILE_BEGIN) == 0 || (!append && SetEndOfFile(file->Get()) == 0))
        return Err(ErrorCode::FileWriteError, "Failed to position extension data file.");
    std::size_t offset = 0;
    while (offset < data.size()) {
        DWORD written = 0;
        const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(data.size() - offset, MAXDWORD));
        if (WriteFile(file->Get(), data.data() + offset, chunk, &written, nullptr) == 0 || written == 0)
            return Err(ErrorCode::FileWriteError, "Failed to write extension data file.");
        offset += written;
    }
    return Ok();
}

#else

class FileDescriptor final {
public:
    explicit FileDescriptor(int value = -1) noexcept
        : value_(value) {}

    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    FileDescriptor(FileDescriptor&& other) noexcept
        : value_(std::exchange(other.value_, -1)) {}

    FileDescriptor& operator=(FileDescriptor&& other) noexcept {
        if (this != &other) {
            if (value_ >= 0)
                ::close(value_);
            value_ = std::exchange(other.value_, -1);
        }
        return *this;
    }

    ~FileDescriptor() {
        if (value_ >= 0)
            ::close(value_);
    }

    [[nodiscard]] int Get() const noexcept {
        return value_;
    }

private:
    int value_;
};

[[nodiscard]] std::string SystemMessage(int error) {
    return std::error_code(error, std::generic_category()).message();
}

[[nodiscard]] Result<FileDescriptor> OpenDirectoryPath(const fs::path& path, bool create) {
    if (path.empty())
        return Err(ErrorCode::InvalidArgument, "Extension storage root must not be empty.");
    std::error_code path_error;
    const fs::path absolute = fs::absolute(path, path_error).lexically_normal();
    if (path_error)
        return Err(ErrorCode::FileReadError, path_error.message());

    int directory_flags = O_RDONLY | O_DIRECTORY | O_CLOEXEC;
#ifdef O_NOFOLLOW
    directory_flags |= O_NOFOLLOW;
#endif
    FileDescriptor current(::open("/", directory_flags));
    if (current.Get() < 0)
        return Err(ErrorCode::FileReadError, "Failed to open filesystem root: " + SystemMessage(errno));
    for (const fs::path& component : absolute.relative_path()) {
        const std::string name = component.string();
        int next = ::openat(current.Get(), name.c_str(), directory_flags);
        if (next < 0 && errno == ENOENT && create) {
            if (::mkdirat(current.Get(), name.c_str(), 0700) != 0 && errno != EEXIST)
                return Err(ErrorCode::FileWriteError, "Failed to create extension storage directory: " + SystemMessage(errno));
            next = ::openat(current.Get(), name.c_str(), directory_flags);
        }
        if (next < 0) {
            const ErrorCode code = errno == ELOOP || errno == ENOTDIR ? ErrorCode::FileAccessDenied : (errno == ENOENT ? ErrorCode::FileNotFound : ErrorCode::FileReadError);
            return Err(code, "Failed to securely open extension storage directory: " + SystemMessage(errno));
        }
        current = FileDescriptor(next);
    }
    return Ok(std::move(current));
}

[[nodiscard]] Result<void> EnsureSecureDirectory(const fs::path& path) {
    auto directory = OpenDirectoryPath(path, true);
    if (!directory)
        return Err(directory.error());
    return Ok();
}

struct RelativeParent {
    FileDescriptor directory;
    std::string filename;
};

[[nodiscard]] Result<RelativeParent> OpenRelativeParent(const fs::path& root, const fs::path& relative_path, bool create) {
    auto current = OpenDirectoryPath(root, create);
    if (!current)
        return Err(current.error());
    int directory_flags = O_RDONLY | O_DIRECTORY | O_CLOEXEC;
#ifdef O_NOFOLLOW
    directory_flags |= O_NOFOLLOW;
#endif
    for (const fs::path& component : relative_path.parent_path()) {
        const std::string name = component.string();
        int next = ::openat(current->Get(), name.c_str(), directory_flags);
        if (next < 0 && errno == ENOENT && create) {
            if (::mkdirat(current->Get(), name.c_str(), 0700) != 0 && errno != EEXIST)
                return Err(ErrorCode::FileWriteError, "Failed to create extension storage parent: " + SystemMessage(errno));
            next = ::openat(current->Get(), name.c_str(), directory_flags);
        }
        if (next < 0) {
            const ErrorCode code = errno == ELOOP || errno == ENOTDIR ? ErrorCode::FileAccessDenied : (errno == ENOENT ? ErrorCode::FileNotFound : ErrorCode::FileReadError);
            return Err(code, "Failed to securely open extension storage parent: " + SystemMessage(errno));
        }
        *current = FileDescriptor(next);
    }
    return Ok(RelativeParent{std::move(*current), relative_path.filename().string()});
}

[[nodiscard]] Result<FileDescriptor> OpenRelativeFile(const fs::path& root, const fs::path& relative_path, int access, bool create) {
    auto parent = OpenRelativeParent(root, relative_path, create);
    if (!parent)
        return Err(parent.error());
    int flags = access | O_CLOEXEC;
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    if (create)
        flags |= O_CREAT;
    const int descriptor = ::openat(parent->directory.Get(), parent->filename.c_str(), flags, 0600);
    if (descriptor < 0) {
        const ErrorCode code = errno == ELOOP || errno == ENOTDIR ? ErrorCode::FileAccessDenied : (errno == ENOENT ? ErrorCode::FileNotFound : ErrorCode::FileReadError);
        return Err(code, "Failed to securely open extension file: " + SystemMessage(errno));
    }
    struct stat status{};
    if (::fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode)) {
        ::close(descriptor);
        return Err(ErrorCode::FileAccessDenied, "Extension storage entry must be a regular file.");
    }
    return Ok(FileDescriptor(descriptor));
}

[[nodiscard]] Result<void> LockDescriptor(int descriptor, int operation) {
    while (::flock(descriptor, operation) != 0) {
        if (errno == EINTR)
            continue;
        return Err(ErrorCode::FileAccessDenied, "Failed to lock extension storage file: " + SystemMessage(errno));
    }
    return Ok();
}

[[nodiscard]] Result<std::vector<u8>> ReadLockedFile(const fs::path& root, const fs::path& relative_path, std::size_t limit, std::string_view limit_message) {
    auto file = OpenRelativeFile(root, relative_path, O_RDONLY, false);
    if (!file)
        return Err(file.error());
    auto locked = LockDescriptor(file->Get(), LOCK_SH);
    if (!locked)
        return Err(locked.error());
    struct stat status{};
    if (::fstat(file->Get(), &status) != 0)
        return Err(ErrorCode::FileReadError, SystemMessage(errno));
    if (status.st_size < 0 || static_cast<std::uintmax_t>(status.st_size) > limit)
        return Err(ErrorCode::ValidationOutOfRange, limit_message);

    std::vector<u8> data;
    data.reserve(static_cast<std::size_t>(status.st_size));
    std::array<u8, 16 * 1024> buffer{};
    while (true) {
        const ssize_t count = ::read(file->Get(), buffer.data(), buffer.size());
        if (count == 0)
            break;
        if (count < 0) {
            if (errno == EINTR)
                continue;
            return Err(ErrorCode::FileReadError, "Failed to read extension file: " + SystemMessage(errno));
        }
        const std::size_t bytes = static_cast<std::size_t>(count);
        if (data.size() > limit || bytes > limit - data.size())
            return Err(ErrorCode::ValidationOutOfRange, limit_message);
        data.insert(data.end(), buffer.begin(), buffer.begin() + count);
    }
    return Ok(std::move(data));
}

[[nodiscard]] Result<void> WriteLockedFile(const fs::path& root, const fs::path& relative_path, std::span<const u8> data, bool append) {
    auto file = OpenRelativeFile(root, relative_path, O_WRONLY | (append ? O_APPEND : 0), true);
    if (!file)
        return Err(file.error());
    auto locked = LockDescriptor(file->Get(), LOCK_EX);
    if (!locked)
        return Err(locked.error());
    struct stat status{};
    if (::fstat(file->Get(), &status) != 0)
        return Err(ErrorCode::FileReadError, SystemMessage(errno));
    if (status.st_size < 0 || (append && (static_cast<std::uintmax_t>(status.st_size) > limits::kMaxFileBytes || data.size() > limits::kMaxFileBytes - static_cast<std::uintmax_t>(status.st_size))))
        return Err(ErrorCode::ValidationOutOfRange, "Extension file append would exceed 16 MiB limit.");
    if (!append && ::ftruncate(file->Get(), 0) != 0)
        return Err(ErrorCode::FileWriteError, "Failed to truncate extension data file: " + SystemMessage(errno));

    std::size_t offset = 0;
    while (offset < data.size()) {
        const ssize_t count = ::write(file->Get(), data.data() + offset, data.size() - offset);
        if (count > 0) {
            offset += static_cast<std::size_t>(count);
            continue;
        }
        if (count < 0 && errno == EINTR)
            continue;
        return Err(ErrorCode::FileWriteError, "Failed to write extension data file: " + SystemMessage(errno));
    }
    return Ok();
}

#endif

[[nodiscard]] Result<void> AtomicWrite(const fs::path& path, std::string_view value) {
    static std::atomic_uint64_t sequence{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();

#ifdef _WIN32
    auto guarded_parent = OpenWindowsParent(path, true);
    if (!guarded_parent)
        return Err(guarded_parent.error());
    HANDLE existing = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (existing != INVALID_HANDLE_VALUE) {
        FILE_ATTRIBUTE_TAG_INFO attributes{};
        const bool safe = GetFileInformationByHandleEx(existing, FileAttributeTagInfo, &attributes, sizeof(attributes)) != 0
                          && (attributes.FileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) == 0;
        CloseHandle(existing);
        if (!safe)
            return Err(ErrorCode::FileAccessDenied, "Extension config entry must be a regular non-reparse file.");
    } else {
        const DWORD open_error = GetLastError();
        if (open_error != ERROR_FILE_NOT_FOUND && open_error != ERROR_PATH_NOT_FOUND)
            return Err(ErrorCode::FileReadError, "Failed to securely inspect extension config file.");
    }
#endif

    for (int attempt = 0; attempt < 32; ++attempt) {
        const fs::path temporary = path.parent_path() / ("." + path.filename().string() + "." + std::to_string(stamp) + "." + std::to_string(sequence.fetch_add(1)) + ".tmp");
#ifdef _WIN32
        HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            if (GetLastError() == ERROR_FILE_EXISTS || GetLastError() == ERROR_ALREADY_EXISTS) {
                continue;
            }
            return Err(ErrorCode::FileWriteError, "Failed to create temporary extension config file.");
        }
        DWORD written = 0;
        const bool wrote = WriteFile(file, value.data(), static_cast<DWORD>(value.size()), &written, nullptr) != 0 && written == value.size();
        const bool flushed = wrote && FlushFileBuffers(file) != 0;
        CloseHandle(file);
        if (!flushed) {
            DeleteFileW(temporary.c_str());
            return Err(ErrorCode::FileWriteError, "Failed to write temporary extension config file.");
        }
        if (MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
            DeleteFileW(temporary.c_str());
            return Err(ErrorCode::FileWriteError, "Failed to atomically replace extension config file.");
        }
#else
        auto parent = OpenDirectoryPath(path.parent_path(), true);
        if (!parent)
            return Err(parent.error());
        const std::string temporary_name = temporary.filename().string();
        int flags = O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC;
#ifdef O_NOFOLLOW
        flags |= O_NOFOLLOW;
#endif
        const int file = ::openat(parent->Get(), temporary_name.c_str(), flags, 0600);
        if (file < 0) {
            if (errno == EEXIST) {
                continue;
            }
            return Err(ErrorCode::FileWriteError, "Failed to create temporary extension config file: " + std::error_code(errno, std::generic_category()).message());
        }
        std::size_t offset = 0;
        while (offset < value.size()) {
            const ssize_t written = ::write(file, value.data() + offset, value.size() - offset);
            if (written > 0) {
                offset += static_cast<std::size_t>(written);
                continue;
            }
            if (written < 0 && errno == EINTR) {
                continue;
            }
            const int write_error = errno;
            ::close(file);
            ::unlinkat(parent->Get(), temporary_name.c_str(), 0);
            return Err(ErrorCode::FileWriteError, "Failed to write temporary extension config file: " + std::error_code(write_error, std::generic_category()).message());
        }
        const int sync_status = ::fsync(file);
        const int sync_error = errno;
        const int close_status = ::close(file);
        if (sync_status != 0 || close_status != 0) {
            const int write_error = sync_status != 0 ? sync_error : errno;
            ::unlinkat(parent->Get(), temporary_name.c_str(), 0);
            return Err(ErrorCode::FileWriteError, "Failed to flush temporary extension config file: " + std::error_code(write_error, std::generic_category()).message());
        }
        const std::string destination_name = path.filename().string();
        if (::renameat(parent->Get(), temporary_name.c_str(), parent->Get(), destination_name.c_str()) != 0) {
            const int rename_error = errno;
            ::unlinkat(parent->Get(), temporary_name.c_str(), 0);
            return Err(ErrorCode::FileWriteError, "Failed to atomically replace extension config file: " + std::error_code(rename_error, std::generic_category()).message());
        }
        if (::fsync(parent->Get()) != 0)
            return Err(ErrorCode::FileWriteError, "Extension config was replaced, but its containing directory could not be flushed: " + SystemMessage(errno));
#endif
        return Ok();
    }
    return Err(ErrorCode::FileWriteError, "Failed to allocate a unique temporary extension config file.");
}

} // namespace

HostApi::HostApi(Context context) noexcept
    : context_(std::move(context)) {}

bool HostApi::Allows(Permission permission) const noexcept {
    return std::ranges::find(context_.granted_permissions, permission) != context_.granted_permissions.end();
}

void HostApi::Log(LogLevel level, std::string_view message) const {
    if (!Require(Permission::Log)) {
        return;
    }

    if (message.size() > limits::kMaxLogBytes) {
        message = message.substr(0, limits::kMaxLogBytes);
    }

    switch (level) {
        case LogLevel::Debug:
            slog::Debug("[{}] {}", context_.extension_id, message);
            break;
        case LogLevel::Info:
            slog::Info("[{}] {}", context_.extension_id, message);
            break;
        case LogLevel::Warn:
            slog::Warn("[{}] {}", context_.extension_id, message);
            break;
        case LogLevel::Error:
            slog::Error("[{}] {}", context_.extension_id, message);
            break;
    }
}

Result<fs::path> HostApi::DataPath() const {
    auto allowed = Require(Permission::Paths);
    if (!allowed) {
        return Err(allowed.error());
    }
#ifdef _WIN32
    auto created = EnsureDirectory(context_.data_root);
#else
    auto created = EnsureSecureDirectory(context_.data_root);
#endif
    if (!created) {
        return Err(created.error());
    }
    return Ok(context_.data_root);
}

Result<fs::path> HostApi::CachePath() const {
    auto allowed = Require(Permission::Paths);
    if (!allowed) {
        return Err(allowed.error());
    }
#ifdef _WIN32
    auto created = EnsureDirectory(context_.cache_root);
#else
    auto created = EnsureSecureDirectory(context_.cache_root);
#endif
    if (!created) {
        return Err(created.error());
    }
    return Ok(context_.cache_root);
}

Result<std::vector<u8>> HostApi::ReadFile(const fs::path& relative_path) const {
    auto file = ResolveDataFile(relative_path);
    if (!file) {
        return Err(file.error());
    }

#ifdef _WIN32
    return ReadLockedFile(*file, limits::kMaxFileBytes, "Extension file read exceeds 16 MiB limit.");
#else
    return ReadLockedFile(context_.data_root, relative_path, limits::kMaxFileBytes, "Extension file read exceeds 16 MiB limit.");
#endif
}

Result<void> HostApi::WriteFile(const fs::path& relative_path, std::span<const u8> data) const {
    if (data.size() > limits::kMaxFileBytes) {
        return Err(ErrorCode::ValidationOutOfRange, "Extension file write exceeds 16 MiB limit.");
    }

    auto file = ResolveDataFile(relative_path);
    if (!file) {
        return Err(file.error());
    }

#ifdef _WIN32
    return WriteLockedFile(*file, data, false);
#else
    return WriteLockedFile(context_.data_root, relative_path, data, false);
#endif
}

Result<void> HostApi::AppendFile(const fs::path& relative_path, std::span<const u8> data) const {
    if (data.size() > limits::kMaxFileBytes) {
        return Err(ErrorCode::ValidationOutOfRange, "Extension file append exceeds 16 MiB limit.");
    }

    auto file = ResolveDataFile(relative_path);
    if (!file) {
        return Err(file.error());
    }

#ifdef _WIN32
    return WriteLockedFile(*file, data, true);
#else
    return WriteLockedFile(context_.data_root, relative_path, data, true);
#endif
}

Result<std::string> HostApi::ReadConfig(std::string_view key) const {
    auto file = ResolveConfigFile(key);
    if (!file) {
        return Err(file.error());
    }

#ifdef _WIN32
    auto data = ReadLockedFile(*file, limits::kMaxConfigValueBytes, "Extension config value exceeds 64 KiB limit.");
#else
    auto data = ReadLockedFile(context_.config_root, fs::path(std::string(key)), limits::kMaxConfigValueBytes, "Extension config value exceeds 64 KiB limit.");
#endif
    if (!data)
        return Err(data.error());
    if (data->empty())
        return Ok(std::string{});
    if (std::ranges::find(*data, u8{0}) != data->end())
        return Err(ErrorCode::ParseInvalidFormat, "Extension config value contains an embedded NUL byte.");
    return Ok(std::string(reinterpret_cast<const char*>(data->data()), data->size()));
}

Result<void> HostApi::WriteConfig(std::string_view key, std::string_view value) const {
    if (value.size() > limits::kMaxConfigValueBytes) {
        return Err(ErrorCode::ValidationOutOfRange, "Extension config value exceeds 64 KiB limit.");
    }
    if (value.contains('\0')) {
        return Err(ErrorCode::InvalidArgument, "Extension config value must not contain embedded NUL bytes.");
    }

    auto file = ResolveConfigFile(key);
    if (!file) {
        return Err(file.error());
    }

    auto safe = RejectSymlinks(context_.config_root, std::string(key));
    if (!safe) {
        return Err(safe.error());
    }

    return AtomicWrite(*file, value);
}

Result<void> HostApi::SubscribeEvent(u32 event_type) const {
    if (auto allowed = Require(Permission::Events); !allowed)
        return Err(allowed.error());
    if (context_.event_session == nullptr)
        return Err(ErrorCode::InvalidState, "Extension event session is not configured.");
    context_.event_session->Subscribe(event_type);
    return Ok();
}

Result<void> HostApi::EmitEvent(u32 event_type, std::span<const u8> payload) const {
    if (auto allowed = Require(Permission::Events); !allowed)
        return Err(allowed.error());
    if (event_type == kWildcardEventType || (event_type & kExtensionEventNamespace) == 0)
        return Err(ErrorCode::InvalidArgument, "Guest event types must use the extension namespace (high bit set).");
    if (payload.size() > limits::kMaxEventBytes)
        return Err(ErrorCode::ValidationOutOfRange, "Extension event payload exceeds 64 KiB limit.");
    if (context_.event_service == nullptr)
        return Err(ErrorCode::InvalidState, "Extension event service is not configured.");
    return context_.event_service->Enqueue({event_type, std::vector<u8>(payload.begin(), payload.end()), {EventOriginKind::Extension, context_.extension_id}, std::nullopt});
}

Result<void> HostApi::SubscribeNamedEvent(std::string_view topic) const {
    if (auto allowed = Require(Permission::Events); !allowed)
        return Err(allowed.error());
    if (!IsValidEventTopic(topic))
        return Err(ErrorCode::InvalidArgument, "Named event topic must be a lowercase reverse-DNS name.");
    if (context_.event_session == nullptr)
        return Err(ErrorCode::InvalidState, "Extension event session is not configured.");
    context_.event_session->Subscribe(std::string(topic));
    return Ok();
}

Result<void> HostApi::EmitNamedEvent(std::string_view topic, std::span<const u8> payload) const {
    if (auto allowed = Require(Permission::Events); !allowed)
        return Err(allowed.error());
    if (!IsValidEventTopic(topic) || topic.size() <= context_.extension_id.size() || !topic.starts_with(context_.extension_id) || topic[context_.extension_id.size()] != '.')
        return Err(ErrorCode::InvalidArgument, "Named event topic must be prefixed by the emitting extension id.");
    if (payload.size() > limits::kMaxEventBytes)
        return Err(ErrorCode::ValidationOutOfRange, "Extension event payload exceeds 64 KiB limit.");
    if (context_.event_service == nullptr)
        return Err(ErrorCode::InvalidState, "Extension event service is not configured.");
    Event event{0, std::vector<u8>(payload.begin(), payload.end()), {EventOriginKind::Extension, context_.extension_id}, std::nullopt};
    event.topic = std::string(topic);
    return context_.event_service->Enqueue(std::move(event));
}

Result<void> HostApi::Require(Permission permission) const {
    if (!Allows(permission)) {
        return Err(ErrorCode::FileAccessDenied, "Extension '" + context_.extension_id + "' does not declare permission '" + std::string(ToString(permission)) + "'. Add it to manifest.yaml permissions.");
    }
    return Ok();
}

Result<fs::path> HostApi::ResolveDataFile(const fs::path& relative_path) const {
    auto allowed = Require(Permission::Storage);
    if (!allowed) {
        return Err(allowed.error());
    }
    if (context_.data_root.empty()) {
        return Err(ErrorCode::InvalidState, "Extension data root is not configured.");
    }
#ifdef _WIN32
    if (relative_path.native().contains(L'\\')) {
        return Err(ErrorCode::InvalidArgument, "Extension storage path must be a portable relative path without '.', '..', or backslashes.");
    }
#endif
    if (!IsSafeRelativePath(relative_path)) {
        return Err(ErrorCode::InvalidArgument, "Extension storage path must be a portable relative path without '.', '..', or backslashes.");
    }

    auto safe = RejectSymlinks(context_.data_root, relative_path);
    if (!safe) {
        return Err(safe.error());
    }

    return Ok((context_.data_root / relative_path).lexically_normal());
}

Result<fs::path> HostApi::ResolveConfigFile(std::string_view key) const {
    auto allowed = Require(Permission::Config);
    if (!allowed) {
        return Err(allowed.error());
    }
    if (context_.config_root.empty()) {
        return Err(ErrorCode::InvalidState, "Extension config root is not configured.");
    }
    if (!IsSafeConfigKey(key)) {
        return Err(ErrorCode::InvalidArgument, "Extension config key must use only letters, digits, '.', '_' or '-'.");
    }

    auto safe = RejectSymlinks(context_.config_root, std::string(key));
    if (!safe) {
        return Err(safe.error());
    }

    return Ok((context_.config_root / std::string(key)).lexically_normal());
}

} // namespace woki::ext::host
