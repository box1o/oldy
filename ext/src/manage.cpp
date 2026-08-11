#include <cerrno>
#include <memory>
#include <filesystem>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif !defined(__EMSCRIPTEN__)
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#endif

#include <woki/ext/package.hpp>
#include <woki/ext/manifest.hpp>
#include <woki/ext/registry.hpp>

#include "cli_internal.hpp"

namespace wokiext {

namespace {

namespace fs = std::filesystem;

class PackageInstallLock final {
public:
    PackageInstallLock(const PackageInstallLock&) = delete;
    PackageInstallLock& operator=(const PackageInstallLock&) = delete;

    ~PackageInstallLock() {
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

    [[nodiscard]] static std::expected<std::unique_ptr<PackageInstallLock>, std::string> Acquire(const fs::path& root) {
        std::error_code directory_error;
        fs::create_directories(root, directory_error);
        if (directory_error)
            return std::unexpected("Failed to create extension install root before locking: " + directory_error.message());
        auto lock = std::unique_ptr<PackageInstallLock>(new PackageInstallLock());
        const fs::path path = root / ".woki-install.lock";
#ifdef _WIN32
        lock->handle_ = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_HIDDEN | FILE_FLAG_OPEN_REPARSE_POINT,
            nullptr);
        if (lock->handle_ == INVALID_HANDLE_VALUE)
            return std::unexpected("Failed to open extension install lock");
        FILE_ATTRIBUTE_TAG_INFO attributes{};
        if (GetFileInformationByHandleEx(lock->handle_, FileAttributeTagInfo, &attributes, sizeof(attributes)) == 0 || (attributes.FileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) != 0)
            return std::unexpected("Extension install lock must be a regular non-reparse file");
        OVERLAPPED overlapped{};
        if (LockFileEx(lock->handle_, LOCKFILE_EXCLUSIVE_LOCK, 0, MAXDWORD, MAXDWORD, &overlapped) == 0)
            return std::unexpected("Failed to acquire extension install lock");
#elif !defined(__EMSCRIPTEN__)
        int flags = O_RDWR | O_CREAT | O_CLOEXEC;
#ifdef O_NOFOLLOW
        flags |= O_NOFOLLOW;
#endif
        lock->file_ = ::open(path.c_str(), flags, 0600);
        if (lock->file_ < 0 || ::flock(lock->file_, LOCK_EX) != 0)
            return std::unexpected("Failed to acquire extension install lock: " + std::error_code(errno, std::generic_category()).message());
#endif
        return lock;
    }

private:
    PackageInstallLock() = default;
#ifdef _WIN32
    HANDLE handle_{INVALID_HANDLE_VALUE};
#elif !defined(__EMSCRIPTEN__)
    int file_{-1};
#endif
};

[[nodiscard]] bool RemovePath(Context& context, const fs::path& path) {
    std::error_code error;
    fs::remove_all(path, error);
    if (error) {
        context.diagnostics.Err() << error.message() << ": " << path << '\n';
        return false;
    }
    return true;
}

} // namespace

Status List(Context& context, const ListOptions& options) {
    auto roots = woki::ext::RootsFromBase(options.root);
    if (!roots) {
        context.diagnostics.Error(roots.error().Message());
        return Status::Error;
    }

    woki::ext::Registry registry;
    auto scanned = registry.Scan(*roots);
    if (!scanned) {
        context.diagnostics.Error(scanned.error().Message());
        return Status::Error;
    }

    for (const woki::ext::ExtensionPackage& package : registry.Packages()) {
        context.diagnostics.Out() << package.Id() << " " << package.GetManifest().version << " ok\n";
    }
    for (const woki::ext::DiscoveryFailure& failure : registry.Failures()) {
        context.diagnostics.Out() << failure.CandidateId() << " failed " << failure.Cause().Message() << '\n';
    }

    return Status::Ok;
}

Status Remove(Context& context, const RemoveOptions& options) {
    if (!woki::ext::IsValidExtensionId(options.id)) {
        context.diagnostics.Error("Extension id must be a valid lowercase reverse-DNS id");
        return Status::Usage;
    }

    auto requested_roots = woki::ext::RootsFromBase(options.root);
    if (!requested_roots) {
        context.diagnostics.Error(requested_roots.error().Message());
        return Status::Error;
    }
    auto roots = woki::ext::ValidateRoots(*requested_roots);
    if (!roots) {
        context.diagnostics.Error(roots.error().Message());
        return Status::Error;
    }

    auto lock = PackageInstallLock::Acquire(roots->extensions);
    if (!lock) {
        context.diagnostics.Error(lock.error());
        return Status::Error;
    }

    bool ok = RemovePath(context, roots->extensions / options.id);
    if (!options.keep_data) {
        ok = RemovePath(context, roots->data / options.id) && ok;
        ok = RemovePath(context, roots->config / options.id) && ok;
        ok = RemovePath(context, roots->cache / options.id) && ok;
    }
    if (!ok) {
        return Status::Error;
    }

    context.diagnostics.Out() << "Removed extension: " << options.id;
    if (options.keep_data)
        context.diagnostics.Out() << " (kept data, config, and cache)";
    context.diagnostics.Out() << '\n';
    return Status::Ok;
}

} // namespace wokiext
