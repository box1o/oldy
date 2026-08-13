#include "internal.hpp"

namespace woki::gfx {

std::span<const GraphDependency> CompiledRenderGraph::Dependencies() const noexcept {
    return impl_->dependencies;
}

} // namespace woki::gfx

namespace woki::gfx::graph_compile_detail {

Result<void> AddRetainedDependencies(State& state) {
    auto& definition = state.definition;
    auto& readers = state.readers;
    auto& reverse = state.reverse;
    auto& retained = state.retained;
    auto& edges = state.edges;
    edges = state.data_edges;
    for (u32 pass_index = 0; pass_index < definition->passes.size(); ++pass_index) {
        if (!retained[pass_index])
            continue;
        for (const auto& use : definition->passes[pass_index].uses) {
            if (!use.write)
                continue;
            const auto& version = definition->versions[use.version];
            if (version.previous == kInvalidGraphIndex)
                continue;
            const auto& previous = definition->versions[version.previous];
            const auto& resource = definition->resources[version.resource];
            const auto* write = ProducerUse(*definition, version);
            const auto* previous_write = ProducerUse(*definition, previous);
            if (previous.producer != kInvalidGraphIndex && previous.producer != pass_index && retained[previous.producer]
                && (resource.type != graph_detail::ResourceType::Texture || write == nullptr || previous_write == nullptr || graph_detail::Overlaps(resource.texture, write->subresources, previous_write->subresources)))
                edges.try_emplace(EdgeKey{previous.producer, pass_index}, "WAW resource " + std::to_string(version.resource));
            for (const auto& reader : readers[version.previous])
                if (reader.pass != pass_index && retained[reader.pass]
                    && (resource.type != graph_detail::ResourceType::Texture || write == nullptr || graph_detail::Overlaps(resource.texture, write->subresources, reader.range)))
                    edges.try_emplace(EdgeKey{reader.pass, pass_index}, "WAR resource " + std::to_string(version.resource));
        }
    }
    for (auto& predecessors : reverse)
        predecessors.clear();
    for (const auto& [edge, reason] : edges) {
        (void)reason;
        if (retained[edge.before] && retained[edge.after])
            reverse[edge.after].push_back(edge.before);
    }
    return Ok();
}

} // namespace woki::gfx::graph_compile_detail
