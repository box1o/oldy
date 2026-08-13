#include "internal.hpp"

namespace woki::gfx::graph_compile_detail {

Result<void> BuildSchedule(State& state) {
    auto& definition = state.definition;
    const u32 width = state.width;
    const u32 height = state.height;
    auto& edges = state.edges;
    auto& reverse = state.reverse;
    auto& retained = state.retained;
    auto& schedule = state.schedule;
    auto& out = state.out;
    std::vector<u32> indegree(definition->passes.size());
    std::vector<std::vector<u32>> outgoing(definition->passes.size());
    for (const auto& [edge, reason] : edges) {
        (void)reason;
        if (retained[edge.before] && retained[edge.after]) {
            ++indegree[edge.after];
            outgoing[edge.before].push_back(edge.after);
        }
    }
    std::priority_queue<u32, std::vector<u32>, std::greater<>> ready;
    for (u32 pass = 0; pass < retained.size(); ++pass)
        if (retained[pass] && indegree[pass] == 0)
            ready.push(pass);
    while (!ready.empty()) {
        const u32 pass = ready.top();
        ready.pop();
        schedule.push_back(pass);
        std::ranges::sort(outgoing[pass]);
        for (const u32 next : outgoing[pass])
            if (--indegree[next] == 0)
                ready.push(next);
    }
    const size_t retained_count = static_cast<size_t>(std::ranges::count(retained, true));
    if (schedule.size() != retained_count) {
        std::vector<std::string> chain;
        for (u32 pass = 0; pass < retained.size(); ++pass)
            if (retained[pass] && indegree[pass] != 0)
                chain.push_back(definition->passes[pass].name);
        std::string message = "cycle:";
        for (const auto& name : chain)
            message += " " + name;
        return Failure("GRF1016", message);
    }

    out.definition = std::move(definition);
    out.width = width;
    out.height = height;
    out.schedule = schedule;
    out.retained_passes = retained;
    out.retained_resources.resize(out.definition->resources.size());
    out.resource_slots.assign(out.definition->resources.size(), kInvalidGraphIndex);
    out.extents.reserve(out.definition->resources.size());
    for (const auto& resource : out.definition->resources)
        out.extents.push_back(resource.type == graph_detail::ResourceType::Texture ? graph_detail::ResolveExtent(resource.texture, width, height) : rhi::Extent3D{});

    std::vector<u32> levels(out.definition->passes.size());
    std::vector<u32> schedule_position(out.definition->passes.size(), kInvalidGraphIndex);
    for (u32 position = 0; position < schedule.size(); ++position) {
        const u32 pass = schedule[position];
        schedule_position[pass] = position;
        for (const u32 previous : reverse[pass])
            if (retained[previous])
                levels[pass] = std::max(levels[pass], levels[previous] + 1);
        if (out.waves.size() <= levels[pass])
            out.waves.resize(levels[pass] + 1);
        out.waves[levels[pass]].push_back(pass);
        out.passes.push_back({out.definition->passes[pass].name, out.definition->passes[pass].kind, AssignQueue(out.definition->passes[pass]), pass, position, levels[pass]});
    }
    for (const auto& [edge, reason] : edges)
        if (retained[edge.before] && retained[edge.after])
            out.dependencies.push_back({edge.before, edge.after, reason});
    return Ok();
}

} // namespace woki::gfx::graph_compile_detail

namespace woki::gfx {

std::span<const CompiledPassInfo> CompiledRenderGraph::Passes() const noexcept {
    return impl_->passes;
}

std::span<const std::vector<u32>> CompiledRenderGraph::Waves() const noexcept {
    return impl_->waves;
}

} // namespace woki::gfx
