#include <algorithm>

#include <woki/gfx/advanced/reload.hpp>

namespace woki::gfx {

void ShaderDependencyGraph::Replace(
    const ShaderAssetHandle shader,
    const std::span<const asset::AssetPath> dependencies
) {
    auto next_reverse = reverse_;
    auto next_forward = forward_;
    const auto old = next_forward.find(shader);
    if (old != next_forward.end()) {
        for (const auto& dependency : old->second) {
            auto reverse = next_reverse.find(dependency);
            if (reverse != next_reverse.end()) {
                reverse->second.erase(shader);
                if (reverse->second.empty())
                    next_reverse.erase(reverse);
            }
        }
    }
    auto& stored = next_forward[shader];
    stored.assign(dependencies.begin(), dependencies.end());
    std::ranges::sort(stored);
    stored.erase(std::ranges::unique(stored).begin(), stored.end());
    for (const auto& dependency : stored)
        next_reverse[dependency].insert(shader);
    reverse_.swap(next_reverse);
    forward_.swap(next_forward);
}

std::vector<ShaderAssetHandle> ShaderDependencyGraph::Dependents(const asset::AssetPath& dependency) const {
    const auto found = reverse_.find(dependency);
    if (found == reverse_.end())
        return {};
    return {found->second.begin(), found->second.end()};
}

std::vector<ShaderAssetHandle> ShaderDependencyGraph::Shaders() const {
    std::vector<ShaderAssetHandle> result;
    result.reserve(forward_.size());
    for (const auto& [shader, dependencies] : forward_) {
        static_cast<void>(dependencies);
        result.push_back(shader);
    }
    return result;
}

void ShaderReloadCoordinator::Process(const std::span<const FileWatchEvent> events) {
    std::set<ShaderAssetHandle> dirty;
    for (const FileWatchEvent& event : events) {
        if (event.type == FileWatchEventType::RescanRequired) {
            const auto shaders = graph_.Shaders();
            dirty.insert(shaders.begin(), shaders.end());
            continue;
        }
        auto path = asset::AssetPath::Parse(event.path.generic_string());
        if (!path)
            continue;
        const auto dependents = graph_.Dependents(*path);
        dirty.insert(dependents.begin(), dependents.end());
    }
    for (const ShaderAssetHandle handle : dirty) {
        ReloadEvent event = Publish(build_(handle));
        if (callback_)
            callback_(event);
    }
}

ReloadEvent ShaderReloadCoordinator::Publish(ReloadCandidate candidate) {
    const auto current = library_.Record(candidate.handle);
    if (!current || current->version != candidate.base_version)
        return {.handle = candidate.handle,
            .outcome = ReloadOutcome::Stale,
            .version = current ? current->version : 0,
            .message = "reload candidate was built from a stale generation",
            .interface_changed = false,
            .previous_interface = {},
            .current_interface = {}};
    if (!candidate.product)
        return {.handle = candidate.handle,
            .outcome = ReloadOutcome::Rejected,
            .version = current->version,
            .message = std::string(candidate.product.error().Message()),
            .interface_changed = false,
            .previous_interface = {},
            .current_interface = {}};
    ContentHash previous_interface{};
    if (auto previous = library_.Borrow(candidate.handle); previous)
        previous_interface = previous->Interface().hash;
    auto published = library_.Publish(candidate.handle, *candidate.product);
    if (!published)
        return {.handle = candidate.handle,
            .outcome = ReloadOutcome::Rejected,
            .version = current->version,
            .message = std::string(published.error().Message()),
            .interface_changed = false,
            .previous_interface = {},
            .current_interface = {}};
    const auto generation = library_.Borrow(candidate.handle);
    if (!generation)
        return {.handle = candidate.handle,
            .outcome = ReloadOutcome::Rejected,
            .version = current->version,
            .message = std::string(generation.error().Message()),
            .interface_changed = false,
            .previous_interface = {},
            .current_interface = {}};
    graph_.Replace(candidate.handle, generation->Dependencies());
    const ContentHash current_interface = generation->Interface().hash;
    return {candidate.handle,
        ReloadOutcome::Published,
        generation->Version(),
        {},
        previous_interface != ContentHash{} && previous_interface != current_interface,
        previous_interface,
        current_interface};
}

} // namespace woki::gfx
