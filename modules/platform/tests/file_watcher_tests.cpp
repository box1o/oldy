#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <woki/platform/file_watcher.hpp>

namespace {

namespace fs = std::filesystem;
using namespace std::chrono_literals;

class TemporaryDirectory final {
public:
    explicit TemporaryDirectory(std::string_view name)
        : path_(fs::temp_directory_path() / ("woki_file_watcher_" + std::string(name))) {
        std::error_code error;
        fs::remove_all(path_, error);
        fs::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        fs::remove_all(path_, error);
    }

    [[nodiscard]] const fs::path& Path() const noexcept {
        return path_;
    }

private:
    fs::path path_;
};

template <typename Predicate>
std::vector<woki::FileWatchEvent> WaitFor(woki::FileWatcher& watcher, Predicate predicate) {
    std::vector<woki::FileWatchEvent> collected;
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (std::chrono::steady_clock::now() < deadline) {
        auto events = watcher.Drain();
        collected.insert(collected.end(), events.begin(), events.end());
        if (predicate(collected)) {
            break;
        }
        std::this_thread::sleep_for(10ms);
    }
    return collected;
}

bool Contains(const std::vector<woki::FileWatchEvent>& events, woki::FileWatchEventType type, const fs::path& path) {
    return std::ranges::any_of(events, [&](const auto& event) { return event.type == type && event.path == path; });
}

bool Invalidates(const std::vector<woki::FileWatchEvent>& events, const fs::path& path) {
    return std::ranges::any_of(events, [&](const auto& event) { return event.path == path; }) || Contains(events, woki::FileWatchEventType::RescanRequired, {});
}

std::vector<woki::FileWatchEvent> WaitForRescan(woki::FileWatcher& watcher) {
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (std::chrono::steady_clock::now() < deadline) {
        auto events = watcher.Drain();
        if (Contains(events, woki::FileWatchEventType::RescanRequired, {})) {
            return events;
        }
        std::this_thread::sleep_for(10ms);
    }
    return {};
}

} // namespace

TEST_CASE("File watcher rejects missing roots") {
    TemporaryDirectory temporary("missing");
    const auto missing = temporary.Path() / "does-not-exist";

    const auto watcher = woki::FileWatcher::Create(missing);

    REQUIRE_FALSE(watcher);
    CHECK(watcher.error().Code() == woki::ErrorCode::FileNotFound);
    CHECK_FALSE(fs::exists(missing));
}

TEST_CASE("File watcher reports recursive file changes with relative paths") {
    TemporaryDirectory temporary("recursive");
    fs::create_directory(temporary.Path() / "nested");
    {
        std::ofstream modified(temporary.Path() / "nested" / "modified.txt");
        modified << "first";
        std::ofstream removed(temporary.Path() / "nested" / "removed.txt");
        removed << "remove me";
    }
    auto watcher_result = woki::FileWatcher::Create(temporary.Path());
    REQUIRE(watcher_result);
    auto watcher = std::move(*watcher_result);
    REQUIRE(watcher->IsRunning());
    CHECK(watcher->Root().is_absolute());
    CHECK(watcher->Drain().empty());

    {
        std::ofstream file(temporary.Path() / "nested" / "added.txt");
        file << "first";
    }
    auto events = WaitFor(*watcher, [](const auto& current) { return Invalidates(current, "nested/added.txt"); });
    CHECK(Invalidates(events, "nested/added.txt"));

    {
        std::ofstream file(temporary.Path() / "nested" / "modified.txt", std::ios::app);
        file << "second";
    }
    events = WaitFor(*watcher, [](const auto& current) { return Invalidates(current, "nested/modified.txt"); });
    CHECK(Invalidates(events, "nested/modified.txt"));

    fs::remove(temporary.Path() / "nested" / "removed.txt");
    events = WaitFor(*watcher, [](const auto& current) { return Invalidates(current, "nested/removed.txt"); });
    CHECK(Invalidates(events, "nested/removed.txt"));

    watcher->Stop();
    CHECK_FALSE(watcher->IsRunning());
    watcher->Stop();
}

TEST_CASE("File watcher queue overflow collapses to rescan") {
    TemporaryDirectory temporary("overflow");
    auto watcher_result = woki::FileWatcher::Create(temporary.Path(), {.queue_capacity = 1});
    REQUIRE(watcher_result);
    auto watcher = std::move(*watcher_result);

    {
        std::ofstream first(temporary.Path() / "first.txt");
        first << "first";
    }
    {
        std::ofstream second(temporary.Path() / "second.txt");
        second << "second";
    }

    const auto events = WaitForRescan(*watcher);
    REQUIRE(events.size() == 1);
    CHECK(events.front().type == woki::FileWatchEventType::RescanRequired);
    CHECK(events.front().path.empty());
}
