#include <array>
#include <cerrno>
#include <poll.h>
#include <string>
#include <thread>
#include <cstdint>
#include <cstring>
#include <utility>
#include <unistd.h>
#include <stdexcept>
#include <filesystem>
#include <system_error>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <unordered_map>

#include "file_watcher_internal.hpp"

namespace woki::detail {
namespace {

constexpr std::uint32_t kWatchMask = IN_CREATE | IN_CLOSE_WRITE | IN_MODIFY | IN_ATTRIB | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE_SELF | IN_MOVE_SELF;

class LinuxFileWatcherBackend final : public FileWatcherBackend {
public:
    static Result<scope<FileWatcherBackend>> Create(const std::filesystem::path& root, FileWatcherState& state) {
        const int inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (inotify_fd < 0) {
            return Err(ErrorCode::FailedToAcquireResource, std::strerror(errno));
        }
        const int wake_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (wake_fd < 0) {
            const std::string message = std::strerror(errno);
            close(inotify_fd);
            return Err(ErrorCode::FailedToAcquireResource, message);
        }

        scope<LinuxFileWatcherBackend> backend;
        try {
            backend.reset(new LinuxFileWatcherBackend(root, state, inotify_fd, wake_fd));
        } catch (...) {
            close(wake_fd);
            close(inotify_fd);
            throw;
        }
        if (!backend->AddTree(root, {})) {
            return Err(ErrorCode::FileAccessDenied, "Failed to watch file watcher root");
        }

        try {
            state.running.store(true, std::memory_order_release);
            backend->thread_ = std::thread([instance = backend.get()] { instance->RunBoundary(); });
        } catch (...) {
            state.running.store(false, std::memory_order_release);
            throw;
        }
        return scope<FileWatcherBackend>(std::move(backend));
    }

    ~LinuxFileWatcherBackend() override {
        Stop();
        close(wake_fd_);
        close(inotify_fd_);
    }

    void Stop() noexcept override {
        if (!stop_requested_.exchange(true, std::memory_order_acq_rel)) {
            const std::uint64_t value = 1;
            [[maybe_unused]] const auto written = write(wake_fd_, &value, sizeof(value));
        }
        if (thread_.joinable()) {
            thread_.join();
        }
        state_.running.store(false, std::memory_order_release);
    }

private:
    LinuxFileWatcherBackend(std::filesystem::path root, FileWatcherState& state, int inotify_fd, int wake_fd)
        : root_(std::move(root)),
          state_(state),
          inotify_fd_(inotify_fd),
          wake_fd_(wake_fd) {}

    bool AddWatch(const std::filesystem::path& directory, const std::filesystem::path& relative) {
        const int descriptor = inotify_add_watch(inotify_fd_, directory.c_str(), kWatchMask);
        if (descriptor < 0) {
            return false;
        }
        watches_[descriptor] = relative.lexically_normal();
        return true;
    }

    bool AddTree(const std::filesystem::path& directory, const std::filesystem::path& relative, bool report_existing = false) {
        if (!AddWatch(directory, relative)) {
            return false;
        }

        std::error_code error;
        std::filesystem::recursive_directory_iterator iterator(directory, std::filesystem::directory_options::skip_permission_denied, error);
        const std::filesystem::recursive_directory_iterator end;
        while (!error && iterator != end) {
            const auto child_relative = relative / iterator->path().lexically_relative(directory);
            const auto symlink_status = iterator->symlink_status(error);
            if (error) {
                break;
            }
            if (std::filesystem::is_symlink(symlink_status)) {
                if (std::filesystem::is_directory(iterator->status(error))) {
                    iterator.disable_recursion_pending();
                }
                error.clear();
            } else if (std::filesystem::is_directory(symlink_status)) {
                if (!AddWatch(iterator->path(), child_relative)) {
                    return false;
                }
            }
            if (report_existing) {
                state_.Push({FileWatchEventType::Added, child_relative.lexically_normal()});
            }
            iterator.increment(error);
        }
        return !error;
    }

    void Rebuild() {
        for (const auto& [descriptor, path] : watches_) {
            (void)path;
            inotify_rm_watch(inotify_fd_, descriptor);
        }
        watches_.clear();
        std::array<char, 4096> discarded{};
        while (read(inotify_fd_, discarded.data(), discarded.size()) > 0) {
        }
        if (!AddTree(root_, {})) {
            state_.PushRescan();
            state_.running.store(false, std::memory_order_release);
            stop_requested_.store(true, std::memory_order_release);
        }
    }

    void Handle(const inotify_event& event) {
        if ((event.mask & IN_Q_OVERFLOW) != 0U) {
            state_.PushRescan();
            rebuild_requested_ = true;
            return;
        }
        const auto watch = watches_.find(event.wd);
        if (watch == watches_.end()) {
            return;
        }
        if ((event.mask & (IN_DELETE_SELF | IN_MOVE_SELF)) != 0U && watch->second.empty()) {
            state_.PushRescan();
            state_.running.store(false, std::memory_order_release);
            stop_requested_.store(true, std::memory_order_release);
            return;
        }
        if ((event.mask & IN_IGNORED) != 0U) {
            watches_.erase(watch);
            return;
        }

        const std::filesystem::path name = event.len == 0 ? std::filesystem::path{} : std::filesystem::path(event.name);
        const auto relative = (watch->second / name).lexically_normal();
        const bool directory = (event.mask & IN_ISDIR) != 0U;

        if (directory && (event.mask & (IN_MOVED_FROM | IN_MOVED_TO)) != 0U) {
            state_.PushRescan();
            rebuild_requested_ = true;
            return;
        }
        if (directory && (event.mask & IN_CREATE) != 0U) {
            const auto absolute = root_ / relative;
            std::error_code error;
            if (!std::filesystem::is_symlink(std::filesystem::symlink_status(absolute, error)) && !error) {
                if (!AddTree(absolute, relative, true)) {
                    state_.PushRescan();
                    rebuild_requested_ = true;
                    return;
                }
            }
        }

        if ((event.mask & (IN_CREATE | IN_MOVED_TO)) != 0U) {
            state_.Push({FileWatchEventType::Added, relative});
        } else if ((event.mask & (IN_DELETE | IN_MOVED_FROM)) != 0U) {
            state_.Push({FileWatchEventType::Removed, relative});
        } else if ((event.mask & (IN_CLOSE_WRITE | IN_MODIFY | IN_ATTRIB)) != 0U) {
            state_.Push({FileWatchEventType::Modified, relative});
        }
    }

    void RunBoundary() noexcept {
        try {
            Run();
        } catch (...) {
            state_.PushRescan();
        }
        state_.running.store(false, std::memory_order_release);
    }

    void Run() {
        alignas(inotify_event) std::array<char, 64 * 1024> buffer{};
        const std::array<pollfd, 2> base_fds{{
            {inotify_fd_, POLLIN, 0},
            {wake_fd_, POLLIN, 0},
        }};
        while (!stop_requested_.load(std::memory_order_acquire)) {
            auto fds = base_fds;
            const int result = poll(fds.data(), fds.size(), -1);
            if (result < 0) {
                if (errno == EINTR) {
                    continue;
                }
                throw std::system_error(errno, std::generic_category());
            }
            if ((fds[1].revents & POLLIN) != 0) {
                return;
            }
            if ((fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                throw std::runtime_error("inotify polling failed");
            }
            if ((fds[0].revents & POLLIN) == 0) {
                continue;
            }

            for (;;) {
                const auto size = read(inotify_fd_, buffer.data(), buffer.size());
                if (size < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    break;
                }
                if (size < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    throw std::system_error(errno, std::generic_category());
                }
                std::size_t offset = 0;
                while (offset < static_cast<std::size_t>(size)) {
                    const auto* event = reinterpret_cast<const inotify_event*>(buffer.data() + offset);
                    Handle(*event);
                    offset += sizeof(inotify_event) + event->len;
                }
                if (rebuild_requested_) {
                    rebuild_requested_ = false;
                    Rebuild();
                    break;
                }
            }
        }
    }

    std::filesystem::path root_;
    FileWatcherState& state_;
    int inotify_fd_;
    int wake_fd_;
    std::unordered_map<int, std::filesystem::path> watches_;
    bool rebuild_requested_{false};
    std::atomic_bool stop_requested_{false};
    std::thread thread_;
};

} // namespace

Result<scope<FileWatcherBackend>> CreateFileWatcherBackend(const std::filesystem::path& root, FileWatcherState& state) {
    return LinuxFileWatcherBackend::Create(root, state);
}

} // namespace woki::detail
