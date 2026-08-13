#include "internal.hpp"

namespace woki::gfx::graph_compile_detail {

void BuildTransitions(State& state) {
    auto& out = state.out;
    auto& schedule = state.schedule;

    struct PreviousAccess final {
        u32 pass{};
        GraphAccess access{};
        QueueClass queue{};
        bool write{};
        u32 version{};
    };

    std::map<u32, PreviousAccess> previous_access;
    for (u32 position = 0; position < schedule.size(); ++position) {
        const u32 pass = schedule[position];
        const QueueClass queue = AssignQueue(out.definition->passes[pass]);
        for (const auto& use : out.definition->passes[pass].uses) {
            const u32 resource = out.definition->versions[use.version].resource;
            if (const auto found = previous_access.find(resource); found != previous_access.end()) {
                const bool write_barrier = found->second.write || use.write;
                const bool queue_dependency = found->second.queue != queue;
                if (found->second.access != use.access || write_barrier || queue_dependency)
                    out.transitions
                        .push_back({use.version, found->second.pass, pass, found->second.access, use.access, found->second.queue, queue, ExternalState::Undefined, false, false, write_barrier, queue_dependency});
            } else {
                const auto& value = out.definition->resources[resource];
                if (value.external && value.initial_state != ExternalState::Undefined)
                    out.transitions.push_back({use.version, kInvalidGraphIndex, pass, use.access, use.access, queue, queue, value.initial_state, true, false, use.write, false});
            }
            previous_access[resource] = {pass, use.access, queue, use.write, use.version};
        }
    }
    for (const auto& [resource, access] : previous_access) {
        const auto& value = out.definition->resources[resource];
        if (value.final_state != ExternalState::Undefined)
            out.transitions.push_back({access.version, access.pass, kInvalidGraphIndex, access.access, access.access, access.queue, access.queue, value.final_state, false, true, access.write, false});
    }
}

void BuildCanonicalHash(State& state) {
    auto& out = state.out;
    const u32 width = state.width;
    const u32 height = state.height;
    out.hash = 1469598103934665603ull;
    HashValue(out.hash, width);
    HashValue(out.hash, height);
    HashValue(out.hash, out.definition->resources.size());
    for (const auto& pass : out.passes) {
        HashString(out.hash, pass.name);
        HashValue(out.hash, pass.kind);
        HashValue(out.hash, pass.queue);
        HashValue(out.hash, pass.wave);
    }
    for (const auto& edge : out.dependencies) {
        HashValue(out.hash, edge.before);
        HashValue(out.hash, edge.after);
        HashString(out.hash, edge.reason);
    }
    for (u32 resource = 0; resource < out.definition->resources.size(); ++resource) {
        const auto& value = out.definition->resources[resource];
        HashValue(out.hash, value.type);
        HashValue(out.hash, value.lifetime);
        HashValue(out.hash, value.initial_state);
        HashValue(out.hash, value.final_state);
        HashValue(out.hash, value.frame_bound);
        HashValue(out.hash, value.external);
        const bool retained_resource = out.retained_resources[resource];
        HashValue(out.hash, retained_resource);
        HashValue(out.hash, out.resource_slots[resource]);
        if (value.type == graph_detail::ResourceType::Texture) {
            HashString(out.hash, value.texture.label);
            HashValue(out.hash, value.texture.format);
            HashValue(out.hash, value.texture.usage);
            HashValue(out.hash, value.texture.extent.kind);
            HashValue(out.hash, value.texture.extent.width);
            HashValue(out.hash, value.texture.extent.height);
            HashValue(out.hash, value.texture.extent.scale_x);
            HashValue(out.hash, value.texture.extent.scale_y);
            HashValue(out.hash, out.extents[resource].width);
            HashValue(out.hash, out.extents[resource].height);
            HashValue(out.hash, out.extents[resource].depth_or_array_layers);
            HashValue(out.hash, value.texture.mip_levels);
            HashValue(out.hash, value.texture.sample_count);
            HashValue(out.hash, value.texture.dimension);
            HashValue(out.hash, value.texture.view_formats.size());
            for (const auto format : value.texture.view_formats)
                HashValue(out.hash, format);
            HashValue(out.hash, value.texture.view_aspect);
        } else {
            HashString(out.hash, value.buffer.label);
            HashValue(out.hash, value.buffer.size);
            HashValue(out.hash, value.buffer.alignment);
            HashValue(out.hash, value.buffer.usage);
        }
    }
    HashValue(out.hash, out.definition->versions.size());
    for (const auto& version : out.definition->versions) {
        HashValue(out.hash, version.type);
        HashValue(out.hash, version.resource);
        HashValue(out.hash, version.ordinal);
        HashValue(out.hash, version.producer);
        HashValue(out.hash, version.previous);
    }
    HashValue(out.hash, out.definition->passes.size());
    for (u32 pass_index = 0; pass_index < out.definition->passes.size(); ++pass_index) {
        const auto& pass = out.definition->passes[pass_index];
        HashValue(out.hash, pass_index);
        HashString(out.hash, pass.name);
        HashValue(out.hash, pass.kind);
        HashValue(out.hash, pass.preference);
        HashValue(out.hash, pass.async_compute);
        HashValue(out.hash, pass.side_effect.has_value());
        if (pass.side_effect)
            HashString(out.hash, *pass.side_effect);
        HashValue(out.hash, pass.uses.size());
        for (const auto& use : pass.uses) {
            HashValue(out.hash, use.version);
            HashValue(out.hash, use.access);
            HashValue(out.hash, use.write);
            HashValue(out.hash, use.subresources.base_mip_level);
            HashValue(out.hash, use.subresources.mip_level_count);
            HashValue(out.hash, use.subresources.base_array_layer);
            HashValue(out.hash, use.subresources.array_layer_count);
            HashValue(out.hash, use.subresources.aspect);
            HashValue(out.hash, use.view.format);
            HashValue(out.hash, use.view.dimension);
            HashValue(out.hash, use.view.usage);
            HashString(out.hash, use.view.label);
        }
        HashValue(out.hash, pass.colors.size());
        for (const auto& color : pass.colors) {
            HashValue(out.hash, color.version);
            HashValue(out.hash, color.attachment.slot);
            HashValue(out.hash, color.attachment.load);
            HashValue(out.hash, color.attachment.store);
            HashValue(out.hash, color.attachment.clear.r);
            HashValue(out.hash, color.attachment.clear.g);
            HashValue(out.hash, color.attachment.clear.b);
            HashValue(out.hash, color.attachment.clear.a);
            HashValue(out.hash, color.attachment.region.viewport_origin.has_value());
            if (color.attachment.region.viewport_origin) {
                HashValue(out.hash, color.attachment.region.viewport_origin->x);
                HashValue(out.hash, color.attachment.region.viewport_origin->y);
            }
            HashValue(out.hash, color.attachment.region.viewport_extent.has_value());
            if (color.attachment.region.viewport_extent) {
                HashValue(out.hash, color.attachment.region.viewport_extent->width);
                HashValue(out.hash, color.attachment.region.viewport_extent->height);
            }
            HashValue(out.hash, color.attachment.region.scissor_origin.has_value());
            if (color.attachment.region.scissor_origin) {
                HashValue(out.hash, color.attachment.region.scissor_origin->x);
                HashValue(out.hash, color.attachment.region.scissor_origin->y);
            }
            HashValue(out.hash, color.attachment.region.scissor_extent.has_value());
            if (color.attachment.region.scissor_extent) {
                HashValue(out.hash, color.attachment.region.scissor_extent->width);
                HashValue(out.hash, color.attachment.region.scissor_extent->height);
            }
            const u32 resolve = color.attachment.resolve ? color.attachment.resolve->Index() : kInvalidGraphIndex;
            HashValue(out.hash, resolve);
        }
        HashValue(out.hash, pass.depth.has_value());
        if (pass.depth) {
            HashValue(out.hash, pass.depth->version);
            HashValue(out.hash, pass.depth->attachment.load);
            HashValue(out.hash, pass.depth->attachment.store);
            HashValue(out.hash, pass.depth->attachment.clear);
            HashValue(out.hash, pass.depth->attachment.read_only);
            HashValue(out.hash, pass.depth->attachment.region.view.subresources.base_mip_level);
            HashValue(out.hash, pass.depth->attachment.region.view.subresources.base_array_layer);
            HashValue(out.hash, pass.depth->attachment.region.viewport_extent.has_value());
            if (pass.depth->attachment.region.viewport_extent) {
                HashValue(out.hash, pass.depth->attachment.region.viewport_extent->width);
                HashValue(out.hash, pass.depth->attachment.region.viewport_extent->height);
            }
        }
        HashValue(out.hash, pass.copies.size());
        for (const auto& copy : pass.copies) {
            HashValue(out.hash, copy.source);
            HashValue(out.hash, copy.destination);
            HashValue(out.hash, copy.size);
        }
        HashValue(out.hash, pass.explicit_dependencies.size());
        for (const u32 dependency : pass.explicit_dependencies)
            HashValue(out.hash, dependency);
    }
    HashValue(out.hash, out.definition->exports.size());
    for (const u32 version : out.definition->exports)
        HashValue(out.hash, version);
    for (const auto& transition : out.transitions) {
        HashValue(out.hash, transition.version);
        HashValue(out.hash, transition.before_pass);
        HashValue(out.hash, transition.pass);
        HashValue(out.hash, transition.before);
        HashValue(out.hash, transition.after);
        HashValue(out.hash, transition.source_queue);
        HashValue(out.hash, transition.destination_queue);
        HashValue(out.hash, transition.external_state);
        HashValue(out.hash, transition.external_acquire);
        HashValue(out.hash, transition.external_release);
        HashValue(out.hash, transition.write_barrier);
        HashValue(out.hash, transition.queue_dependency);
    }
}

} // namespace woki::gfx::graph_compile_detail

namespace woki::gfx {

std::span<const GraphTransition> CompiledRenderGraph::Transitions() const noexcept {
    return impl_->transitions;
}

} // namespace woki::gfx
