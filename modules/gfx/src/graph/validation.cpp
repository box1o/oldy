#include "internal.hpp"

namespace woki::gfx {

[[nodiscard]] std::unexpected<Error> graph_compile_detail::Failure(std::string code, std::string message) {
    return Err(ErrorCode::ValidationInvalidState, code + " " + message);
}

void graph_compile_detail::AddDiagnostic(std::vector<GraphDiagnostic>& diagnostics, std::string code, std::string message, std::string stage, std::string pass, std::string resource) {
    diagnostics.push_back({std::move(code), std::move(message), std::move(stage), std::move(pass), std::move(resource), {}});
}

[[nodiscard]] GraphDiagnostic graph_compile_detail::DiagnosticFromError(const Error& error) {
    const std::string text(error.Message());
    const size_t separator = text.find(' ');
    GraphDiagnostic diagnostic{.code = separator == std::string::npos ? "GRF1099" : text.substr(0, separator),
        .message = separator == std::string::npos ? text : text.substr(separator + 1),
        .stage = "compiler",
        .pass = {},
        .resource = {},
        .chain = {}};
    const auto extract = [&](const std::string_view prefix) {
        const size_t begin = diagnostic.message.find(prefix);
        if (begin == std::string::npos)
            return std::string{};
        const size_t value_begin = begin + prefix.size();
        const size_t end = diagnostic.message.find('\'', value_begin);
        return diagnostic.message.substr(value_begin, end - value_begin);
    };
    diagnostic.pass = extract("pass '");
    diagnostic.resource = extract("resource '");
    return diagnostic;
}

[[nodiscard]] bool graph_compile_detail::ValidTexture(const GraphTextureDesc& value) {
    if (value.format == rhi::TextureFormat::Undefined || value.depth_or_layers == 0 || value.mip_levels == 0 || (value.sample_count != 1 && value.sample_count != 4))
        return false;
    if (value.extent.kind == ExtentKind::Fixed)
        return value.extent.width != 0 && value.extent.height != 0;
    return std::isfinite(value.extent.scale_x) && std::isfinite(value.extent.scale_y) && value.extent.scale_x > 0.f && value.extent.scale_y > 0.f;
}

[[nodiscard]] QueueClass graph_compile_detail::AssignQueue(const graph_detail::Pass& pass) {
    if (pass.kind == PassKind::Render)
        return QueueClass::Graphics;
    if (pass.kind == PassKind::Copy)
        return pass.preference == QueuePreference::Graphics ? QueueClass::Graphics : QueueClass::Copy;
    if (pass.preference == QueuePreference::Graphics)
        return QueueClass::Graphics;
    return QueueClass::Compute;
}

[[nodiscard]] bool graph_compile_detail::SameExtent(const rhi::Extent3D& lhs, const rhi::Extent3D& rhs) {
    return lhs.width == rhs.width && lhs.height == rhs.height && lhs.depth_or_array_layers == rhs.depth_or_array_layers;
}

[[nodiscard]] const graph_detail::Use* graph_compile_detail::ProducerUse(const graph_detail::Definition& definition, const graph_detail::Version& version) {
    if (version.producer == kInvalidGraphIndex)
        return nullptr;
    const auto& uses = definition.passes[version.producer].uses;
    const auto version_index = static_cast<u32>(&version - definition.versions.data());
    const auto found = std::ranges::find_if(uses, [&](const graph_detail::Use& use) { return use.write && use.version == version_index; });
    return found == uses.end() ? nullptr : &*found;
}

void graph_compile_detail::HashString(u64& hash, const std::string_view value) {
    detail::CanonicalHashWriter writer;
    writer.Value(hash);
    writer.Value(value);
    hash = detail::Hash64(writer.Finish());
}

} // namespace woki::gfx

namespace woki::gfx::graph_compile_detail {

Result<void> ValidateAndBuildDependencies(State& state) {
    auto& definition = state.definition;
    const u32 width = state.width;
    const u32 height = state.height;
    auto& names = state.names;
    auto& data_edges = state.data_edges;
    auto& readers = state.readers;
    readers.assign(definition->versions.size(), {});
    for (u32 index = 0; index < definition->resources.size(); ++index) {
        auto& resource = definition->resources[index];
        if (resource.type == graph_detail::ResourceType::Texture) {
            if (!ValidTexture(resource.texture))
                return Failure("GRF1004", "invalid texture descriptor at resource " + std::to_string(index));
            if (resource.frame_bound && resource.lifetime != ResourceLifetime::Imported && resource.lifetime != ResourceLifetime::Presentation && resource.lifetime != ResourceLifetime::Temporal)
                return Failure("GRF1014", "frame-bound texture has an invalid lifetime");
            if (resource.external && resource.initial_state == ExternalState::Undefined)
                return Failure("GRF1013", "imported texture requires an explicit initial state");
            if (resource.external && !resource.frame_bound && resource.imported_texture == nullptr)
                return Failure("GRF1014", "static imported texture is null");
        } else if (resource.buffer.size == 0 || resource.buffer.alignment == 0 || (resource.buffer.alignment & (resource.buffer.alignment - 1)) != 0) {
            return Failure("GRF1005", "invalid buffer descriptor at resource " + std::to_string(index));
        } else if (resource.external && resource.initial_state == ExternalState::Undefined) {
            return Failure("GRF1013", "imported buffer requires an explicit initial state");
        } else if (resource.external && !resource.frame_bound && resource.imported_buffer == nullptr) {
            return Failure("GRF1014", "static imported buffer is null");
        }
    }

    for (u32 pass_index = 0; pass_index < definition->passes.size(); ++pass_index) {
        auto& pass = definition->passes[pass_index];
        if (pass.name.empty() || !names.insert(pass.name).second)
            return Failure("GRF1006", "duplicate or empty pass name '" + pass.name + "'");
        if (pass.kind != PassKind::Copy && !pass.callback)
            return Failure("GRF1007", "pass '" + pass.name + "' has no execute callback");
        if (pass.kind == PassKind::Render && pass.colors.empty() && !pass.depth)
            return Failure("GRF1008", "render pass '" + pass.name + "' has no attachments");
        if (pass.kind != PassKind::Render && (!pass.colors.empty() || pass.depth))
            return Failure("GRF1009", "non-render pass declares attachments");
        if (pass.kind != PassKind::Copy && !pass.copies.empty())
            return Failure("GRF1010", "non-copy pass declares a copy");
        if ((pass.kind == PassKind::Render && (pass.preference == QueuePreference::Compute || pass.preference == QueuePreference::Copy || pass.async_compute))
            || (pass.kind == PassKind::Compute && pass.preference == QueuePreference::Copy) || (pass.kind == PassKind::Copy && (pass.preference == QueuePreference::Compute || pass.async_compute)))
            return Failure("GRF1019", "pass '" + pass.name + "' has an impossible queue preference");

        std::set<u32> color_slots;
        for (const auto& color : pass.colors) {
            if (!color_slots.insert(color.attachment.slot).second)
                return Failure("GRF1011", "render pass '" + pass.name + "' has duplicate MRT slot " + std::to_string(color.attachment.slot));
        }
        for (u32 slot = 0; slot < color_slots.size(); ++slot)
            if (!color_slots.contains(slot))
                return Failure("GRF1020", "render pass '" + pass.name + "' has a sparse MRT declaration");

        std::vector<const graph_detail::Use*> declarations;
        for (const auto& use : pass.uses) {
            if (use.version >= definition->versions.size())
                return Failure("GRF1002", "pass '" + pass.name + "' uses a stale or cross-graph handle");
            const auto& version = definition->versions[use.version];
            const auto& resource = definition->resources[version.resource];
            if (resource.type == graph_detail::ResourceType::Texture) {
                const auto range = graph_detail::NormalizeRange(resource.texture, use.subresources);
                if (range.mip_level_count == 0 || range.array_layer_count == 0 || range.base_mip_level >= resource.texture.mip_levels || range.mip_level_count > resource.texture.mip_levels - range.base_mip_level
                    || range.base_array_layer >= resource.texture.depth_or_layers || range.array_layer_count > resource.texture.depth_or_layers - range.base_array_layer)
                    return Failure("GRF1028", "pass '" + pass.name + "' has an out-of-bounds texture subresource range");
                const auto format = use.view.format == rhi::TextureFormat::Undefined ? resource.texture.format : use.view.format;
                if (format != resource.texture.format && std::ranges::find(resource.texture.view_formats, format) == resource.texture.view_formats.end())
                    return Failure("GRF1029", "pass '" + pass.name + "' requests an undeclared texture view format");
                if (resource.texture.sample_count > 1 && (range.mip_level_count != 1 || range.base_mip_level != 0))
                    return Failure("GRF1029", "multisampled texture views must select mip zero");
                if (std::ranges::any_of(declarations,
                        [&](const graph_detail::Use* prior) { return prior->version == use.version && prior->access == use.access && graph_detail::Overlaps(resource.texture, prior->subresources, use.subresources); }))
                    return Failure("GRF1012", "pass '" + pass.name + "' has an overlapping duplicate declaration");
            } else if (std::ranges::any_of(declarations, [&](const graph_detail::Use* prior) { return prior->version == use.version && prior->access == use.access; })) {
                return Failure("GRF1012", "pass '" + pass.name + "' has a duplicate declaration");
            }
            declarations.push_back(&use);
            if ((resource.type == graph_detail::ResourceType::Texture && graph_detail::TextureUsageFor(use.access) == rhi::TextureUsage::None)
                || (resource.type == graph_detail::ResourceType::Buffer && graph_detail::BufferUsageFor(use.access) == rhi::BufferUsage::None))
                return Failure("GRF1009", "resource access is incompatible with its kind");
            if (pass.kind == PassKind::Copy && use.access != GraphAccess::CopySource && use.access != GraphAccess::CopyDestination)
                return Failure("GRF1009", "copy pass has non-copy access");
            if (pass.kind == PassKind::Compute && (use.access == GraphAccess::ColorAttachment || use.access == GraphAccess::DepthRead || use.access == GraphAccess::DepthWrite))
                return Failure("GRF1009", "compute pass has attachment access");
            if (pass.kind != PassKind::Render && (use.access == GraphAccess::Vertex || use.access == GraphAccess::Index))
                return Failure("GRF1009", "non-render pass has vertex or index access");
            const bool attachment_load = !use.write && use.access == GraphAccess::ColorAttachment && std::ranges::any_of(pass.approved_read_writes, [&](const auto& pair) { return pair.first == use.version; });
            if ((!use.write && !graph_detail::IsRead(use.access) && !attachment_load) || (use.write && !graph_detail::IsWrite(use.access)))
                return Failure("GRF1021", "pass '" + pass.name + "' declares an access with the wrong direction");

            if (!use.write) {
                if (version.producer == kInvalidGraphIndex && resource.lifetime == ResourceLifetime::Transient)
                    return Failure("GRF1003", "transient resource '" + resource.texture.label + "' is read before its first write");
                for (const graph_detail::Version* source_version = &version; source_version != nullptr;) {
                    if (source_version->producer != kInvalidGraphIndex && source_version->producer != pass_index) {
                        const auto* producer = ProducerUse(*definition, *source_version);
                        if (resource.type != graph_detail::ResourceType::Texture || producer == nullptr || graph_detail::Overlaps(resource.texture, producer->subresources, use.subresources))
                            data_edges.try_emplace(EdgeKey{source_version->producer, pass_index}, "RAW resource " + std::to_string(version.resource) + ".v" + std::to_string(source_version->ordinal));
                    }
                    source_version = source_version->previous == kInvalidGraphIndex ? nullptr : &definition->versions[source_version->previous];
                }
                readers[use.version].push_back({pass_index, use.subresources});
            } else {
                if (version.producer != pass_index)
                    return Failure("GRF1015", "resource version producer does not match its declaring pass");
                if (version.previous != kInvalidGraphIndex) {
                    const bool reads_previous = std::ranges::any_of(pass.uses, [&](const auto& candidate) { return !candidate.write && candidate.version == version.previous; });
                    if (reads_previous && !pass.approved_read_writes.contains({version.previous, use.version}))
                        return Failure("GRF1017", "pass '" + pass.name + "' has ambiguous in-place access; use ReadWrite or an attachment load");
                }
            }
        }
        for (const u32 dependency : pass.explicit_dependencies) {
            if (dependency >= definition->passes.size())
                return Failure("GRF1002", "pass '" + pass.name + "' has a stale explicit dependency");
            data_edges.try_emplace(EdgeKey{dependency, pass_index}, "explicit");
        }

        std::optional<rhi::Extent3D> attachment_extent;
        u32 attachment_samples{};
        for (const auto& color : pass.colors) {
            const auto resource_index = definition->versions[color.version].resource;
            const auto& descriptor = definition->resources[resource_index].texture;
            const auto& use = *std::ranges::find_if(pass.uses, [&](const graph_detail::Use& candidate) { return candidate.write && candidate.version == color.version; });
            const auto extent = graph_detail::SubresourceExtent(graph_detail::ResolveExtent(descriptor, width, height), graph_detail::NormalizeRange(descriptor, use.subresources));
            if (graph_detail::IsDepthFormat(descriptor.format))
                return Failure("GRF1022", "color attachment uses a depth format");
            if (attachment_extent && (!SameExtent(*attachment_extent, extent) || attachment_samples != descriptor.sample_count))
                return Failure("GRF1023", "render attachments have mismatched extents or sample counts");
            attachment_extent = extent;
            attachment_samples = descriptor.sample_count;
            if (color.attachment.resolve) {
                const u32 resolve_version = color.attachment.resolve->Index();
                if (resolve_version >= definition->versions.size())
                    return Failure("GRF1002", "resolve attachment uses a stale handle");
                const auto& resolve = definition->resources[definition->versions[resolve_version].resource].texture;
                if (descriptor.sample_count == 1 || resolve.sample_count != 1 || descriptor.format != resolve.format || !SameExtent(graph_detail::ResolveExtent(resolve, width, height), extent))
                    return Failure("GRF1024", "resolve source/target format, extent, or sample count is invalid");
            }
        }
        if (pass.depth) {
            const auto resource_index = definition->versions[pass.depth->version].resource;
            const auto& descriptor = definition->resources[resource_index].texture;
            const auto& use = *std::ranges::find_if(pass.uses, [&](const graph_detail::Use& candidate) { return candidate.version == pass.depth->version; });
            const auto extent = graph_detail::SubresourceExtent(graph_detail::ResolveExtent(descriptor, width, height), graph_detail::NormalizeRange(descriptor, use.subresources));
            if (!graph_detail::IsDepthFormat(descriptor.format))
                return Failure("GRF1022", "depth attachment uses a color format");
            if (attachment_extent && (!SameExtent(*attachment_extent, extent) || attachment_samples != descriptor.sample_count))
                return Failure("GRF1023", "render attachments have mismatched extents or sample counts");
        }
        for (const auto& copy : pass.copies) {
            if (copy.source >= definition->versions.size() || copy.destination >= definition->versions.size())
                return Failure("GRF1002", "copy uses a stale handle");
            const auto& source_version = definition->versions[copy.source];
            const auto& destination_version = definition->versions[copy.destination];
            if (source_version.type != destination_version.type)
                return Failure("GRF1025", "copy source and destination kinds differ");
            const auto& source = definition->resources[source_version.resource];
            const auto& destination = definition->resources[destination_version.resource];
            if (source.type == graph_detail::ResourceType::Buffer) {
                const u64 size = copy.size == 0 ? std::min(source.buffer.size, destination.buffer.size) : copy.size;
                if (size == 0 || size > source.buffer.size || size > destination.buffer.size || size % 4 != 0)
                    return Failure("GRF1026", "buffer copy size is out of bounds or unaligned");
            } else if (source.texture.format != destination.texture.format || source.texture.sample_count != 1 || destination.texture.sample_count != 1 || source.texture.mip_levels != destination.texture.mip_levels
                       || !SameExtent(graph_detail::ResolveExtent(source.texture, width, height), graph_detail::ResolveExtent(destination.texture, width, height))) {
                return Failure("GRF1027", "texture copy descriptors are incompatible");
            }
        }
    }
    return Ok();
}

} // namespace woki::gfx::graph_compile_detail

namespace woki::gfx {

std::span<const GraphDiagnostic> CompiledRenderGraph::Diagnostics() const noexcept {
    return impl_->diagnostics;
}

GraphCompileReport GraphCompiler::CompileWithReport(std::shared_ptr<graph_detail::Definition> definition, const u32 width, const u32 height) {
    GraphCompileReport report;
    if (definition == nullptr || width == 0 || height == 0) {
        graph_compile_detail::AddDiagnostic(report.diagnostics, "GRF1000", "compile requires a definition and non-zero frame extent", "validation");
        return report;
    }

    std::set<std::string> names;
    for (const auto& resource : definition->resources) {
        const std::string name = resource.type == graph_detail::ResourceType::Texture ? resource.texture.label : resource.buffer.label;
        if (resource.type == graph_detail::ResourceType::Texture && !graph_compile_detail::ValidTexture(resource.texture))
            graph_compile_detail::AddDiagnostic(report.diagnostics, "GRF1004", "invalid texture descriptor", "validation", {}, name);
        if (resource.type == graph_detail::ResourceType::Buffer && (resource.buffer.size == 0 || resource.buffer.alignment == 0 || (resource.buffer.alignment & (resource.buffer.alignment - 1)) != 0))
            graph_compile_detail::AddDiagnostic(report.diagnostics, "GRF1005", "invalid buffer descriptor", "validation", {}, name);
        if (resource.external && resource.initial_state == ExternalState::Undefined)
            graph_compile_detail::AddDiagnostic(report.diagnostics, "GRF1013", "imported resource requires an explicit initial state", "validation", {}, name);
        if (resource.external && !resource.frame_bound && resource.imported_texture == nullptr && resource.imported_buffer == nullptr)
            graph_compile_detail::AddDiagnostic(report.diagnostics, "GRF1014", "static imported resource is null", "validation", {}, name);
    }
    for (const auto& pass : definition->passes) {
        if (pass.name.empty() || !names.insert(pass.name).second)
            graph_compile_detail::AddDiagnostic(report.diagnostics, "GRF1006", "duplicate or empty pass name", "validation", pass.name);
        if (pass.kind != PassKind::Copy && !pass.callback)
            graph_compile_detail::AddDiagnostic(report.diagnostics, "GRF1007", "pass has no execute callback", "validation", pass.name);
        if (pass.kind == PassKind::Render && pass.colors.empty() && !pass.depth)
            graph_compile_detail::AddDiagnostic(report.diagnostics, "GRF1008", "render pass has no attachments", "validation", pass.name);
        if (pass.kind != PassKind::Render && (!pass.colors.empty() || pass.depth))
            graph_compile_detail::AddDiagnostic(report.diagnostics, "GRF1009", "non-render pass declares attachments", "validation", pass.name);
        if (pass.kind != PassKind::Copy && !pass.copies.empty())
            graph_compile_detail::AddDiagnostic(report.diagnostics, "GRF1010", "non-copy pass declares a copy", "validation", pass.name);
    }
    if (!report.diagnostics.empty())
        return report;

    auto compiled = Compile(std::move(definition), width, height);
    if (!compiled) {
        report.diagnostics.push_back(graph_compile_detail::DiagnosticFromError(compiled.error()));
        return report;
    }
    report.graph.emplace(std::move(*compiled));
    return report;
}

} // namespace woki::gfx
