#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

#include <woki/asset.hpp>

namespace woki::asset::test {

class TempDirectory {
public:
    TempDirectory() {
        static std::atomic_uint sequence{};
        path_ = std::filesystem::temp_directory_path() / ("woki-asset-focused-" + std::to_string(++sequence));
        std::filesystem::create_directories(path_);
    }

    ~TempDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& Path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

inline Product ProductFor(const AssetId id, std::string_view payload = "payload", std::vector<ProductDependency> dependencies = {}) {
    std::vector<std::byte> bytes;
    bytes.reserve(payload.size());
    for (const char value : payload)
        bytes.push_back(static_cast<std::byte>(value));
    return MakeProduct(id, 17, 2, 3, Sha256("source"), std::move(dependencies), Sha256("target"), std::move(bytes));
}

template <typename T>
bool PumpUntil(AssetManager& manager, const task::Future<T>& future, std::chrono::milliseconds timeout = std::chrono::seconds(2)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!future.IsReady() && std::chrono::steady_clock::now() < deadline) {
        (void)manager.PumpPublications(8);
        std::this_thread::yield();
    }
    (void)manager.PumpPublications();
    return future.IsReady();
}

} // namespace woki::asset::test
