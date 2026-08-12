#include <woki/gfx/compiler.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>

#include "src/tint/api/tint.h"
#include "src/tint/lang/wgsl/inspector/inspector.h"
#include "src/tint/lang/wgsl/reader/reader.h"
#include "src/tint/utils/text/string_stream.h"

namespace woki::gfx {
namespace {

struct TintRuntime {
    TintRuntime() {
        tint::Initialize();
    }

    ~TintRuntime() {
        tint::Shutdown();
    }
};

u8 StageBit(const ShaderStage stage) {
    return static_cast<u8>(1U << static_cast<u8>(stage));
}

ShaderStage Stage(const tint::inspector::PipelineStage stage) {
    switch (stage) {
        case tint::inspector::PipelineStage::kVertex:
            return ShaderStage::Vertex;
        case tint::inspector::PipelineStage::kFragment:
            return ShaderStage::Fragment;
        case tint::inspector::PipelineStage::kCompute:
            return ShaderStage::Compute;
    }
    return ShaderStage::Vertex;
}

ValueType Type(const tint::inspector::ComponentType component, const tint::inspector::CompositionType composition) {
    using C = tint::inspector::ComponentType;
    using N = tint::inspector::CompositionType;
    if (composition == N::kScalar)
        return component == C::kF32 ? ValueType::F32 : component == C::kF16 ? ValueType::F16 : component == C::kI32 ? ValueType::I32 : component == C::kU32 ? ValueType::U32 : ValueType::Unknown;
    if (component == C::kF32)
        return composition == N::kVec2 ? ValueType::Vec2F : composition == N::kVec3 ? ValueType::Vec3F : composition == N::kVec4 ? ValueType::Vec4F : ValueType::Unknown;
    if (component == C::kI32)
        return composition == N::kVec2 ? ValueType::Vec2I : composition == N::kVec3 ? ValueType::Vec3I : composition == N::kVec4 ? ValueType::Vec4I : ValueType::Unknown;
    if (component == C::kU32)
        return composition == N::kVec2 ? ValueType::Vec2U : composition == N::kVec3 ? ValueType::Vec3U : composition == N::kVec4 ? ValueType::Vec4U : ValueType::Unknown;
    return ValueType::Unknown;
}

TextureDimension Dimension(const tint::inspector::ResourceBinding::TextureDimension value) {
    using D = tint::inspector::ResourceBinding::TextureDimension;
    switch (value) {
        case D::k1d:
            return TextureDimension::D1;
        case D::k2d:
            return TextureDimension::D2;
        case D::k2dArray:
            return TextureDimension::D2Array;
        case D::k3d:
            return TextureDimension::D3;
        case D::kCube:
            return TextureDimension::Cube;
        case D::kCubeArray:
            return TextureDimension::CubeArray;
        case D::kNone:
            return TextureDimension::None;
    }
    return TextureDimension::None;
}

SampleType Sample(const tint::inspector::ResourceBinding::SampledKind value) {
    using S = tint::inspector::ResourceBinding::SampledKind;
    switch (value) {
        case S::kUInt:
            return SampleType::Uint;
        case S::kSInt:
            return SampleType::Sint;
        case S::kFloat:
            return SampleType::Float;
        case S::kUnknown:
            return SampleType::None;
    }
    return SampleType::None;
}

StorageFormat Format(const tint::inspector::ResourceBinding::TexelFormat value) {
    using F = tint::inspector::ResourceBinding::TexelFormat;
    switch (value) {
        case F::kR32Uint:
            return StorageFormat::R32Uint;
        case F::kR32Sint:
            return StorageFormat::R32Sint;
        case F::kR32Float:
            return StorageFormat::R32Float;
        case F::kRg32Uint:
            return StorageFormat::RG32Uint;
        case F::kRg32Sint:
            return StorageFormat::RG32Sint;
        case F::kRg32Float:
            return StorageFormat::RG32Float;
        case F::kRgba8Unorm:
            return StorageFormat::RGBA8Unorm;
        case F::kRgba8Snorm:
            return StorageFormat::RGBA8Snorm;
        case F::kRgba8Uint:
            return StorageFormat::RGBA8Uint;
        case F::kRgba8Sint:
            return StorageFormat::RGBA8Sint;
        case F::kBgra8Unorm:
            return StorageFormat::BGRA8Unorm;
        case F::kRgba16Uint:
            return StorageFormat::RGBA16Uint;
        case F::kRgba16Sint:
            return StorageFormat::RGBA16Sint;
        case F::kRgba16Float:
            return StorageFormat::RGBA16Float;
        case F::kRgba32Uint:
            return StorageFormat::RGBA32Uint;
        case F::kRgba32Sint:
            return StorageFormat::RGBA32Sint;
        case F::kRgba32Float:
            return StorageFormat::RGBA32Float;
        default:
            return StorageFormat::None;
    }
}

BindingInfo Binding(const tint::inspector::ResourceBinding& source, const ShaderStage stage) {
    using R = tint::inspector::ResourceBinding;
    BindingInfo result{.group = source.bind_group, .binding = source.binding, .stages = StageBit(stage), .min_binding_size = source.size, .array_size = source.array_size.value_or(0)};
    switch (source.resource_type) {
        case R::ResourceType::kUniformBuffer:
            result.kind = ResourceKind::UniformBuffer;
            break;
        case R::ResourceType::kStorageBuffer:
            result.kind = ResourceKind::StorageBuffer;
            break;
        case R::ResourceType::kReadOnlyStorageBuffer:
            result.kind = ResourceKind::ReadOnlyStorageBuffer;
            break;
        case R::ResourceType::kSampler:
            result.kind = ResourceKind::Sampler;
            break;
        case R::ResourceType::kComparisonSampler:
            result.kind = ResourceKind::ComparisonSampler;
            break;
        case R::ResourceType::kSampledTexture:
            result.kind = ResourceKind::SampledTexture;
            result.dimension = Dimension(source.dim);
            result.sample_type = Sample(source.sampled_kind);
            break;
        case R::ResourceType::kMultisampledTexture:
            result.kind = ResourceKind::MultisampledTexture;
            result.dimension = Dimension(source.dim);
            result.sample_type = Sample(source.sampled_kind);
            break;
        case R::ResourceType::kDepthTexture:
            result.kind = ResourceKind::DepthTexture;
            result.dimension = Dimension(source.dim);
            result.sample_type = SampleType::Depth;
            break;
        case R::ResourceType::kDepthMultisampledTexture:
            result.kind = ResourceKind::MultisampledTexture;
            result.dimension = Dimension(source.dim);
            result.sample_type = SampleType::Depth;
            break;
        case R::ResourceType::kWriteOnlyStorageTexture:
            result.kind = ResourceKind::StorageTexture;
            result.dimension = Dimension(source.dim);
            result.storage_access = StorageAccess::WriteOnly;
            result.storage_format = Format(source.image_format);
            break;
        case R::ResourceType::kReadOnlyStorageTexture:
            result.kind = ResourceKind::StorageTexture;
            result.dimension = Dimension(source.dim);
            result.storage_access = StorageAccess::ReadOnly;
            result.storage_format = Format(source.image_format);
            break;
        case R::ResourceType::kReadWriteStorageTexture:
            result.kind = ResourceKind::StorageTexture;
            result.dimension = Dimension(source.dim);
            result.storage_access = StorageAccess::ReadWrite;
            result.storage_format = Format(source.image_format);
            break;
        case R::ResourceType::kExternalTexture:
            result.kind = ResourceKind::ExternalTexture;
            break;
        default:
            break;
    }
    return result;
}

u64 ByteOffset(const std::string_view source, const tint::Source::Location location) {
    if (location.line == 0 || location.column == 0)
        return 0;
    u64 offset = 0;
    for (u32 line = 1; line < location.line && offset < source.size(); ++line) {
        const auto newline = source.find('\n', offset);
        offset = newline == std::string_view::npos ? source.size() : newline + 1;
    }
    return std::min<u64>(source.size(), offset + location.column - 1);
}

void AddDiagnostics(CompileOutput& output, const CompileRequest& request, const tint::diag::List& diagnostics) {
    for (const auto& diagnostic : diagnostics) {
        const u64 begin = ByteOffset(request.source.code, diagnostic.source.range.begin);
        const u64 end = ByteOffset(request.source.code, diagnostic.source.range.end);
        const DiagnosticSeverity tint_severity = diagnostic.severity == tint::diag::Severity::Note      ? DiagnosticSeverity::Note
                                                 : diagnostic.severity == tint::diag::Severity::Warning ? DiagnosticSeverity::Warning
                                                                                                        : DiagnosticSeverity::Error;
        const DiagnosticSeverity severity = tint_severity == DiagnosticSeverity::Warning && request.descriptor.compile_options.warnings_as_errors ? DiagnosticSeverity::Error : tint_severity;
        SourceRange range = MapGeneratedRange(request.source, begin, end > begin ? end - begin : 1);
        if (!range.path) {
            range.line = diagnostic.source.range.begin.line;
            range.column = diagnostic.source.range.begin.column;
        }
        output.diagnostics.push_back({"SHD3002", severity, diagnostic.message.Plain(), std::move(range), {}});
    }
}

bool VariantTypeMatches(const PermutationValue& value, const ValueType type) {
    return std::visit(
        [type](const auto& scalar) {
            using T = std::decay_t<decltype(scalar)>;
            if constexpr (std::same_as<T, bool>)
                return type == ValueType::Bool;
            if constexpr (std::same_as<T, i64>)
                return (type == ValueType::I32 && scalar >= std::numeric_limits<i32>::min() && scalar <= std::numeric_limits<i32>::max())
                       || (type == ValueType::U32 && scalar >= 0 && static_cast<u64>(scalar) <= std::numeric_limits<u32>::max());
            if constexpr (std::same_as<T, f64>)
                return std::isfinite(scalar) && (type == ValueType::F32 || type == ValueType::F16);
            return false;
        },
        value
    );
}

class TintCompiler final : public ShaderCompiler {
public:
    CompileOutput Compile(const CompileRequest& request) const override {
        static const TintRuntime runtime;
        static_cast<void>(runtime);
        CompileOutput output;
        output.code = request.source.code;
        tint::Source::File file(request.descriptor.name, output.code);
        tint::Program program = tint::wgsl::reader::Parse(&file);
        AddDiagnostics(output, request, program.Diagnostics());
        if (!program.IsValid()) {
            return output;
        }
        tint::inspector::Inspector inspector(program);
        const auto entries = inspector.GetEntryPoints();
        if (inspector.has_error()) {
            output.diagnostics.push_back({"SHD3003", DiagnosticSeverity::Error, inspector.error(), {}, {}});
            return output;
        }
        std::set<std::pair<ShaderStage, std::string>> reflected;
        std::set<std::pair<ShaderStage, std::string>> selected;
        for (const auto& entry : request.descriptor.entry_points)
            selected.emplace(entry.stage, entry.name);
        for (const auto& source : entries) {
            EntryPointInfo entry;
            entry.name = source.name;
            entry.stage = Stage(source.stage);
            reflected.emplace(entry.stage, entry.name);
            if (!selected.contains({entry.stage, entry.name}))
                continue;
            for (const auto& input : source.input_variables)
                if (input.attributes.location)
                    entry.inputs.push_back({*input.attributes.location, Type(input.component_type, input.composition_type)});
            for (const auto& result : source.output_variables)
                if (result.attributes.location)
                    entry.outputs.push_back({*result.attributes.location, Type(result.component_type, result.composition_type)});
            if (source.workgroup_size)
                entry.workgroup_size = {source.workgroup_size->x, source.workgroup_size->y, source.workgroup_size->z};
            for (const auto& binding : inspector.GetResourceBindings(source.name)) {
                if (binding.resource_type == tint::inspector::ResourceBinding::ResourceType::kExternalTexture) {
                    output.diagnostics.push_back({"SHD3007", DiagnosticSeverity::Error, "external-texture bindings are unsupported by the RHI layout model", {}, {}});
                    continue;
                }
                output.interface.bindings.push_back(Binding(binding, entry.stage));
            }
            for (const auto& override_value : source.overrides) {
                ValueType type = override_value.type == tint::inspector::Override::Type::kBool      ? ValueType::Bool
                                 : override_value.type == tint::inspector::Override::Type::kFloat32 ? ValueType::F32
                                 : override_value.type == tint::inspector::Override::Type::kFloat16 ? ValueType::F16
                                 : override_value.type == tint::inspector::Override::Type::kInt32   ? ValueType::I32
                                                                                                    : ValueType::U32;
                output.interface.overrides.push_back({override_value.name, override_value.id.value, type, override_value.is_initialized});
            }
            output.interface.entry_points.push_back(std::move(entry));
        }
        for (const auto& descriptor_entry : request.descriptor.entry_points)
            if (!reflected.contains({descriptor_entry.stage, descriptor_entry.name}))
                output.diagnostics.push_back({"SHD3004", DiagnosticSeverity::Error, "descriptor entry point was not found with the declared stage: " + descriptor_entry.name, {}, {}});
        output.interface.capabilities = request.descriptor.capabilities;
        NormalizeInterface(output.interface);
        for (const auto& permutation : request.descriptor.permutations) {
            const auto override_value = std::ranges::find(output.interface.overrides, permutation.name, &OverrideInfo::name);
            if (override_value == output.interface.overrides.end()) {
                output.diagnostics.push_back({"SHD3008", DiagnosticSeverity::Error, "permutation is not backed by a selected-entry WGSL override: " + permutation.name, {}, {}});
                continue;
            }
            if (std::ranges::any_of(permutation.values, [&](const auto& value) { return !VariantTypeMatches(value, override_value->type); }))
                output.diagnostics.push_back({"SHD3009", DiagnosticSeverity::Error, "permutation values do not match the WGSL override type: " + permutation.name, {}, {}});
        }
        for (std::size_t i = 1; i < output.interface.bindings.size(); ++i) {
            const auto& previous = output.interface.bindings[i - 1];
            const auto& current = output.interface.bindings[i];
            if (previous.group == current.group && previous.binding == current.binding)
                output.diagnostics.push_back({"SHD3006", DiagnosticSeverity::Error,
                    "entry points declare incompatible resources at binding " + std::to_string(current.group) + ":" + std::to_string(current.binding) + " (kind " + std::to_string(static_cast<u32>(previous.kind)) + "/"
                        + std::to_string(static_cast<u32>(current.kind)) + ", minimum size " + std::to_string(previous.min_binding_size) + "/" + std::to_string(current.min_binding_size) + ", dimension "
                        + std::to_string(static_cast<u32>(previous.dimension)) + "/" + std::to_string(static_cast<u32>(current.dimension)) + ", sample " + std::to_string(static_cast<u32>(previous.sample_type)) + "/"
                        + std::to_string(static_cast<u32>(current.sample_type)) + ", access " + std::to_string(static_cast<u32>(previous.storage_access)) + "/" + std::to_string(static_cast<u32>(current.storage_access))
                        + ", format " + std::to_string(static_cast<u32>(previous.storage_format)) + "/" + std::to_string(static_cast<u32>(current.storage_format)) + ", array " + std::to_string(previous.array_size) + "/"
                        + std::to_string(current.array_size) + ")",
                    {}, {}});
        }
        output.validated_with_tint = std::ranges::none_of(output.diagnostics, [](const ShaderDiagnostic& value) { return value.severity == DiagnosticSeverity::Error; });
        return output;
    }
};

} // namespace

scope<ShaderCompiler> CreateTintShaderCompiler() {
    return createScope<TintCompiler>();
}

bool IsTintShaderCompilerAvailable() noexcept {
    return true;
}

} // namespace woki::gfx
