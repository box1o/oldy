#include <CoreServices/CoreServices.h>

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "file_watcher_internal.hpp"

namespace woki::detail {
namespace {

class MacFileWatcherBackend final : public FileWatcherBackend {
public:
    static Result<scope<FileWatcherBackend>> Create(const std::filesystem::path& root, FileWatcherState& state) {
        auto backend = scope<MacFileWatcherBackend>(new MacFileWatcherBackend(root, state));
        const auto root_string = root.string();
        CFStringRef path = CFStringCreateWithFileSystemRepresentation(kCFAllocatorDefault, root_string.c_str());
        if (path == nullptr) {
            return Err(ErrorCode::FailedToAcquireResource, "Failed to encode file watcher root");
        }
        const void* values[] = {path};
        CFArrayRef paths = CFArrayCreate(kCFAllocatorDefault, values, 1, &kCFTypeArrayCallBacks);
        CFRelease(path);
        if (paths == nullptr) {
            return Err(ErrorCode::FailedToAcquireResource, "Failed to create FSEvents path list");
        }

        FSEventStreamContext context{};
        context.info = backend.get();
        constexpr FSEventStreamCreateFlags flags = kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagWatchRoot | kFSEventStreamCreateFlagNoDefer;
        backend->stream_ = FSEventStreamCreate(kCFAllocatorDefault, &Callback, &context, paths, kFSEventStreamEventIdSinceNow, 0.05, flags);
        CFRelease(paths);
        if (backend->stream_ == nullptr) {
            return Err(ErrorCode::FailedToAcquireResource, "Failed to create FSEvents stream");
        }

        try {
            state.running.store(true, std::memory_order_release);
            backend->thread_ = std::thread([instance = backend.get()] { instance->RunBoundary(); });
        } catch (...) {
            state.running.store(false, std::memory_order_release);
            throw;
        }

        std::unique_lock lock(backend->run_loop_mutex_);
        backend->started_condition_.wait(lock, [&] { return backend->startup_complete_; });
        if (!backend->startup_succeeded_) {
            lock.unlock();
            backend->Stop();
            return Err(ErrorCode::FailedToAcquireResource, "Failed to start FSEvents stream");
        }
        return scope<FileWatcherBackend>(std::move(backend));
    }

    ~MacFileWatcherBackend() override {
        Stop();
        if (stream_ != nullptr) {
            FSEventStreamInvalidate(stream_);
            FSEventStreamRelease(stream_);
        }
    }

    void Stop() noexcept override {
        if (!stop_requested_.exchange(true, std::memory_order_acq_rel)) {
            std::scoped_lock lock(run_loop_mutex_);
            if (run_loop_ != nullptr) {
                CFRunLoopStop(run_loop_);
            }
        }
        if (thread_.joinable()) {
            thread_.join();
        }
        state_.running.store(false, std::memory_order_release);
    }

private:
    MacFileWatcherBackend(std::filesystem::path root, FileWatcherState& state)
        : root_(std::move(root)),
          state_(state) {}

    static void Callback(ConstFSEventStreamRef, void* context, std::size_t count, void* paths, const FSEventStreamEventFlags flags[], const FSEventStreamEventId[]) noexcept {
        auto& backend = *static_cast<MacFileWatcherBackend*>(context);
        try {
            const auto event_paths = static_cast<char**>(paths);
            for (std::size_t index = 0; index < count; ++index) {
                backend.Handle(event_paths[index], flags[index]);
                if (backend.stop_requested_.load(std::memory_order_acquire)) {
                    break;
                }
            }
        } catch (...) {
            backend.state_.PushRescan();
        }
    }

    void Handle(const char* event_path, FSEventStreamEventFlags flags) {
        if ((flags & kFSEventStreamEventFlagRootChanged) != 0U) {
            state_.PushRescan();
            stop_requested_.store(true, std::memory_order_release);
            std::scoped_lock lock(run_loop_mutex_);
            if (run_loop_ != nullptr) {
                CFRunLoopStop(run_loop_);
            }
            return;
        }
        constexpr FSEventStreamEventFlags dropped = kFSEventStreamEventFlagMustScanSubDirs | kFSEventStreamEventFlagUserDropped | kFSEventStreamEventFlagKernelDropped | kFSEventStreamEventFlagEventIdsWrapped;
        if ((flags & dropped) != 0U || (flags & kFSEventStreamEventFlagItemRenamed) != 0U) {
            state_.PushRescan();
            return;
        }

        const auto relative = std::filesystem::path(event_path).lexically_relative(root_).lexically_normal();
        if (relative.empty() || relative.is_absolute() || *relative.begin() == "..") {
            return;
        }
        if ((flags & kFSEventStreamEventFlagItemCreated) != 0U) {
            state_.Push({FileWatchEventType::Added, relative});
        }
        if ((flags & kFSEventStreamEventFlagItemRemoved) != 0U) {
            state_.Push({FileWatchEventType::Removed, relative});
        }
        if ((flags
                & (kFSEventStreamEventFlagItemModified | kFSEventStreamEventFlagItemInodeMetaMod | kFSEventStreamEventFlagItemFinderInfoMod | kFSEventStreamEventFlagItemChangeOwner | kFSEventStreamEventFlagItemXattrMod))
            != 0U) {
            state_.Push({FileWatchEventType::Modified, relative});
        }
    }

    void RunBoundary() noexcept {
        try {
            Run();
        } catch (...) {
            {
                std::scoped_lock lock(run_loop_mutex_);
                startup_complete_ = true;
            }
            started_condition_.notify_all();
            state_.PushRescan();
        }
        state_.running.store(false, std::memory_order_release);
    }

    void Run() {
        CFRunLoopRef loop = CFRunLoopGetCurrent();
        CFRetain(loop);
        {
            std::scoped_lock lock(run_loop_mutex_);
            run_loop_ = loop;
        }
        FSEventStreamScheduleWithRunLoop(stream_, loop, kCFRunLoopDefaultMode);
        const bool started = FSEventStreamStart(stream_);
        {
            std::scoped_lock lock(run_loop_mutex_);
            startup_succeeded_ = started;
            startup_complete_ = true;
        }
        started_condition_.notify_all();
        if (started) {
            while (!stop_requested_.load(std::memory_order_acquire)) {
                CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.25, false);
            }
            FSEventStreamStop(stream_);
        }
        FSEventStreamUnscheduleFromRunLoop(stream_, loop, kCFRunLoopDefaultMode);
        {
            std::scoped_lock lock(run_loop_mutex_);
            run_loop_ = nullptr;
        }
        CFRelease(loop);
    }

    std::filesystem::path root_;
    FileWatcherState& state_;
    FSEventStreamRef stream_{nullptr};
    CFRunLoopRef run_loop_{nullptr};
    std::mutex run_loop_mutex_;
    std::condition_variable started_condition_;
    bool startup_complete_{false};
    bool startup_succeeded_{false};
    std::atomic_bool stop_requested_{false};
    std::thread thread_;
};

} // namespace

Result<scope<FileWatcherBackend>> CreateFileWatcherBackend(const std::filesystem::path& root, FileWatcherState& state) {
    return MacFileWatcherBackend::Create(root, state);
}

} // namespace woki::detail
