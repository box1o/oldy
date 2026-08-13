#include <bit>
#include <tuple>
#include <cstddef>
#include <algorithm>
#include <sstream>

#include <woki/gfx/advanced/reflection.hpp>

namespace woki::gfx {
namespace {

template <typename T>
void Write(std::vector<std::byte>& bytes, T value) {
    using U = std::make_unsigned_t<T>;
    U bits = static_cast<U>(value);
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        bytes.push_back(static_cast<std::byte>((bits >> (i * 8U)) & 0xffU));
    }
}

void String(std::vector<std::byte>& bytes, const std::string_view value) {
    Write(bytes, static_cast<u32>(value.size()));
    for (const char character : value)
        bytes.push_back(static_cast<std::byte>(static_cast<u8>(character)));
}

void Member(std::vector<std::byte>& bytes, const BufferMemberLayout& member) {
    String(bytes, member.name);
    String(bytes, member.type);
    Write(bytes, member.offset);
    Write(bytes, member.size);
    Write(bytes, member.alignment);
    Write(bytes, member.array_stride);
    Write(bytes, member.matrix_stride);
    Write(bytes, static_cast<u32>(member.members.size()));
    for (const auto& nested : member.members)
        Member(bytes, nested);
}

std::string BindingSummary(const BindingInfo& binding) {
    return std::to_string(static_cast<u32>(binding.kind)) + "/" + std::to_string(binding.min_binding_size) + "/" + binding.semantic;
}

} // namespace

void NormalizeInterface(ShaderInterface& interface) {
    std::ranges::sort(interface.entry_points, {}, [](const EntryPointInfo& entry) { return std::pair{entry.stage, entry.name}; });
    for (auto& entry : interface.entry_points) {
        std::ranges::sort(entry.inputs, {}, &StageIo::location);
        std::ranges::sort(entry.outputs, {}, &StageIo::location);
    }
    std::ranges::sort(interface.bindings, {}, [](const BindingInfo& binding) { return std::pair{binding.group, binding.binding}; });
    std::vector<BindingInfo> merged;
    for (const BindingInfo& binding : interface.bindings) {
        if (!merged.empty() && merged.back().group == binding.group && merged.back().binding == binding.binding && merged.back().kind == binding.kind && merged.back().dimension == binding.dimension
            && merged.back().sample_type == binding.sample_type && merged.back().storage_access == binding.storage_access && merged.back().storage_format == binding.storage_format
            && merged.back().min_binding_size == binding.min_binding_size && merged.back().array_size == binding.array_size && merged.back().semantic == binding.semantic
            && merged.back().group_semantic == binding.group_semantic && merged.back().buffer_type == binding.buffer_type && merged.back().buffer_members == binding.buffer_members) {
            merged.back().stages |= binding.stages;
            merged.back().visible_entries.insert(merged.back().visible_entries.end(), binding.visible_entries.begin(), binding.visible_entries.end());
        } else {
            merged.push_back(binding);
        }
    }
    for (auto& binding : merged) {
        std::ranges::sort(binding.visible_entries);
        binding.visible_entries.erase(std::ranges::unique(binding.visible_entries).begin(), binding.visible_entries.end());
    }
    interface.bindings = std::move(merged);
    std::ranges::sort(interface.overrides, {}, [](const OverrideInfo& value) { return std::pair{value.id, value.name}; });
    interface.overrides.erase(std::ranges::unique(interface.overrides, {}, [](const OverrideInfo& value) { return std::tuple{value.id, value.name, value.type, value.has_default}; }).begin(), interface.overrides.end());
    std::ranges::sort(interface.capabilities);
    interface.capabilities.erase(std::ranges::unique(interface.capabilities).begin(), interface.capabilities.end());
    interface.hash = HashInterface(interface);
}

ContentHash HashInterface(const ShaderInterface& interface) {
    std::vector<std::byte> bytes;
    Write(bytes, static_cast<u32>(interface.entry_points.size()));
    for (const auto& entry : interface.entry_points) {
        String(bytes, entry.name);
        String(bytes, entry.semantic);
        Write(bytes, static_cast<u8>(entry.stage));
        Write(bytes, static_cast<u32>(entry.inputs.size()));
        for (const auto& io : entry.inputs) {
            Write(bytes, io.location);
            Write(bytes, static_cast<u8>(io.type));
        }
        Write(bytes, static_cast<u32>(entry.outputs.size()));
        for (const auto& io : entry.outputs) {
            Write(bytes, io.location);
            Write(bytes, static_cast<u8>(io.type));
        }
        for (const u32 size : entry.workgroup_size)
            Write(bytes, size);
    }
    Write(bytes, static_cast<u32>(interface.bindings.size()));
    for (const auto& binding : interface.bindings) {
        Write(bytes, binding.group);
        Write(bytes, binding.binding);
        Write(bytes, binding.stages);
        Write(bytes, static_cast<u8>(binding.kind));
        Write(bytes, static_cast<u8>(binding.dimension));
        Write(bytes, static_cast<u8>(binding.sample_type));
        Write(bytes, static_cast<u8>(binding.storage_access));
        Write(bytes, static_cast<u8>(binding.storage_format));
        Write(bytes, binding.min_binding_size);
        Write(bytes, binding.array_size);
        String(bytes, binding.semantic);
        String(bytes, binding.group_semantic);
        Write(bytes, static_cast<u32>(binding.visible_entries.size()));
        for (const auto& entry : binding.visible_entries)
            String(bytes, entry);
        String(bytes, binding.buffer_type);
        Write(bytes, static_cast<u32>(binding.buffer_members.size()));
        for (const auto& member : binding.buffer_members)
            Member(bytes, member);
    }
    Write(bytes, static_cast<u32>(interface.overrides.size()));
    for (const auto& override_value : interface.overrides) {
        String(bytes, override_value.name);
        Write(bytes, override_value.id);
        Write(bytes, static_cast<u8>(override_value.type));
        Write(bytes, static_cast<u8>(override_value.has_default));
    }
    Write(bytes, static_cast<u32>(interface.capabilities.size()));
    for (const auto& capability : interface.capabilities)
        String(bytes, capability);
    return Sha256(bytes);
}

InterfaceDiff DiffInterfaces(const ShaderInterface& expected_input, const ShaderInterface& actual_input) {
    ShaderInterface expected = expected_input;
    ShaderInterface actual = actual_input;
    NormalizeInterface(expected);
    NormalizeInterface(actual);
    InterfaceDiff result{expected.hash, actual.hash, {}};
    for (const auto& binding : expected.bindings) {
        const auto found = std::ranges::find_if(actual.bindings, [&](const BindingInfo& value) { return value.group == binding.group && value.binding == binding.binding; });
        const std::string path = "bindings/" + std::to_string(binding.group) + ":" + std::to_string(binding.binding);
        if (found == actual.bindings.end())
            result.differences.push_back({InterfaceDifferenceKind::Missing, path, BindingSummary(binding), {}});
        else if (*found != binding)
            result.differences.push_back({InterfaceDifferenceKind::Changed, path, BindingSummary(binding), BindingSummary(*found)});
    }
    for (const auto& binding : actual.bindings)
        if (std::ranges::none_of(expected.bindings, [&](const BindingInfo& value) { return value.group == binding.group && value.binding == binding.binding; }))
            result.differences.push_back({InterfaceDifferenceKind::Added, "bindings/" + std::to_string(binding.group) + ":" + std::to_string(binding.binding), {}, BindingSummary(binding)});
    for (const auto& entry : expected.entry_points) {
        const auto found = std::ranges::find_if(actual.entry_points, [&](const EntryPointInfo& value) { return value.stage == entry.stage && value.name == entry.name; });
        if (found == actual.entry_points.end())
            result.differences.push_back({InterfaceDifferenceKind::Missing, "entries/" + entry.name, entry.semantic, {}});
        else if (*found != entry)
            result.differences.push_back({InterfaceDifferenceKind::Changed, "entries/" + entry.name, entry.semantic, found->semantic});
    }
    for (const auto& entry : actual.entry_points)
        if (std::ranges::none_of(expected.entry_points, [&](const EntryPointInfo& value) { return value.stage == entry.stage && value.name == entry.name; }))
            result.differences.push_back({InterfaceDifferenceKind::Added, "entries/" + entry.name, {}, entry.semantic});
    if (expected.overrides != actual.overrides)
        result.differences.push_back({InterfaceDifferenceKind::Changed, "overrides", std::to_string(expected.overrides.size()), std::to_string(actual.overrides.size())});
    if (expected.capabilities != actual.capabilities)
        result.differences.push_back({InterfaceDifferenceKind::Changed, "capabilities", std::to_string(expected.capabilities.size()), std::to_string(actual.capabilities.size())});
    return result;
}

Result<void> RequireInterface(const ShaderInterface& actual_input, const ContentHash required_hash) {
    ShaderInterface actual = actual_input;
    NormalizeInterface(actual);
    if (actual.hash != required_hash)
        return Err(ErrorCode::ValidationInvalidState, "shader interface hash mismatch: required " + required_hash.Hex() + ", actual " + actual.hash.Hex());
    return Ok();
}

Result<void> RequireInterface(const ShaderInterface& actual, const ShaderInterface& required) {
    const auto diff = DiffInterfaces(required, actual);
    if (diff.Compatible())
        return Ok();
    std::ostringstream message;
    message << "shader interface mismatch (" << diff.expected_hash.Hex() << " -> " << diff.actual_hash.Hex() << ")";
    for (const auto& item : diff.differences)
        message << "; " << item.path << " expected='" << item.expected << "' actual='" << item.actual << "'";
    return Err(ErrorCode::ValidationInvalidState, message.str());
}

} // namespace woki::gfx
