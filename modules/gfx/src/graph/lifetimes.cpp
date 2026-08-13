#include "internal.hpp"

namespace woki::gfx::graph_compile_detail {

void BuildLifetimes(State& state) {
    auto& out = state.out;
    auto& schedule = state.schedule;
    std::vector<u32> first(out.definition->versions.size(), kInvalidGraphIndex);
    std::vector<u32> last(out.definition->versions.size());
    std::vector<u32> resource_first(out.definition->resources.size(), kInvalidGraphIndex);
    std::vector<u32> resource_last(out.definition->resources.size());
    for (u32 position = 0; position < schedule.size(); ++position) {
        const u32 pass = schedule[position];
        for (const auto& use : out.definition->passes[pass].uses) {
            first[use.version] = std::min(first[use.version], position);
            last[use.version] = std::max(last[use.version], position);
            const u32 resource = out.definition->versions[use.version].resource;
            resource_first[resource] = std::min(resource_first[resource], position);
            resource_last[resource] = std::max(resource_last[resource], position);
            out.retained_resources[resource] = true;
            if (out.definition->resources[resource].type == graph_detail::ResourceType::Texture)
                out.definition->resources[resource].texture.usage = out.definition->resources[resource].texture.usage | graph_detail::TextureUsageFor(use.access);
            else
                out.definition->resources[resource].buffer.usage = out.definition->resources[resource].buffer.usage | graph_detail::BufferUsageFor(use.access);
        }
    }

    struct Slot final {
        u32 resource{};
        u32 last{};
        u32 index{};
    };

    std::vector<Slot> texture_slots;
    std::vector<Slot> buffer_slots;
    auto texture_compatible = [&](const u32 lhs, const u32 rhs) {
        const auto& a = out.definition->resources[lhs].texture;
        const auto& b = out.definition->resources[rhs].texture;
        return out.extents[lhs].width == out.extents[rhs].width && out.extents[lhs].height == out.extents[rhs].height && out.extents[lhs].depth_or_array_layers == out.extents[rhs].depth_or_array_layers
               && a.format == b.format && a.usage == b.usage && a.mip_levels == b.mip_levels && a.sample_count == b.sample_count && a.dimension == b.dimension && a.view_formats == b.view_formats;
    };
    for (u32 resource = 0; resource < out.definition->resources.size(); ++resource) {
        if (!out.retained_resources[resource] || out.definition->resources[resource].lifetime != ResourceLifetime::Transient)
            continue;
        auto& slots = out.definition->resources[resource].type == graph_detail::ResourceType::Texture ? texture_slots : buffer_slots;
        auto found = slots.end();
        for (auto it = slots.begin(); it != slots.end(); ++it) {
            bool compatible{};
            if (out.definition->resources[resource].type == graph_detail::ResourceType::Texture)
                compatible = texture_compatible(it->resource, resource);
            else {
                const auto& a = out.definition->resources[it->resource].buffer;
                const auto& b = out.definition->resources[resource].buffer;
                compatible = a.size == b.size && a.alignment == b.alignment && a.usage == b.usage;
            }
            if (it->last < resource_first[resource] && compatible) {
                found = it;
                break;
            }
        }
        if (found == slots.end()) {
            const u32 index = static_cast<u32>(slots.size());
            slots.push_back({resource, resource_last[resource], index});
            out.resource_slots[resource] = index;
        } else {
            found->resource = resource;
            found->last = resource_last[resource];
            out.resource_slots[resource] = found->index;
        }
    }
    out.texture_slot_count = static_cast<u32>(texture_slots.size());
    out.buffer_slot_count = static_cast<u32>(buffer_slots.size());
    for (u32 resource = 0; resource < out.definition->resources.size(); ++resource) {
        const auto& value = out.definition->resources[resource];
        if (!out.retained_resources[resource] || value.external || value.lifetime == ResourceLifetime::Transient)
            continue;
        if (value.type == graph_detail::ResourceType::Texture)
            out.resource_slots[resource] = out.texture_slot_count++;
        else
            out.resource_slots[resource] = out.buffer_slot_count++;
    }
    for (u32 version = 0; version < out.definition->versions.size(); ++version) {
        if (first[version] == kInvalidGraphIndex)
            continue;
        const u32 resource = out.definition->versions[version].resource;
        out.lifetimes.push_back({version, first[version], last[version], out.resource_slots[resource]});
    }

    for (u32 position = 0; position < schedule.size(); ++position) {
        u64 live{};
        std::set<std::pair<graph_detail::ResourceType, u32>> counted;
        for (u32 resource = 0; resource < out.definition->resources.size(); ++resource) {
            if (out.definition->resources[resource].lifetime != ResourceLifetime::Transient || resource_first[resource] == kInvalidGraphIndex || position < resource_first[resource] || position > resource_last[resource])
                continue;
            const auto key = std::pair{out.definition->resources[resource].type, out.resource_slots[resource]};
            if (!counted.insert(key).second)
                continue;
            live += key.first == graph_detail::ResourceType::Texture ? graph_detail::TextureBytes(out.definition->resources[resource].texture, out.extents[resource]) : out.definition->resources[resource].buffer.size;
        }
        out.peak_bytes = std::max(out.peak_bytes, live);
    }
}

} // namespace woki::gfx::graph_compile_detail

namespace woki::gfx {

std::span<const GraphLifetimeInterval> CompiledRenderGraph::Lifetimes() const noexcept {
    return impl_->lifetimes;
}

} // namespace woki::gfx
