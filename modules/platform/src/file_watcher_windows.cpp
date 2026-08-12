#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>

#include "file_watcher_internal.hpp"

namespace woki::detail {
namespace {

class WindowsFileWatcherBackend final : public FileWatcherBackend {
public:
    static Result<scope<FileWatcherBackend>> Create(const std::filesystem::path& root, FileWatcherState& state) {
        const HANDLE directory = CreateFileW(root.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
        if (directory == INVALID_HANDLE_VALUE) {
            return Err(ErrorCode::FileAccessDenied, "Failed to open file watcher root");
        }
        const HANDLE io_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        const HANDLE stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (io_event == nullptr || stop_event == nullptr) {
            if (io_event != nullptr) {
                CloseHandle(io_event);
            }
            if (stop_event != nullptr) {
                CloseHandle(stop_event);
            }
            CloseHandle(directory);
            return Err(ErrorCode::FailedToAcquireResource, "Failed to create file watcher events");
        }

        scope<WindowsFileWatcherBackend> backend;
        try {
            backend.reset(new WindowsFileWatcherBackend(state, directory, io_event, stop_event));
        } catch (...) {
            CloseHandle(stop_event);
            CloseHandle(io_event);
            CloseHandle(directory);
            throw;
        }
        try {
            state.running.store(true, std::memory_order_release);
            backend->thread_ = std::thread([instance = backend.get()] { instance->RunBoundary(); });
        } catch (...) {
            state.running.store(false, std::memory_order_release);
            throw;
        }
        std::unique_lock lock(backend->startup_mutex_);
        backend->startup_condition_.wait(lock, [&] { return backend->startup_complete_; });
        if (!backend->startup_succeeded_) {
            lock.unlock();
            backend->Stop();
            return Err(ErrorCode::FailedToAcquireResource, "Failed to start the Windows file watcher");
        }
        return scope<FileWatcherBackend>(std::move(backend));
    }

    ~WindowsFileWatcherBackend() override {
        Stop();
        CloseHandle(stop_event_);
        CloseHandle(io_event_);
        CloseHandle(directory_);
    }

    void Stop() noexcept override {
        if (!stop_requested_.exchange(true, std::memory_order_acq_rel)) {
            SetEvent(stop_event_);
        }
        if (thread_.joinable()) {
            thread_.join();
        }
        state_.running.store(false, std::memory_order_release);
    }

private:
    WindowsFileWatcherBackend(FileWatcherState& state, HANDLE directory, HANDLE io_event, HANDLE stop_event)
        : state_(state),
          directory_(directory),
          io_event_(io_event),
          stop_event_(stop_event) {}

    void Push(const FILE_NOTIFY_INFORMATION& information) {
        const std::wstring_view name(information.FileName, information.FileNameLength / sizeof(wchar_t));
        const auto path = std::filesystem::path(name).lexically_normal();
        switch (information.Action) {
            case FILE_ACTION_ADDED:
            case FILE_ACTION_RENAMED_NEW_NAME:
                state_.Push({FileWatchEventType::Added, path});
                break;
            case FILE_ACTION_REMOVED:
            case FILE_ACTION_RENAMED_OLD_NAME:
                state_.Push({FileWatchEventType::Removed, path});
                break;
            case FILE_ACTION_MODIFIED:
                state_.Push({FileWatchEventType::Modified, path});
                break;
            default:
                break;
        }
    }

    void RunBoundary() noexcept {
        try {
            Run();
        } catch (...) {
            ReportStartup(false);
            state_.PushRescan();
        }
        state_.running.store(false, std::memory_order_release);
    }

    void Run() {
        constexpr DWORD notify_filter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION;
        alignas(DWORD) std::array<std::byte, 64 * 1024> buffer{};

        while (!stop_requested_.load(std::memory_order_acquire)) {
            OVERLAPPED overlapped{};
            overlapped.hEvent = io_event_;
            ResetEvent(io_event_);
            const BOOL requested = ReadDirectoryChangesW(directory_, buffer.data(), static_cast<DWORD>(buffer.size()), TRUE, notify_filter, nullptr, &overlapped, nullptr);
            if (requested == FALSE && GetLastError() != ERROR_IO_PENDING) {
                throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
            }
            ReportStartup(true);

            const std::array<HANDLE, 2> events{stop_event_, io_event_};
            const DWORD wait_result = WaitForMultipleObjects(static_cast<DWORD>(events.size()), events.data(), FALSE, INFINITE);
            if (wait_result == WAIT_OBJECT_0) {
                CancelIoEx(directory_, &overlapped);
                DWORD transferred = 0;
                if (GetOverlappedResult(directory_, &overlapped, &transferred, TRUE) == FALSE) {
                    const DWORD error = GetLastError();
                    if (error != ERROR_OPERATION_ABORTED) {
                        throw std::system_error(static_cast<int>(error), std::system_category());
                    }
                }
                return;
            }
            if (wait_result != WAIT_OBJECT_0 + 1) {
                throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
            }

            DWORD transferred = 0;
            if (GetOverlappedResult(directory_, &overlapped, &transferred, FALSE) == FALSE) {
                const DWORD error = GetLastError();
                if (error == ERROR_OPERATION_ABORTED && stop_requested_.load(std::memory_order_acquire)) {
                    return;
                }
                if (error == ERROR_NOTIFY_ENUM_DIR) {
                    state_.PushRescan();
                    continue;
                }
                throw std::system_error(static_cast<int>(error), std::system_category());
            }
            if (transferred == 0) {
                state_.PushRescan();
                continue;
            }

            DWORD offset = 0;
            for (;;) {
                const auto* information = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(buffer.data() + offset);
                Push(*information);
                if (information->NextEntryOffset == 0) {
                    break;
                }
                offset += information->NextEntryOffset;
            }
        }
    }

    void ReportStartup(const bool succeeded) noexcept {
        {
            std::scoped_lock lock(startup_mutex_);
            if (startup_complete_) {
                return;
            }
            startup_succeeded_ = succeeded;
            startup_complete_ = true;
        }
        startup_condition_.notify_all();
    }

    FileWatcherState& state_;
    HANDLE directory_;
    HANDLE io_event_;
    HANDLE stop_event_;
    std::mutex startup_mutex_;
    std::condition_variable startup_condition_;
    bool startup_complete_{false};
    bool startup_succeeded_{false};
    std::atomic_bool stop_requested_{false};
    std::thread thread_;
};

} // namespace

Result<scope<FileWatcherBackend>> CreateFileWatcherBackend(const std::filesystem::path& root, FileWatcherState& state) {
    return WindowsFileWatcherBackend::Create(root, state);
}

} // namespace woki::detail
