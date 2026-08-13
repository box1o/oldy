#pragma once

#include <mutex>
#include <atomic>
#include <vector>
#include <cstddef>
#include <utility>
#include <filesystem>

#include <woki/platform/file_watcher.hpp>

namespace woki::detail {

class FileWatcherState final {
public:
    explicit FileWatcherState(std::size_t capacity)
        : capacity_(capacity) {
        events_.reserve(capacity);
    }

    void Push(FileWatchEvent event) noexcept {
        std::scoped_lock lock(mutex_);
        if (rescan_pending_) {
            return;
        }
        if (events_.size() >= capacity_) {
            events_.clear();
            events_.push_back({FileWatchEventType::RescanRequired, {}});
            rescan_pending_ = true;
            return;
        }
        events_.push_back(std::move(event));
    }

    void PushRescan() noexcept {
        std::scoped_lock lock(mutex_);
        if (!rescan_pending_) {
            events_.clear();
            events_.push_back({FileWatchEventType::RescanRequired, {}});
            rescan_pending_ = true;
        }
    }

    [[nodiscard]] std::vector<FileWatchEvent> Drain() {
        std::scoped_lock lock(mutex_);
        std::vector<FileWatchEvent> result;
        result.reserve(events_.size());
        for (auto& event : events_) {
            result.push_back(std::move(event));
        }
        events_.clear();
        rescan_pending_ = false;
        return result;
    }

    std::atomic_bool running{false};

private:
    std::mutex mutex_;
    std::vector<FileWatchEvent> events_;
    std::size_t capacity_;
    bool rescan_pending_{false};
};

class FileWatcherBackend {
public:
    virtual ~FileWatcherBackend() = default;
    virtual void Stop() noexcept = 0;
};

[[nodiscard]] Result<scope<FileWatcherBackend>> CreateFileWatcherBackend(const std::filesystem::path& root, FileWatcherState& state);

} // namespace woki::detail
