#pragma once

#include <vector>
#include <cstddef>
#include <filesystem>

#include <woki/core.hpp>

namespace woki {

enum class FileWatchEventType : u8 { Added, Modified, Removed, RescanRequired };

struct FileWatchEvent {
    FileWatchEventType type{FileWatchEventType::Modified};
    std::filesystem::path path;
};

struct FileWatcherOptions {
    std::size_t queue_capacity{1024};
};

class FileWatcher final {
public:
    ~FileWatcher();

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;
    FileWatcher(FileWatcher&&) = delete;
    FileWatcher& operator=(FileWatcher&&) = delete;

    [[nodiscard]] static Result<scope<FileWatcher>> Create(const std::filesystem::path& root, const FileWatcherOptions& options = {});

    [[nodiscard]] std::vector<FileWatchEvent> Drain();
    [[nodiscard]] const std::filesystem::path& Root() const noexcept;
    [[nodiscard]] bool IsRunning() const noexcept;
    void Stop() noexcept;

private:
    struct Impl;

    explicit FileWatcher(scope<Impl> impl) noexcept;
    scope<Impl> impl_;
};

} // namespace woki
