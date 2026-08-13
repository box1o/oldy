#include "internal.hpp"
#include <sstream>

namespace woki::gfx {

namespace {

[[nodiscard]] std::string Escape(const std::string_view value) {
    std::string out;
    constexpr char hex[] = "0123456789abcdef";
    for (const char character : value) {
        const auto c = static_cast<unsigned char>(character);
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (c < 0x20) {
                    out += "\\u00";
                    out.push_back(hex[c >> 4]);
                    out.push_back(hex[c & 0xf]);
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    return out;
}

[[nodiscard]] std::string EscapeMermaid(const std::string_view value) {
    std::string out;
    for (const char c : value) {
        if (c == '"')
            out += "&quot;";
        else if (c == '\n' || c == '\r')
            out += "<br/>";
        else if (c == '&')
            out += "&amp;";
        else if (static_cast<unsigned char>(c) >= 0x20)
            out.push_back(c);
    }
    return out;
}

} // namespace

std::string CompiledRenderGraph::DumpJson() const {
    std::ostringstream out;
    out << "{\"hash\":" << impl_->hash << ",\"peakBytes\":" << impl_->peak_bytes << ",\"passes\":[";
    for (size_t i = 0; i < impl_->passes.size(); ++i) {
        const auto& pass = impl_->passes[i];
        if (i)
            out << ',';
        out << "{\"name\":\"" << Escape(pass.name) << "\",\"declaration\":" << pass.declaration_index << ",\"schedule\":" << pass.schedule_index << ",\"queue\":" << static_cast<u32>(pass.queue)
            << ",\"wave\":" << pass.wave << ",\"uses\":[";
        const auto& declaration = impl_->definition->passes[pass.declaration_index];
        for (size_t use_index = 0; use_index < declaration.uses.size(); ++use_index) {
            const auto& use = declaration.uses[use_index];
            const auto& version = impl_->definition->versions[use.version];
            if (use_index)
                out << ',';
            out << "{\"version\":" << use.version << ",\"resource\":" << version.resource << ",\"access\":" << static_cast<u32>(use.access) << ",\"write\":" << use.write;
            if (version.type == graph_detail::ResourceType::Texture) {
                const auto range = graph_detail::NormalizeRange(impl_->definition->resources[version.resource].texture, use.subresources);
                out << ",\"mip\":[" << range.base_mip_level << ',' << range.mip_level_count << "],\"layer\":[" << range.base_array_layer << ',' << range.array_layer_count
                    << "],\"aspect\":" << static_cast<u32>(range.aspect) << ",\"viewFormat\":" << static_cast<u32>(use.view.format) << ",\"viewDimension\":" << static_cast<u32>(use.view.dimension);
            }
            out << '}';
        }
        out << "]}";
    }
    out << "],\"diagnostics\":[";
    for (size_t i = 0; i < impl_->diagnostics.size(); ++i) {
        const auto& diagnostic = impl_->diagnostics[i];
        if (i)
            out << ',';
        out << "{\"code\":\"" << Escape(diagnostic.code) << "\",\"stage\":\"" << Escape(diagnostic.stage) << "\",\"message\":\"" << Escape(diagnostic.message) << "\",\"pass\":\"" << Escape(diagnostic.pass)
            << "\",\"resource\":\"" << Escape(diagnostic.resource) << "\"}";
    }
    out << "],\"culledPasses\":[";
    bool comma = false;
    for (u32 pass = 0; pass < impl_->definition->passes.size(); ++pass) {
        if (impl_->retained_passes[pass])
            continue;
        if (comma)
            out << ',';
        comma = true;
        out << '"' << Escape(impl_->definition->passes[pass].name) << '"';
    }
    out << "],\"resources\":[";
    for (u32 resource = 0; resource < impl_->definition->resources.size(); ++resource) {
        if (resource)
            out << ',';
        const auto& value = impl_->definition->resources[resource];
        const std::string& label = value.type == graph_detail::ResourceType::Texture ? value.texture.label : value.buffer.label;
        out << "{\"index\":" << resource << ",\"name\":\"" << Escape(label) << "\",\"type\":" << static_cast<u32>(value.type) << ",\"lifetime\":" << static_cast<u32>(value.lifetime)
            << ",\"retained\":" << (impl_->retained_resources[resource] ? "true" : "false") << ",\"slot\":" << impl_->resource_slots[resource] << ",\"versions\":[";
        bool version_comma = false;
        for (u32 version = 0; version < impl_->definition->versions.size(); ++version) {
            if (impl_->definition->versions[version].resource != resource)
                continue;
            if (version_comma)
                out << ',';
            version_comma = true;
            out << version;
        }
        out << "]}";
    }
    out << "],\"dependencies\":[";
    for (size_t i = 0; i < impl_->dependencies.size(); ++i) {
        const auto& edge = impl_->dependencies[i];
        if (i)
            out << ',';
        out << "{\"before\":" << edge.before << ",\"after\":" << edge.after << ",\"reason\":\"" << Escape(edge.reason) << "\"}";
    }
    out << "],\"lifetimes\":[";
    for (size_t i = 0; i < impl_->lifetimes.size(); ++i) {
        const auto& life = impl_->lifetimes[i];
        if (i)
            out << ',';
        out << "{\"version\":" << life.version << ",\"first\":" << life.first_use << ",\"last\":" << life.last_use << ",\"slot\":" << life.physical_slot << '}';
    }
    out << "],\"transitions\":[";
    for (size_t i = 0; i < impl_->transitions.size(); ++i) {
        const auto& transition = impl_->transitions[i];
        if (i)
            out << ',';
        out << "{\"version\":" << transition.version << ",\"beforePass\":" << transition.before_pass << ",\"pass\":" << transition.pass << ",\"from\":" << static_cast<u32>(transition.before)
            << ",\"to\":" << static_cast<u32>(transition.after) << ",\"sourceQueue\":" << static_cast<u32>(transition.source_queue) << ",\"destinationQueue\":" << static_cast<u32>(transition.destination_queue)
            << ",\"externalState\":" << static_cast<u32>(transition.external_state) << ",\"acquire\":" << transition.external_acquire << ",\"release\":" << transition.external_release
            << ",\"writeBarrier\":" << transition.write_barrier << ",\"queueDependency\":" << transition.queue_dependency << '}';
    }
    out << "],\"waves\":[";
    for (size_t wave = 0; wave < impl_->waves.size(); ++wave) {
        if (wave)
            out << ',';
        out << '[';
        for (size_t pass = 0; pass < impl_->waves[wave].size(); ++pass) {
            if (pass)
                out << ',';
            out << impl_->waves[wave][pass];
        }
        out << ']';
    }
    out << "]}";
    return out.str();
}

std::string CompiledRenderGraph::DumpDot() const {
    std::ostringstream out;
    out << "digraph RenderGraph {\n";
    for (const auto& pass : impl_->passes)
        out << "  p" << pass.declaration_index << " [label=\"" << Escape(pass.name) << "\\nq" << static_cast<u32>(pass.queue) << " w" << pass.wave << "\"];\n";
    for (u32 pass = 0; pass < impl_->definition->passes.size(); ++pass)
        if (!impl_->retained_passes[pass])
            out << "  p" << pass << " [label=\"" << Escape(impl_->definition->passes[pass].name) << "\\nculled\",style=dashed];\n";
    for (const auto& edge : impl_->dependencies)
        out << "  p" << edge.before << " -> p" << edge.after << " [label=\"" << Escape(edge.reason) << "\"];\n";
    for (size_t index = 0; index < impl_->diagnostics.size(); ++index)
        out << "  d" << index << " [shape=note,label=\"" << Escape(impl_->diagnostics[index].code + " " + impl_->diagnostics[index].message) << "\"];\n";
    out << "}\n";
    return out.str();
}

std::string CompiledRenderGraph::DumpMermaid() const {
    std::ostringstream out;
    out << "flowchart TD\n";
    for (const auto& pass : impl_->passes)
        out << "  p" << pass.declaration_index << "[\"" << EscapeMermaid(pass.name) << "\"]\n";
    for (const auto& edge : impl_->dependencies)
        out << "  p" << edge.before << " -->|" << EscapeMermaid(edge.reason) << "| p" << edge.after << '\n';
    for (size_t index = 0; index < impl_->diagnostics.size(); ++index)
        out << "  d" << index << "[\"" << EscapeMermaid(impl_->diagnostics[index].code + " " + impl_->diagnostics[index].message) << "\"]\n";
    return out.str();
}

} // namespace woki::gfx
