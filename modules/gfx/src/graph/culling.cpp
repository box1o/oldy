#include "internal.hpp"

namespace woki::gfx::graph_compile_detail {

Result<void> Cull(State& state) {
    auto& definition = state.definition;
    auto& data_edges = state.data_edges;
    auto& reverse = state.reverse;
    auto& retained = state.retained;
    reverse.assign(definition->passes.size(), {});
    retained.assign(definition->passes.size(), false);
    for (const auto& [edge, reason] : data_edges) {
        (void)reason;
        reverse[edge.after].push_back(edge.before);
    }
    std::vector<u32> stack;
    for (u32 pass = 0; pass < definition->passes.size(); ++pass) {
        bool root = definition->passes[pass].side_effect.has_value();
        for (const auto& use : definition->passes[pass].uses) {
            if (!use.write)
                continue;
            const auto& resource = definition->resources[definition->versions[use.version].resource];
            const bool published_lifetime = resource.lifetime == ResourceLifetime::Presentation || resource.lifetime == ResourceLifetime::Readback || resource.lifetime == ResourceLifetime::Persistent
                                            || resource.lifetime == ResourceLifetime::Temporal;
            root = root || definition->exports.contains(use.version) || (published_lifetime && resource.current_version == use.version);
        }
        if (root)
            stack.push_back(pass);
    }

    while (!stack.empty()) {
        const u32 pass = stack.back();
        stack.pop_back();
        if (retained[pass])
            continue;
        retained[pass] = true;
        stack.insert(stack.end(), reverse[pass].begin(), reverse[pass].end());
    }
    return Ok();
}

} // namespace woki::gfx::graph_compile_detail
