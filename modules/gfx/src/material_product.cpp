#include <bit>
#include <cstring>
#include <limits>
#include <woki/config.hpp>
#include <set>

#include <woki/gfx/advanced/material_product.hpp>
#include <woki/gfx/advanced/render_abi.hpp>
#include <woki/gfx/advanced/mesh.hpp>
#include "binary_codec.hpp"

namespace woki::gfx {
namespace {
constexpr std::array<std::byte, 4> kDefinitionMagic{std::byte{'W'}, std::byte{'M'}, std::byte{'T'}, std::byte{'D'}};
constexpr std::array<std::byte, 4> kInstanceMagic{std::byte{'W'}, std::byte{'M'}, std::byte{'T'}, std::byte{'I'}};
constexpr u32 kMaxRecords = 4096;
constexpr u32 kMaxString = 1024 * 1024;

using Writer = detail::BinaryWriter;

class Reader : public detail::BinaryReader {
public:
    using BinaryReader::BinaryReader;

    bool String(std::string& value) {
        return BinaryReader::String(value, kMaxString);
    }
};

template <typename T, typename F>
bool Vector(Reader& reader, std::vector<T>& values, F read) {
    u32 count{};
    if (!reader.Int(count) || count > kMaxRecords)
        return false;
    values.resize(count);
    for (auto& value : values)
        if (!read(value))
            return false;
    return true;
}

u32 Align(u32 value, u32 alignment) {
    return (value + alignment - 1U) & ~(alignment - 1U);
}

std::pair<u32, u32> Layout(MaterialValueType type) {
    switch (type) {
        case MaterialValueType::Bool:
        case MaterialValueType::I32:
        case MaterialValueType::U32:
        case MaterialValueType::F32:
            return {4, 4};
        case MaterialValueType::Vec2:
            return {8, 8};
        case MaterialValueType::Vec3:
            return {16, 12};
        case MaterialValueType::Vec4:
            return {16, 16};
        case MaterialValueType::Mat4:
            return {16, 64};
        case MaterialValueType::Texture2D:
        case MaterialValueType::TextureCube:
        case MaterialValueType::Sampler:
            return {1, 0};
    }
    return {1, 0};
}

template <typename T>
void Store(std::vector<std::byte>& output, u32 offset, const T& value) {
    std::memcpy(output.data() + offset, &value, sizeof(value));
}

Result<std::vector<std::byte>> PackValue(MaterialValueType type, const MaterialValue& value, u32 size) {
    if (!MaterialValueMatches(type, value))
        return Err(ErrorCode::ValidationInvalidState, "material value type mismatch");
    std::vector<std::byte> bytes(size);
    switch (type) {
        case MaterialValueType::Bool: {
            const u32 encoded = std::get<bool>(value) ? 1U : 0U;
            Store(bytes, 0, encoded);
            break;
        }
        case MaterialValueType::I32:
            Store(bytes, 0, std::get<i32>(value));
            break;
        case MaterialValueType::U32:
            Store(bytes, 0, std::get<u32>(value));
            break;
        case MaterialValueType::F32:
            Store(bytes, 0, std::get<f32>(value));
            break;
        case MaterialValueType::Vec2:
            Store(bytes, 0, std::get<std::array<f32, 2>>(value));
            break;
        case MaterialValueType::Vec3:
            Store(bytes, 0, std::get<std::array<f32, 3>>(value));
            break;
        case MaterialValueType::Vec4:
            Store(bytes, 0, std::get<std::array<f32, 4>>(value));
            break;
        case MaterialValueType::Mat4:
            Store(bytes, 0, std::get<std::array<f32, 16>>(value));
            break;
        default:
            return Err(ErrorCode::ValidationInvalidState, "resource value cannot be packed into material parameters");
    }
    return Ok(std::move(bytes));
}

void WriteValue(Writer& out, MaterialValueType type, const MaterialValue& value) {
    out.Int(static_cast<u8>(type));
    if (type >= MaterialValueType::Texture2D) {
        out.Id(std::get<asset::AssetId>(value));
        return;
    }
    const auto size = Layout(type).second;
    auto packed = PackValue(type, value, size);
    out.Raw(*packed);
}

bool ReadValue(Reader& in, MaterialValue& value, MaterialValueType* expected = nullptr) {
    u8 raw{};
    if (!in.Int(raw) || raw > static_cast<u8>(MaterialValueType::Sampler))
        return false;
    const auto type = static_cast<MaterialValueType>(raw);
    if (expected && type != *expected)
        return false;
    if (type >= MaterialValueType::Texture2D) {
        asset::AssetId id;
        if (!in.Id(id))
            return false;
        value = id;
        return true;
    }
    std::vector<std::byte> bytes;
    if (!in.Raw(bytes) || bytes.size() != Layout(type).second)
        return false;
    switch (type) {
        case MaterialValueType::Bool: {
            u32 item{};
            std::memcpy(&item, bytes.data(), 4);
            if (item > 1)
                return false;
            value = item != 0;
            break;
        }
        case MaterialValueType::I32: {
            i32 item{};
            std::memcpy(&item, bytes.data(), 4);
            value = item;
            break;
        }
        case MaterialValueType::U32: {
            u32 item{};
            std::memcpy(&item, bytes.data(), 4);
            value = item;
            break;
        }
        case MaterialValueType::F32: {
            f32 item{};
            std::memcpy(&item, bytes.data(), 4);
            value = item;
            break;
        }
        case MaterialValueType::Vec2: {
            std::array<f32, 2> item{};
            std::memcpy(item.data(), bytes.data(), bytes.size());
            value = item;
            break;
        }
        case MaterialValueType::Vec3: {
            std::array<f32, 3> item{};
            std::memcpy(item.data(), bytes.data(), bytes.size());
            value = item;
            break;
        }
        case MaterialValueType::Vec4: {
            std::array<f32, 4> item{};
            std::memcpy(item.data(), bytes.data(), bytes.size());
            value = item;
            break;
        }
        case MaterialValueType::Mat4: {
            std::array<f32, 16> item{};
            std::memcpy(item.data(), bytes.data(), bytes.size());
            value = item;
            break;
        }
        default:
            return false;
    }
    return true;
}

void WriteBinding(Writer& out, const BindingInfo& item) {
    out.Int(item.group);
    out.Int(item.binding);
    out.Int(item.stages);
    out.Int(static_cast<u8>(item.kind));
    out.Int(static_cast<u8>(item.dimension));
    out.Int(static_cast<u8>(item.sample_type));
    out.Int(static_cast<u8>(item.storage_access));
    out.Int(static_cast<u8>(item.storage_format));
    out.Int(item.min_binding_size);
    out.Int(item.array_size);
}

bool ReadBinding(Reader& in, BindingInfo& item) {
    u8 kind{}, dim{}, sample{}, access{}, format{};
    if (!in.Int(item.group) || !in.Int(item.binding) || !in.Int(item.stages) || !in.Int(kind) || !in.Int(dim) || !in.Int(sample) || !in.Int(access) || !in.Int(format) || !in.Int(item.min_binding_size)
        || !in.Int(item.array_size) || kind > static_cast<u8>(ResourceKind::ExternalTexture) || dim > static_cast<u8>(TextureDimension::CubeArray) || sample > static_cast<u8>(SampleType::Depth)
        || access > static_cast<u8>(StorageAccess::ReadWrite) || format > static_cast<u8>(StorageFormat::RGBA32Float))
        return false;
    item.kind = static_cast<ResourceKind>(kind);
    item.dimension = static_cast<TextureDimension>(dim);
    item.sample_type = static_cast<SampleType>(sample);
    item.storage_access = static_cast<StorageAccess>(access);
    item.storage_format = static_cast<StorageFormat>(format);
    return true;
}

void WriteSampler(Writer& out, const SamplerKey& key) {
    out.Int(static_cast<u8>(key.address_u));
    out.Int(static_cast<u8>(key.address_v));
    out.Int(static_cast<u8>(key.address_w));
    out.Int(static_cast<u8>(key.mag_filter));
    out.Int(static_cast<u8>(key.min_filter));
    out.Int(static_cast<u8>(key.mip_filter));
    out.Int(key.lod_min_bits);
    out.Int(key.lod_max_bits);
    out.Int(static_cast<u8>(key.compare));
    out.Int(key.max_anisotropy);
}

bool ReadSampler(Reader& in, SamplerKey& key) {
    u8 au{}, av{}, aw{}, mag{}, min{}, mip{}, compare{};
    if (!in.Int(au) || !in.Int(av) || !in.Int(aw) || !in.Int(mag) || !in.Int(min) || !in.Int(mip) || !in.Int(key.lod_min_bits) || !in.Int(key.lod_max_bits) || !in.Int(compare) || !in.Int(key.max_anisotropy))
        return false;
    if (au < static_cast<u8>(rhi::AddressMode::ClampToEdge) || au > static_cast<u8>(rhi::AddressMode::MirrorRepeat) || av < static_cast<u8>(rhi::AddressMode::ClampToEdge)
        || av > static_cast<u8>(rhi::AddressMode::MirrorRepeat) || aw < static_cast<u8>(rhi::AddressMode::ClampToEdge) || aw > static_cast<u8>(rhi::AddressMode::MirrorRepeat)
        || mag < static_cast<u8>(rhi::FilterMode::Nearest) || mag > static_cast<u8>(rhi::FilterMode::Linear) || min < static_cast<u8>(rhi::FilterMode::Nearest) || min > static_cast<u8>(rhi::FilterMode::Linear)
        || mip < static_cast<u8>(rhi::MipmapFilterMode::Nearest) || mip > static_cast<u8>(rhi::MipmapFilterMode::Linear) || compare > static_cast<u8>(rhi::CompareFunction::Always) || key.max_anisotropy == 0
        || key.max_anisotropy > 16)
        return false;
    key.address_u = static_cast<rhi::AddressMode>(au);
    key.address_v = static_cast<rhi::AddressMode>(av);
    key.address_w = static_cast<rhi::AddressMode>(aw);
    key.mag_filter = static_cast<rhi::FilterMode>(mag);
    key.min_filter = static_cast<rhi::FilterMode>(min);
    key.mip_filter = static_cast<rhi::MipmapFilterMode>(mip);
    key.compare = static_cast<rhi::CompareFunction>(compare);
    return true;
}

ContentHash RenderStateHash(const MaterialRenderState& state) {
    Writer out;
    out.Int(static_cast<u8>(state.blend));
    out.Int(static_cast<u8>(state.cull));
    out.Int(static_cast<u8>(state.depth_compare));
    out.Int(static_cast<u8>(state.depth_write));
    out.Int(static_cast<u8>(state.alpha_test));
    return Sha256(out.bytes);
}

void Diagnostic(MaterialCompileResult& result, std::string code, std::string message) {
    result.diagnostics.push_back({std::move(code), DiagnosticSeverity::Error, std::move(message), {}, {}});
}
} // namespace

MaterialCompileResult CompileMaterialType(const MaterialTypeSource& source, const ShaderPayload& shader, const ContentHash shader_product_hash) {
    MaterialCompileResult result;
    MaterialDefinitionProduct output{.id = source.id,
        .name = source.name,
        .shader = source.shader,
        .shader_product_hash = shader_product_hash,
        .shader_variant_hash = shader.variant_hash,
        .product_family = source.product_family,
        .passes = source.passes,
        .programs = {},
        .binding_plan = {}};
    output.binding_plan.interface_hash = shader.interface_hash;
    output.binding_plan.render_state = source.render_state;
    output.binding_plan.render_state_identity = RenderStateHash(source.render_state);
    output.binding_plan.phase = source.render_state.blend == MaterialBlendMode::Opaque ? (source.render_state.alpha_test ? MaterialPhase::AlphaTest : MaterialPhase::Opaque) : MaterialPhase::Transparent;
    for (MaterialPass pass : source.passes) {
        const auto selected = source.entry_points.find(pass);
        if (selected == source.entry_points.end()) {
            Diagnostic(result, "MAT2001", "supported pass has no selected entry points");
            continue;
        }
        const auto vertex = std::ranges::find_if(shader.interface.entry_points, [&](const auto& item) { return item.stage == ShaderStage::Vertex && item.name == selected->second.first; });
        const auto fragment = std::ranges::find_if(shader.interface.entry_points, [&](const auto& item) { return item.stage == ShaderStage::Fragment && item.name == selected->second.second; });
        if (vertex == shader.interface.entry_points.end() || fragment == shader.interface.entry_points.end())
            Diagnostic(result, "MAT2002", "selected material entry point is absent from shader reflection");
        else
            output.programs.push_back({pass, selected->second.first, selected->second.second});
    }
    const auto parameter = std::ranges::find_if(shader.interface.bindings,
        [](const auto& item) { return item.group == kMaterialGroup && (item.kind == ResourceKind::UniformBuffer || item.kind == ResourceKind::ReadOnlyStorageBuffer); });
    if (parameter == shader.interface.bindings.end())
        Diagnostic(result, "MAT2003", "group 2 requires one reflected parameter buffer");
    else
        output.binding_plan.parameter_binding = parameter->binding;
    u32 cursor{};
    for (const auto& source_property : source.properties) {
        const auto [alignment, size] = Layout(source_property.type);
        const u32 offset = source_property.offset.value_or(Align(cursor, alignment));
        if (offset % alignment != 0 || offset < cursor) {
            Diagnostic(result, "MAT2004", "material property offset violates WGSL host-shareable alignment or overlaps");
            continue;
        }
        MaterialPropertyPlan property{.id = source_property.id, .type = source_property.type, .offset = offset, .size = size, .default_bytes = {}};
        auto bytes = PackValue(property.type, source_property.default_value, property.size);
        if (!bytes) {
            Diagnostic(result, "MAT2005", std::string(bytes.error().Message()));
            continue;
        }
        property.default_bytes = std::move(*bytes);
        output.binding_plan.properties.push_back(std::move(property));
        cursor = offset + size;
    }
    output.binding_plan.parameter_size = Align(cursor, 16);
    if (parameter != shader.interface.bindings.end()) {
        if (parameter->min_binding_size > std::numeric_limits<u32>::max())
            Diagnostic(result, "MAT2013", "reflected material buffer size exceeds the supported range");
        else
            output.binding_plan.parameter_size =
                std::max(output.binding_plan.parameter_size, static_cast<u32>(parameter->min_binding_size));
    }
    if (parameter != shader.interface.bindings.end() && output.binding_plan.parameter_size > parameter->min_binding_size)
        Diagnostic(result, "MAT2006", "packed parameters exceed reflected buffer minimum binding size");
    std::set<u32> authored_bindings;
    if (parameter != shader.interface.bindings.end())
        authored_bindings.insert(parameter->binding);
    for (const auto& texture : source.textures) {
        const auto reflected = std::ranges::find_if(shader.interface.bindings, [&](const auto& item) { return item.group == kMaterialGroup && item.binding == texture.binding; });
        const auto dimension = texture.type == MaterialValueType::TextureCube ? TextureDimension::Cube : TextureDimension::D2;
        if (reflected == shader.interface.bindings.end() || reflected->kind != ResourceKind::SampledTexture || reflected->dimension != dimension || reflected->sample_type != SampleType::Float)
            Diagnostic(result, "MAT2007", "texture declaration does not match reflected group 2 binding");
        else
            authored_bindings.insert(texture.binding);
        output.binding_plan.textures.push_back({texture.id, texture.type, texture.binding, texture.semantic, texture.default_asset});
    }
    for (const auto& sampler : source.samplers) {
        const auto reflected = std::ranges::find_if(shader.interface.bindings, [&](const auto& item) { return item.group == kMaterialGroup && item.binding == sampler.binding; });
        if (reflected == shader.interface.bindings.end() || reflected->kind != ResourceKind::Sampler)
            Diagnostic(result, "MAT2008", "sampler declaration does not match reflected group 2 binding");
        else
            authored_bindings.insert(sampler.binding);
        output.binding_plan.samplers.push_back({sampler.id, sampler.binding, MakeSamplerKey(sampler.defaults)});
    }
    if (std::ranges::any_of(shader.interface.bindings, [&](const auto& item) { return item.group == kMaterialGroup && !authored_bindings.contains(item.binding); }))
        Diagnostic(result, "MAT2009", "reflected group 2 binding is not declared by the material type");
    for (const auto& override : source.overrides) {
        const auto reflected = std::ranges::find(shader.interface.overrides, override.override_id, &OverrideInfo::id);
        const auto property = std::ranges::find(source.properties, override.property, &MaterialPropertySource::id);
        if (property == source.properties.end()) {
            Diagnostic(result, "MAT2010", "override mapping refers to an unknown numeric property");
            continue;
        }
        const auto expected = property->type == MaterialValueType::Bool  ? ValueType::Bool
                              : property->type == MaterialValueType::I32 ? ValueType::I32
                              : property->type == MaterialValueType::U32 ? ValueType::U32
                                                                         : ValueType::F32;
        if (reflected == shader.interface.overrides.end() || reflected->type != expected)
            Diagnostic(result, "MAT2010", "override mapping does not match reflected override ID/type");
        output.binding_plan.variants.push_back({override.property, override.override_id, property->type, override.values});
    }
    auto layout = MakePipelineLayoutKey(shader.interface, abi::kDynamicBufferPolicy);
    if (!layout)
        Diagnostic(result, "MAT2011", std::string(layout.error().Message()));
    else
        output.binding_plan.pipeline_layout = std::move(*layout);
    std::ranges::sort(output.binding_plan.properties, {}, &MaterialPropertyPlan::id);
    std::ranges::sort(output.binding_plan.textures, {}, &MaterialTextureBinding::id);
    std::ranges::sort(output.binding_plan.samplers, {}, &MaterialSamplerBinding::id);
    std::ranges::sort(output.binding_plan.variants, {}, &MaterialVariantInput::property);
    if (result.diagnostics.empty())
        result.definition = std::move(output);
    return result;
}

Result<MaterialInstanceProduct> CompileMaterialInstance(const MaterialInstanceSource& source, const MaterialDefinitionProduct& definition, const ContentHash definition_hash) {
    if (source.type != definition.id)
        return Err(ErrorCode::ValidationInvalidState, "material instance definition identity mismatch");
    for (const auto& [id, value] : source.overrides) {
        const auto property = std::ranges::find(definition.binding_plan.properties, id, &MaterialPropertyPlan::id);
        const auto texture = std::ranges::find(definition.binding_plan.textures, id, &MaterialTextureBinding::id);
        if (property != definition.binding_plan.properties.end() && !MaterialValueMatches(property->type, value))
            return Err(ErrorCode::ValidationInvalidState, "material instance property type mismatch");
        if (property == definition.binding_plan.properties.end() && (texture == definition.binding_plan.textures.end() || !MaterialValueMatches(texture->type, value)))
            return Err(ErrorCode::ValidationInvalidState, "material instance contains an unknown property");
    }
    MaterialInstanceProduct result{source.id, source.type, definition_hash, source.overrides, {}};
    for (const auto& [id, value] : source.overrides) {
        static_cast<void>(value);
        const auto property = std::ranges::find(definition.binding_plan.properties, id, &MaterialPropertyPlan::id);
        const auto texture = std::ranges::find(definition.binding_plan.textures, id, &MaterialTextureBinding::id);
        result.override_types.emplace(id, property != definition.binding_plan.properties.end() ? property->type : texture->type);
    }
    return Ok(std::move(result));
}

Result<std::vector<std::byte>> SerializeMaterialDefinition(const MaterialDefinitionProduct& product) {
    if (!product.id || !product.shader || product.name.empty() || product.programs.size() != product.passes.size() || product.binding_plan.properties.size() > kMaxMaterialProperties
        || product.binding_plan.textures.size() > kMaxMaterialProperties || product.binding_plan.samplers.size() > kMaxMaterialProperties || product.binding_plan.variants.size() > kMaxMaterialOverrides)
        return Err(ErrorCode::ValidationInvalidState, "material definition is invalid");
    Writer out;
    out.bytes.insert(out.bytes.end(), kDefinitionMagic.begin(), kDefinitionMagic.end());
    out.Int(kMaterialProductVersion);
    out.Id(product.id);
    out.String(product.name);
    out.Id(product.shader);
    out.Hash(product.shader_product_hash);
    out.Hash(product.shader_variant_hash);
    out.String(product.product_family);
    out.Int(static_cast<u32>(product.passes.size()));
    for (auto pass : product.passes)
        out.Int(static_cast<u8>(pass));
    out.Int(static_cast<u32>(product.programs.size()));
    for (const auto& program : product.programs) {
        out.Int(static_cast<u8>(program.pass));
        out.String(program.vertex_entry);
        out.String(program.fragment_entry);
    }
    const auto& plan = product.binding_plan;
    out.Hash(plan.interface_hash);
    out.Int(plan.parameter_binding);
    out.Int(plan.parameter_size);
    out.Int(static_cast<u8>(plan.phase));
    out.Int(static_cast<u8>(plan.render_state.blend));
    out.Int(static_cast<u8>(plan.render_state.cull));
    out.Int(static_cast<u8>(plan.render_state.depth_compare));
    out.Int(static_cast<u8>(plan.render_state.depth_write));
    out.Int(static_cast<u8>(plan.render_state.alpha_test));
    out.Hash(plan.render_state_identity);
    out.Int(static_cast<u32>(plan.pipeline_layout.groups.size()));
    for (const auto& group : plan.pipeline_layout.groups) {
        out.Int(group.group);
        out.Int(static_cast<u32>(group.bindings.size()));
        for (const auto& binding : group.bindings)
            WriteBinding(out, binding);
        out.Int(static_cast<u32>(group.dynamic_buffer_bindings.size()));
        for (u32 binding : group.dynamic_buffer_bindings)
            out.Int(binding);
        out.Hash(group.hash);
    }
    out.Hash(plan.pipeline_layout.hash);
    out.Int(static_cast<u32>(plan.properties.size()));
    for (const auto& item : plan.properties) {
        out.Int(item.id.Value());
        out.Int(static_cast<u8>(item.type));
        out.Int(item.offset);
        out.Int(item.size);
        out.Raw(item.default_bytes);
    }
    out.Int(static_cast<u32>(plan.textures.size()));
    for (const auto& item : plan.textures) {
        out.Int(item.id.Value());
        out.Int(static_cast<u8>(item.type));
        out.Int(item.binding);
        out.Int(static_cast<u8>(item.semantic));
        out.Id(item.default_asset);
    }
    out.Int(static_cast<u32>(plan.samplers.size()));
    for (const auto& item : plan.samplers) {
        out.Int(item.id.Value());
        out.Int(item.binding);
        WriteSampler(out, item.key);
    }
    out.Int(static_cast<u32>(plan.variants.size()));
    for (const auto& item : plan.variants) {
        out.Int(item.property.Value());
        out.Int(item.override_id);
        out.Int(static_cast<u8>(item.type));
        out.Int(static_cast<u32>(item.allowed_values.size()));
        for (const auto& value : item.allowed_values)
            WriteValue(out, item.type, value);
    }
    return Ok(std::move(out.bytes));
}

Result<MaterialDefinitionProduct> ParseMaterialDefinition(const std::span<const std::byte> bytes) {
    if (bytes.size() < 4 || !std::ranges::equal(kDefinitionMagic, bytes.first(4)))
        return Err(ErrorCode::ParseInvalidFormat, "material definition magic is invalid");
    Reader in(bytes.subspan(4));
    MaterialDefinitionProduct result;
    u32 version{}, count{};
    u8 phase{}, blend{}, cull{}, compare{}, depth{}, alpha_test{};
    if (!in.Int(version) || version != kMaterialProductVersion || !in.Id(result.id) || !in.String(result.name) || !in.Id(result.shader) || !in.Hash(result.shader_product_hash) || !in.Hash(result.shader_variant_hash)
        || !in.String(result.product_family) || !in.Int(count) || count > 4)
        return Err(ErrorCode::ParseInvalidFormat, "material definition header is invalid");
    for (u32 i = 0; i < count; ++i) {
        u8 value{};
        if (!in.Int(value) || value > static_cast<u8>(MaterialPass::Velocity))
            return Err(ErrorCode::ParseInvalidFormat, "material pass is invalid");
        result.passes.push_back(static_cast<MaterialPass>(value));
    }
    if (!Vector(in, result.programs, [&](MaterialPassProgram& program) {
            u8 pass{};
            if (!in.Int(pass) || pass > static_cast<u8>(MaterialPass::Velocity) || !in.String(program.vertex_entry) || program.vertex_entry.empty() || !in.String(program.fragment_entry) || program.fragment_entry.empty())
                return false;
            program.pass = static_cast<MaterialPass>(pass);
            return true;
        }))
        return Err(ErrorCode::ParseInvalidFormat, "material pass programs are invalid");
    auto& plan = result.binding_plan;
    if (!in.Hash(plan.interface_hash) || !in.Int(plan.parameter_binding) || !in.Int(plan.parameter_size) || !in.Int(phase) || phase > static_cast<u8>(MaterialPhase::Transparent) || !in.Int(blend)
        || blend > static_cast<u8>(MaterialBlendMode::Additive) || !in.Int(cull) || cull < static_cast<u8>(rhi::CullMode::None) || cull > static_cast<u8>(rhi::CullMode::Back) || !in.Int(compare)
        || compare < static_cast<u8>(rhi::CompareFunction::Never) || compare > static_cast<u8>(rhi::CompareFunction::Always) || !in.Int(depth) || depth > 1 || !in.Int(alpha_test) || alpha_test > 1
        || !in.Hash(plan.render_state_identity))
        return Err(ErrorCode::ParseInvalidFormat, "material binding plan is invalid");
    plan.phase = static_cast<MaterialPhase>(phase);
    plan.render_state = {static_cast<MaterialBlendMode>(blend), static_cast<rhi::CullMode>(cull), static_cast<rhi::CompareFunction>(compare), depth != 0, alpha_test != 0};
    if (!Vector(in, plan.pipeline_layout.groups,
            [&](BindGroupLayoutKey& group) {
                if (!in.Int(group.group) || !Vector(in, group.bindings, [&](BindingInfo& binding) { return ReadBinding(in, binding); })
                    || !Vector(in, group.dynamic_buffer_bindings, [&](u32& binding) { return in.Int(binding); }))
                    return false;
                return in.Hash(group.hash);
            })
        || !in.Hash(plan.pipeline_layout.hash))
        return Err(ErrorCode::ParseInvalidFormat, "material pipeline layout is invalid");
    if (!Vector(in, plan.properties,
            [&](MaterialPropertyPlan& item) {
                u32 id{};
                u8 type{};
                if (!in.Int(id) || !in.Int(type) || type > static_cast<u8>(MaterialValueType::Mat4) || !in.Int(item.offset) || !in.Int(item.size) || !in.Raw(item.default_bytes) || item.default_bytes.size() != item.size)
                    return false;
                item.id = MaterialPropertyId(id);
                item.type = static_cast<MaterialValueType>(type);
                return true;
            })
        || plan.properties.size() > kMaxMaterialProperties)
        return Err(ErrorCode::ParseInvalidFormat, "material properties are invalid");
    if (!Vector(in, plan.textures,
            [&](MaterialTextureBinding& item) {
                u32 id{};
                u8 type{}, semantic{};
                if (!in.Int(id) || !in.Int(type) || (type != static_cast<u8>(MaterialValueType::Texture2D) && type != static_cast<u8>(MaterialValueType::TextureCube)) || !in.Int(item.binding) || !in.Int(semantic)
                    || semantic > static_cast<u8>(TextureSemantic::Environment) || !in.Id(item.default_asset))
                    return false;
                item.id = MaterialPropertyId(id);
                item.type = static_cast<MaterialValueType>(type);
                item.semantic = static_cast<TextureSemantic>(semantic);
                return true;
            })
        || plan.textures.size() > kMaxMaterialProperties)
        return Err(ErrorCode::ParseInvalidFormat, "material textures are invalid");
    if (!Vector(in, plan.samplers,
            [&](MaterialSamplerBinding& item) {
                u32 id{};
                if (!in.Int(id) || !in.Int(item.binding) || !ReadSampler(in, item.key))
                    return false;
                item.id = MaterialPropertyId(id);
                return true;
            })
        || plan.samplers.size() > kMaxMaterialProperties)
        return Err(ErrorCode::ParseInvalidFormat, "material samplers are invalid");
    if (!Vector(in, plan.variants,
            [&](MaterialVariantInput& item) {
                u32 id{}, values{};
                u8 type{};
                if (!in.Int(id) || !in.Int(item.override_id) || !in.Int(type) || type > static_cast<u8>(MaterialValueType::F32) || !in.Int(values) || values > kMaxMaterialPermutations)
                    return false;
                item.property = MaterialPropertyId(id);
                item.type = static_cast<MaterialValueType>(type);
                for (u32 i = 0; i < values; ++i) {
                    MaterialValue value;
                    if (!ReadValue(in, value, &item.type))
                        return false;
                    item.allowed_values.push_back(std::move(value));
                }
                return true;
            })
        || !in.End())
        return Err(ErrorCode::ParseInvalidFormat, "material variants or trailing bytes are invalid");
    if (plan.render_state_identity != RenderStateHash(plan.render_state))
        return Err(ErrorCode::ParseInvalidFormat, "material render-state identity is invalid");
    if (!result.id || !result.shader || result.name.empty() || result.passes.empty() || result.programs.size() != result.passes.size() || plan.interface_hash == ContentHash{} || plan.parameter_size == 0
        || plan.parameter_size % 16 != 0)
        return Err(ErrorCode::ParseInvalidFormat, "material definition identity or parameter plan is incomplete");
    std::ranges::sort(result.passes);
    std::ranges::sort(result.programs, {}, &MaterialPassProgram::pass);
    if (std::ranges::adjacent_find(result.passes) != result.passes.end() || std::ranges::adjacent_find(result.programs, {}, &MaterialPassProgram::pass) != result.programs.end())
        return Err(ErrorCode::ParseInvalidFormat, "material passes are duplicated");
    for (size_t index = 0; index < result.passes.size(); ++index)
        if (result.passes[index] != result.programs[index].pass)
            return Err(ErrorCode::ParseInvalidFormat, "material pass program membership is inconsistent");
    std::set<MaterialPropertyId> property_ids;
    std::set<u32> bindings{plan.parameter_binding};
    for (const auto& property : plan.properties) {
        const auto [alignment, size] = Layout(property.type);
        if (!property.id || property.size != size || property.default_bytes.size() != size || property.offset % alignment != 0 || property.offset > plan.parameter_size || size > plan.parameter_size - property.offset
            || !property_ids.insert(property.id).second)
            return Err(ErrorCode::ParseInvalidFormat, "material property layout is invalid");
    }
    for (const auto& texture : plan.textures)
        if (!texture.id || !texture.default_asset || !property_ids.insert(texture.id).second || !bindings.insert(texture.binding).second)
            return Err(ErrorCode::ParseInvalidFormat, "material texture layout is invalid");
    for (const auto& sampler : plan.samplers)
        if (!sampler.id || !property_ids.insert(sampler.id).second || !bindings.insert(sampler.binding).second)
            return Err(ErrorCode::ParseInvalidFormat, "material sampler layout is invalid");
    ShaderInterface layout_interface;
    for (const auto& group : plan.pipeline_layout.groups)
        layout_interface.bindings.insert(layout_interface.bindings.end(), group.bindings.begin(), group.bindings.end());
    auto recomputed_layout = MakePipelineLayoutKey(layout_interface, abi::kDynamicBufferPolicy);
    if (!recomputed_layout || recomputed_layout->hash != plan.pipeline_layout.hash || *recomputed_layout != plan.pipeline_layout)
        return Err(ErrorCode::ParseInvalidFormat, "material pipeline layout hash is inconsistent");
    for (const auto& variant : plan.variants) {
        const auto property = std::ranges::find(plan.properties, variant.property, &MaterialPropertyPlan::id);
        if (property == plan.properties.end() || property->type != variant.type || variant.allowed_values.empty()
            || std::ranges::any_of(variant.allowed_values, [&](const MaterialValue& value) { return !MaterialValueMatches(variant.type, value); }))
            return Err(ErrorCode::ParseInvalidFormat, "material specialization mapping is invalid");
    }
    return Ok(std::move(result));
}

Result<std::vector<std::byte>> SerializeMaterialInstance(const MaterialInstanceProduct& product) {
    if (!product.id || !product.definition || product.overrides.size() > kMaxMaterialProperties)
        return Err(ErrorCode::ValidationInvalidState, "material instance product is invalid");
    Writer out;
    out.bytes.insert(out.bytes.end(), kInstanceMagic.begin(), kInstanceMagic.end());
    out.Int(kMaterialProductVersion);
    out.Id(product.id);
    out.Id(product.definition);
    out.Hash(product.definition_product_hash);
    out.Int(static_cast<u32>(product.overrides.size()));
    for (const auto& [id, value] : product.overrides) {
        const auto type = product.override_types.find(id);
        if (type == product.override_types.end() || !MaterialValueMatches(type->second, value))
            return Err(ErrorCode::ValidationInvalidState, "material instance override type metadata is missing");
        out.Int(id.Value());
        WriteValue(out, type->second, value);
    }
    return Ok(std::move(out.bytes));
}

Result<MaterialInstanceProduct> ParseMaterialInstanceProduct(const std::span<const std::byte> bytes) {
    if (bytes.size() < 4 || !std::ranges::equal(kInstanceMagic, bytes.first(4)))
        return Err(ErrorCode::ParseInvalidFormat, "material instance magic is invalid");
    Reader in(bytes.subspan(4));
    MaterialInstanceProduct result;
    u32 version{}, count{};
    if (!in.Int(version) || version != kMaterialProductVersion || !in.Id(result.id) || !in.Id(result.definition) || !in.Hash(result.definition_product_hash) || !in.Int(count) || count > kMaxMaterialProperties)
        return Err(ErrorCode::ParseInvalidFormat, "material instance header is invalid");
    for (u32 i = 0; i < count; ++i) {
        u32 id{};
        MaterialValue value;
        if (!in.Int(id))
            return Err(ErrorCode::ParseInvalidFormat, "material instance override is invalid");
        u8 raw{};
        if (!in.Int(raw) || raw > static_cast<u8>(MaterialValueType::Sampler))
            return Err(ErrorCode::ParseInvalidFormat, "material instance override type is invalid");
        const auto type = static_cast<MaterialValueType>(raw);
        if (type >= MaterialValueType::Texture2D) {
            asset::AssetId asset;
            if (!in.Id(asset) || !asset)
                return Err(ErrorCode::ParseInvalidFormat, "material resource override is invalid");
            value = asset;
        } else {
            std::vector<std::byte> packed;
            if (!in.Raw(packed))
                return Err(ErrorCode::ParseInvalidFormat, "material value bytes are invalid");
            Writer encoded;
            encoded.Int(raw);
            encoded.Raw(packed);
            Reader nested(encoded.bytes);
            if (!ReadValue(nested, value) || !nested.End())
                return Err(ErrorCode::ParseInvalidFormat, "material value is invalid");
        }
        const MaterialPropertyId property(id);
        if (!result.overrides.emplace(property, std::move(value)).second)
            return Err(ErrorCode::ParseInvalidFormat, "material instance override is invalid");
        result.override_types.emplace(property, type);
    }
    if (!in.End())
        return Err(ErrorCode::ParseInvalidFormat, "material instance contains trailing bytes");
    return Ok(std::move(result));
}

Result<asset::Product> MakeMaterialDefinitionProduct(const MaterialDefinitionProduct& definition, ContentHash source_hash, std::vector<asset::ProductDependency> dependencies) {
    auto bytes = SerializeMaterialDefinition(definition);
    if (!bytes)
        return Err(std::move(bytes).error());
    std::ranges::sort(dependencies);
    return Ok(asset::MakeProduct(definition.id, kMaterialDefinitionProductType, kMaterialProductVersion, 1, source_hash, std::move(dependencies), Sha256("woki.gfx.material.v1"), std::move(*bytes)));
}

Result<asset::Product> MakeMaterialInstanceProduct(const MaterialInstanceProduct& instance, ContentHash source_hash, std::vector<asset::ProductDependency> dependencies) {
    auto bytes = SerializeMaterialInstance(instance);
    if (!bytes)
        return Err(std::move(bytes).error());
    std::ranges::sort(dependencies);
    return Ok(asset::MakeProduct(instance.id, kMaterialInstanceProductType, kMaterialProductVersion, 1, source_hash, std::move(dependencies), Sha256("woki.gfx.material.v1"), std::move(*bytes)));
}

MaterialTypeBuilder::MaterialTypeBuilder(ResolveProduct resolve)
    : resolve_(std::move(resolve)),
      descriptor_{asset::AssetId::FromName("woki.builder.material-type.v1"), 1, kMaterialTypeSourceType, kMaterialDefinitionProductType, "Material type"} {}

const asset::BuilderDescriptor& MaterialTypeBuilder::Descriptor() const noexcept {
    return descriptor_;
}

Result<asset::Product> MaterialTypeBuilder::Build(const asset::BuildRequest& request, asset::BuildContext& context, const std::span<const std::byte> bytes) const {
    const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    MaterialTypeSource source;
    TRY_ASSIGN(source, ParseMaterialType(text));
    if (source.id != request.asset_id)
        return Err(ErrorCode::ValidationInvalidState, "material type source ID differs from build request");
    asset::Product shader;
    TRY_ASSIGN(shader, resolve_(source.shader));
    TRY_VOID(asset::ValidateProduct(shader));
    if (shader.type != kShaderProductType)
        return Err(ErrorCode::ValidationInvalidState, "material shader dependency is not a shader product");
    ShaderPayload payload;
    TRY_ASSIGN(payload, ParseShaderPayload(shader.payload));
    auto compiled = CompileMaterialType(source, payload, shader.product_hash);
    if (!compiled.Valid())
        return Err(ErrorCode::ValidationInvalidState, compiled.diagnostics.empty() ? "material compilation failed" : compiled.diagnostics.front().message);
    TRY_VOID(context.AddProductDependency({shader.asset_id, shader.product_hash}));
    std::vector dependencies{asset::ProductDependency{shader.asset_id, shader.product_hash}};
    for (const auto& texture : compiled.definition->binding_plan.textures) {
        if (texture.default_asset == BuiltinFallbackTextureId(texture.semantic))
            continue;
        asset::ProductDependency dependency{texture.default_asset, {}};
        asset::Product texture_product;
        TRY_ASSIGN(texture_product, resolve_(texture.default_asset));
        TRY_VOID(asset::ValidateProduct(texture_product));
        if (texture_product.type != kTextureProductType)
            return Err(ErrorCode::ValidationInvalidState, "material default texture dependency is not a texture product");
        dependency.product_hash = texture_product.product_hash;
        TRY_VOID(context.AddProductDependency(dependency));
        if (std::ranges::find(dependencies, dependency.asset_id, &asset::ProductDependency::asset_id) == dependencies.end())
            dependencies.push_back(dependency);
    }
    return MakeMaterialDefinitionProduct(*compiled.definition, request.source_hash, std::move(dependencies));
}

MaterialInstanceBuilder::MaterialInstanceBuilder(ResolveProduct resolve)
    : resolve_(std::move(resolve)),
      descriptor_{asset::AssetId::FromName("woki.builder.material-instance.v1"), 1, kMaterialInstanceSourceType, kMaterialInstanceProductType, "Material instance"} {}

const asset::BuilderDescriptor& MaterialInstanceBuilder::Descriptor() const noexcept {
    return descriptor_;
}

Result<asset::Product> MaterialInstanceBuilder::Build(const asset::BuildRequest& request, asset::BuildContext& context, const std::span<const std::byte> bytes) const {
    const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    // Read the type ID without weakening the strict instance parser.
    auto parsed_root = config::Json::Parse(text, "material-instance");
    if (!parsed_root)
        return Err(ErrorCode::ParseInvalidFormat, config::FormatDiagnostics(parsed_root.error()));
    const config::Json root = std::move(*parsed_root);
    if (!root.is_object() || !root.contains("type") || !root["type"].is_string())
        return Err(ErrorCode::ParseInvalidFormat, "material instance type is missing");
    asset::AssetId type;
    TRY_ASSIGN(type, asset::AssetId::Parse(root["type"].get_ref<const std::string&>()));
    asset::Product product;
    TRY_ASSIGN(product, resolve_(type));
    TRY_VOID(asset::ValidateProduct(product));
    if (product.type != kMaterialDefinitionProductType)
        return Err(ErrorCode::ValidationInvalidState, "material type dependency is not a definition product");
    MaterialDefinitionProduct definition;
    TRY_ASSIGN(definition, ParseMaterialDefinition(product.payload));
    MaterialTypeSource shape;
    shape.id = definition.id;
    for (const auto& item : definition.binding_plan.properties)
        shape.properties.push_back({item.id, {}, item.type, {}, item.offset});
    for (const auto& item : definition.binding_plan.textures)
        shape.textures.push_back({item.id, {}, item.type, item.binding, item.semantic, item.default_asset});
    // Preserve stable IDs while allowing the strict parser to resolve authored names.
    for (auto& item : shape.properties)
        for (const auto& [name, unused] : root["overrides"].items())
            if (MaterialPropertyId::FromName(name) == item.id)
                item.name = name;
    for (auto& item : shape.textures)
        for (const auto& [name, unused] : root["overrides"].items())
            if (MaterialPropertyId::FromName(name) == item.id)
                item.name = name;
    MaterialInstanceSource source;
    TRY_ASSIGN(source, ParseMaterialInstance(text, shape));
    if (source.id != request.asset_id)
        return Err(ErrorCode::ValidationInvalidState, "material instance source ID differs from build request");
    MaterialInstanceProduct instance;
    TRY_ASSIGN(instance, CompileMaterialInstance(source, definition, product.product_hash));
    TRY_VOID(context.AddProductDependency({product.asset_id, product.product_hash}));
    std::vector dependencies{asset::ProductDependency{product.asset_id, product.product_hash}};
    for (const auto& [id, value] : instance.overrides) {
        const auto value_type = instance.override_types.at(id);
        if (value_type != MaterialValueType::Texture2D && value_type != MaterialValueType::TextureCube)
            continue;
        const auto texture_id = std::get<asset::AssetId>(value);
        asset::ProductDependency dependency{texture_id, {}};
        const auto definition_binding = std::ranges::find(definition.binding_plan.textures, id, &MaterialTextureBinding::id);
        const auto semantic = definition_binding->semantic;
        if (texture_id == BuiltinFallbackTextureId(semantic))
            dependency.product_hash = BuiltinFallbackTextureProductHash(semantic);
        else {
            asset::Product texture;
            TRY_ASSIGN(texture, resolve_(texture_id));
            TRY_VOID(asset::ValidateProduct(texture));
            if (texture.type != kTextureProductType)
                return Err(ErrorCode::ValidationInvalidState, "material override dependency is not a texture product");
            dependency.product_hash = texture.product_hash;
        }
        TRY_VOID(context.AddProductDependency(dependency));
        dependencies.push_back(dependency);
    }
    return MakeMaterialInstanceProduct(instance, request.source_hash, std::move(dependencies));
}

Result<void> RegisterMaterialBuilders(asset::BuilderRegistry& registry, MaterialTypeBuilder::ResolveProduct resolve) {
    TRY_VOID(registry.Register(createRef<MaterialTypeBuilder>(resolve)));
    TRY_VOID(registry.Register(createRef<MaterialInstanceBuilder>(std::move(resolve))));
    return Ok();
}

Result<void> RegisterGraphicsAssetBuilders(asset::AssetServices& services, ref<const asset::Vfs> vfs, MaterialTypeBuilder::ResolveProduct resolve) {
    if (services.builders == nullptr)
        return Err(ErrorCode::InvalidArgument, "asset services have no builder registry");
#ifndef __EMSCRIPTEN__
    if (vfs == nullptr)
        return Err(ErrorCode::InvalidArgument, "graphics builder registration requires a VFS");
    TRY_VOID(services.builders->Register(createRef<TextureBuilder>(vfs)));
    ModelImporterRegistry importer_registry;
    TRY_ASSIGN(importer_registry, CreateDefaultModelImporterRegistry());
    TRY_VOID(RegisterMeshAssetBuilder(services, vfs, createRef<const ModelImporterRegistry>(std::move(importer_registry)), resolve));
#else
    static_cast<void>(vfs);
#endif
    return RegisterMaterialBuilders(*services.builders, std::move(resolve));
}

} // namespace woki::gfx
