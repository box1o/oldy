#include <woki/gfx/reflection.hpp>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <tuple>

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
            && merged.back().min_binding_size == binding.min_binding_size && merged.back().array_size == binding.array_size) {
            merged.back().stages |= binding.stages;
        } else {
            merged.push_back(binding);
        }
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

} // namespace woki::gfx
