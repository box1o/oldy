#pragma once

#include <functional>
#include <map>
#include <set>

#include <woki/platform.hpp>

#include "library.hpp"

namespace woki::gfx {

class ShaderDependencyGraph {
public:
    void Replace(ShaderAssetHandle shader, std::span<const asset::AssetPath> dependencies);
    [[nodiscard]] std::vector<ShaderAssetHandle> Dependents(const asset::AssetPath& dependency) const;
    [[nodiscard]] std::vector<ShaderAssetHandle> Shaders() const;

private:
    std::map<asset::AssetPath, std::set<ShaderAssetHandle>> reverse_;
    std::map<ShaderAssetHandle, std::vector<asset::AssetPath>> forward_;
};

struct ReloadCandidate {
    ShaderAssetHandle handle;
    u64 base_version{0};
    Result<asset::Product> product;
};

enum class ReloadOutcome : u8 { Published, Rejected, Stale };

struct ReloadEvent {
    ShaderAssetHandle handle;
    ReloadOutcome outcome{ReloadOutcome::Rejected};
    u64 version{0};
    std::string message;
};

class ShaderReloadCoordinator {
public:
    using Build = std::function<ReloadCandidate(ShaderAssetHandle)>;
    using Callback = std::function<void(const ReloadEvent&)>;

    ShaderReloadCoordinator(ShaderLibrary& library, ShaderDependencyGraph& graph, Build build, Callback callback)
        : library_(library),
          graph_(graph),
          build_(std::move(build)),
          callback_(std::move(callback)) {}

    void Process(std::span<const FileWatchEvent> events);
    [[nodiscard]] ReloadEvent Publish(ReloadCandidate candidate);

private:
    ShaderLibrary& library_;
    ShaderDependencyGraph& graph_;
    Build build_;
    Callback callback_;
};

} // namespace woki::gfx
