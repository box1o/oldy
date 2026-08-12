#include <filesystem>
#include <exception>
#include <mutex>
#include <system_error>
#include <utility>

#include <woki/platform/file_watcher.hpp>

#include "file_watcher_internal.hpp"

namespace woki {

struct FileWatcher::Impl {
    Impl(std::filesystem::path watched_root, std::size_t capacity)
        : root(std::move(watched_root)),
          state(capacity) {}

    std::filesystem::path root;
    detail::FileWatcherState state;
    scope<detail::FileWatcherBackend> backend;
    std::mutex stop_mutex;
};

FileWatcher::FileWatcher(scope<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

FileWatcher::~FileWatcher() {
    Stop();
}

Result<scope<FileWatcher>> FileWatcher::Create(const std::filesystem::path& root, const FileWatcherOptions& options) {
    if (root.empty()) {
        return Err(ErrorCode::InvalidArgument, "File watcher root cannot be empty");
    }
    if (options.queue_capacity == 0) {
        return Err(ErrorCode::InvalidArgument, "File watcher queue capacity must be greater than zero");
    }

    try {
        std::error_code error;
        auto normalized_root = std::filesystem::absolute(root, error).lexically_normal();
        if (error) {
            return Err(ErrorCode::FileAccessDenied, "Failed to resolve file watcher root");
        }
        const auto status = std::filesystem::status(normalized_root, error);
        if (error || !std::filesystem::exists(status)) {
            return Err(ErrorCode::FileNotFound, "File watcher root does not exist");
        }
        if (!std::filesystem::is_directory(status)) {
            return Err(ErrorCode::InvalidArgument, "File watcher root is not a directory");
        }
        normalized_root = std::filesystem::canonical(normalized_root, error);
        if (error) {
            return Err(ErrorCode::FileAccessDenied, "Failed to canonicalize file watcher root");
        }

        auto impl = createScope<Impl>(std::move(normalized_root), options.queue_capacity);
        auto backend = detail::CreateFileWatcherBackend(impl->root, impl->state);
        if (!backend) {
            return Err(std::move(backend).error());
        }
        impl->backend = std::move(*backend);
        return scope<FileWatcher>(new FileWatcher(std::move(impl)));
    } catch (const std::exception& exception) {
        return Err(ErrorCode::FailedToAcquireResource, exception.what());
    } catch (...) {
        return Err(ErrorCode::UnknownError, "Failed to create file watcher");
    }
}

std::vector<FileWatchEvent> FileWatcher::Drain() {
    return impl_->state.Drain();
}

const std::filesystem::path& FileWatcher::Root() const noexcept {
    return impl_->root;
}

bool FileWatcher::IsRunning() const noexcept {
    return impl_->state.running.load(std::memory_order_acquire);
}

void FileWatcher::Stop() noexcept {
    if (impl_ && impl_->backend) {
        std::scoped_lock lock(impl_->stop_mutex);
        impl_->backend->Stop();
    }
}

} // namespace woki
