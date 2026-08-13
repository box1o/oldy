#pragma once

// IWYU pragma: private, include "woki/asset.hpp"

#include <chrono>
#include <map>
#include <span>

#include "database.hpp"
#include "dependency_graph.hpp"
#include "manager.hpp"

namespace woki::asset {

enum class ReloadHintType : u8 { Changed, Removed, RescanRequired };

struct ReloadHint {
    ReloadHintType type{ReloadHintType::Changed};
    AssetUri uri;
};

class ReloadCoordinator {
public:
    ReloadCoordinator(const Vfs& vfs, AssetDatabase& database, DependencyGraph& dependencies, AssetManager& manager, std::chrono::milliseconds debounce = std::chrono::milliseconds(50))
        : vfs_(vfs),
          database_(database),
          dependencies_(dependencies),
          manager_(manager),
          debounce_(debounce) {}

    void Push(std::span<const ReloadHint> hints);
    [[nodiscard]] Result<std::vector<AssetId>> Reconcile(AssetScheme scheme, std::string_view authority = {});
    [[nodiscard]] Result<std::vector<AssetId>> Pump();

private:
    const Vfs& vfs_;
    AssetDatabase& database_;
    DependencyGraph& dependencies_;
    AssetManager& manager_;
    std::chrono::milliseconds debounce_;
    std::map<AssetUri, std::pair<ReloadHintType, std::chrono::steady_clock::time_point>> pending_;
};

} // namespace woki::asset
