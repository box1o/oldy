#include <span>
#include <array>
#include <limits>
#include <ranges>
#include <string>
#include <vector>
#include <fstream>
#include <optional>
#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "woki/ext/limits.hpp"
#include "woki/ext/wasm/guest_module.hpp"

#if defined(WOKI_EXTENSION_WITH_WASMTIME)
#include <wasmtime.hh>
#endif

namespace woki::ext::wasm {

namespace {

namespace fs = std::filesystem;

[[nodiscard]] Result<std::vector<u8>> ReadWasmBytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.good()) {
        return Err(ErrorCode::FileReadError, "Failed to open wasm module: " + path.string());
    }

    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size <= 0) {
        return Err(ErrorCode::ParseInvalidFormat, "Wasm module is empty: " + path.string());
    }
    if (static_cast<std::uintmax_t>(size) > limits::kMaxWasmBytes)
        return Err(ErrorCode::ValidationOutOfRange, "Wasm module exceeds the 32 MiB host cap: " + path.string());
    input.seekg(0, std::ios::beg);
    std::vector<u8> bytes(static_cast<std::size_t>(size));
    const auto byte_count = static_cast<std::streamsize>(size);
    input.read(reinterpret_cast<char*>(bytes.data()), byte_count);
    if (input.gcount() != byte_count)
        return Err(ErrorCode::FileReadError, "Failed to read complete wasm module: " + path.string());
    return Ok(bytes);
}

void MarkExport(GuestModuleInfo& info, std::string_view name) {
    if (name == "ext_api_version") {
        info.ext_api_version = true;
    } else if (name == "ext_init") {
        info.ext_init = true;
    } else if (name == "ext_on_tick") {
        info.ext_on_tick = true;
    } else if (name == "ext_on_event") {
        info.ext_on_event = true;
    } else if (name == "ext_on_event_named") {
        info.ext_on_event_named = true;
    } else if (name == "ext_on_unload") {
        info.ext_on_unload = true;
    } else if (name == "ext_on_command") {
        info.ext_on_command = true;
    } else if (name == "ext_alloc") {
        info.ext_alloc = true;
    } else if (name == "ext_free") {
        info.ext_free = true;
    }
}

class Reader {
public:
    explicit Reader(std::span<const u8> bytes)
        : bytes_(bytes) {}

    [[nodiscard]] bool Empty() const noexcept {
        return offset_ == bytes_.size();
    }

    [[nodiscard]] std::size_t Remaining() const noexcept {
        return bytes_.size() - offset_;
    }

    [[nodiscard]] Result<u8> Byte() {
        if (Empty())
            return Err(ErrorCode::ParseInvalidFormat, "Unexpected end of wasm module.");
        return Ok(bytes_[offset_++]);
    }

    [[nodiscard]] Result<u32> U32() {
        u32 value = 0;
        for (u32 shift = 0; shift < 35; shift += 7) {
            auto byte = Byte();
            if (!byte)
                return Err(byte.error());
            if (shift == 28 && ((*byte & 0x70u) != 0))
                return Err(ErrorCode::ParseInvalidFormat, "Invalid u32 LEB128 in wasm module.");
            value |= static_cast<u32>(*byte & 0x7fu) << shift;
            if ((*byte & 0x80u) == 0)
                return Ok(value);
        }
        return Err(ErrorCode::ParseInvalidFormat, "Invalid u32 LEB128 in wasm module.");
    }

    [[nodiscard]] Result<std::string> Name() {
        auto size = U32();
        if (!size)
            return Err(size.error());
        if (*size > Remaining())
            return Err(ErrorCode::ParseInvalidFormat, "Wasm name extends beyond its section.");
        std::string result(reinterpret_cast<const char*>(bytes_.data() + offset_), *size);
        offset_ += *size;
        return Ok(std::move(result));
    }

    [[nodiscard]] Result<Reader> Subsection(u32 size) {
        if (size > Remaining())
            return Err(ErrorCode::ParseInvalidFormat, "Wasm section extends beyond the module.");
        Reader result(bytes_.subspan(offset_, size));
        offset_ += size;
        return Ok(result);
    }

    [[nodiscard]] Result<void> Skip(std::size_t size) {
        if (size > Remaining())
            return Err(ErrorCode::ParseInvalidFormat, "Unexpected end of wasm section.");
        offset_ += size;
        return Ok();
    }

private:
    std::span<const u8> bytes_;
    std::size_t offset_{0};
};

[[nodiscard]] Result<WasmValueType> ValueType(Reader& reader) {
    auto value = reader.Byte();
    if (!value)
        return Err(value.error());
    switch (*value) {
        case 0x7f:
            return Ok(WasmValueType::I32);
        case 0x7e:
            return Ok(WasmValueType::I64);
        case 0x7d:
            return Ok(WasmValueType::F32);
        case 0x7c:
            return Ok(WasmValueType::F64);
        default:
            return Err(ErrorCode::ParseInvalidFormat, "Unsupported value type in wasm ABI signature.");
    }
}

struct WasmLimits {
    u32 minimum;
    std::optional<u32> maximum;
};

[[nodiscard]] Result<WasmLimits> Limits(Reader& reader) {
    auto flags = reader.Byte();
    if (!flags)
        return Err(flags.error());
    if (*flags > 1)
        return Err(ErrorCode::ParseInvalidFormat, "Unsupported wasm limits encoding.");
    auto minimum = reader.U32();
    if (!minimum)
        return Err(minimum.error());
    std::optional<u32> result_maximum;
    if (*flags == 1) {
        auto maximum = reader.U32();
        if (!maximum)
            return Err(maximum.error());
        if (*maximum < *minimum)
            return Err(ErrorCode::ParseInvalidFormat, "Wasm memory maximum is smaller than its minimum.");
        result_maximum = *maximum;
    }
    return Ok(WasmLimits{*minimum, result_maximum});
}

struct ExportRecord {
    std::string name;
    u8 kind;
    u32 index;
};

[[nodiscard]] Result<GuestModuleInfo> ParseModule(std::span<const u8> bytes) {
    if (bytes.size() < 8 || !std::ranges::equal(bytes.first<4>(), std::array<u8, 4>{0x00, 0x61, 0x73, 0x6d}) || !std::ranges::equal(bytes.subspan<4, 4>(), std::array<u8, 4>{0x01, 0x00, 0x00, 0x00})) {
        return Err(ErrorCode::ParseInvalidFormat, "Invalid wasm magic or version header.");
    }

    GuestModuleInfo info;
    info.valid_magic = true;
    Reader module(bytes.subspan(8));
    std::vector<GuestFunctionSignature> types;
    std::vector<u32> function_types;
    std::vector<ExportRecord> exports;
    u32 memory_count = 0;
    std::unordered_set<u8> sections;
    u8 last_section_rank = 0;

    while (!module.Empty()) {
        auto id = module.Byte();
        auto size = module.U32();
        if (!id || !size)
            return Err(!id ? id.error() : size.error());
        auto section_result = module.Subsection(*size);
        if (!section_result)
            return Err(section_result.error());
        Reader section = *section_result;
        if (*id != 0) {
            const u8 rank = *id == 12 ? 10 : (*id >= 10 ? static_cast<u8>(*id + 1) : *id);
            if (*id > 12 || sections.contains(*id) || rank < last_section_rank)
                return Err(ErrorCode::ParseInvalidFormat, "Invalid or out-of-order wasm section.");
            sections.insert(*id);
            last_section_rank = rank;
        }

        if (*id == 1) {
            auto count = section.U32();
            if (!count)
                return Err(count.error());
            for (u32 i = 0; i < *count; ++i) {
                auto form = section.Byte();
                if (!form || *form != 0x60)
                    return Err(ErrorCode::ParseInvalidFormat, "Invalid wasm function type.");
                GuestFunctionSignature signature;
                auto params = section.U32();
                if (!params)
                    return Err(params.error());
                for (u32 p = 0; p < *params; ++p) {
                    auto type = ValueType(section);
                    if (!type)
                        return Err(type.error());
                    signature.parameters.push_back(*type);
                }
                auto results = section.U32();
                if (!results)
                    return Err(results.error());
                for (u32 r = 0; r < *results; ++r) {
                    auto type = ValueType(section);
                    if (!type)
                        return Err(type.error());
                    signature.results.push_back(*type);
                }
                types.push_back(std::move(signature));
            }
        } else if (*id == 2) {
            auto count = section.U32();
            if (!count)
                return Err(count.error());
            for (u32 i = 0; i < *count; ++i) {
                auto module_name = section.Name();
                auto name = section.Name();
                auto kind = section.Byte();
                if (!module_name || !name || !kind)
                    return Err(ErrorCode::ParseInvalidFormat, "Malformed wasm import.");
                if (*kind != 0)
                    return Err(ErrorCode::ValidationInvalidState, "Extension wasm may only import host functions.");
                auto type_index = section.U32();
                if (!type_index)
                    return Err(type_index.error());
                if (*type_index >= types.size())
                    return Err(ErrorCode::ParseInvalidFormat, "Wasm import references an invalid function type.");
                function_types.push_back(*type_index);
                info.imports.push_back({std::move(*module_name), std::move(*name), types[*type_index]});
            }
        } else if (*id == 3) {
            auto count = section.U32();
            if (!count)
                return Err(count.error());
            for (u32 i = 0; i < *count; ++i) {
                auto type_index = section.U32();
                if (!type_index)
                    return Err(type_index.error());
                if (*type_index >= types.size())
                    return Err(ErrorCode::ParseInvalidFormat, "Wasm function references an invalid type.");
                function_types.push_back(*type_index);
            }
        } else if (*id == 5) {
            auto count = section.U32();
            if (!count)
                return Err(count.error());
            if (*count != 1)
                return Err(ErrorCode::ValidationInvalidState, "Extension wasm must define exactly one linear memory.");
            memory_count = *count;
            for (u32 i = 0; i < *count; ++i) {
                auto memory_limits = Limits(section);
                if (!memory_limits)
                    return Err(memory_limits.error());
                if (i == 0) {
                    info.memory_minimum_pages = memory_limits->minimum;
                    info.memory_maximum_pages = memory_limits->maximum;
                }
            }
        } else if (*id == 7) {
            auto count = section.U32();
            if (!count)
                return Err(count.error());
            std::unordered_set<std::string> names;
            for (u32 i = 0; i < *count; ++i) {
                auto name = section.Name();
                auto kind = section.Byte();
                auto index = section.U32();
                if (!name || !kind || !index)
                    return Err(ErrorCode::ParseInvalidFormat, "Malformed wasm export.");
                if (!names.insert(*name).second)
                    return Err(ErrorCode::ParseInvalidFormat, "Duplicate wasm export name '" + *name + "'.");
                exports.push_back({std::move(*name), *kind, *index});
            }
        } else if (*id == 8) {
            auto index = section.U32();
            if (!index)
                return Err(index.error());
            info.has_start = true;
        } else {
            auto skipped = section.Skip(section.Remaining());
            if (!skipped)
                return Err(skipped.error());
        }
        if (!section.Empty())
            return Err(ErrorCode::ParseInvalidFormat, "Trailing data in wasm section.");
    }

    const std::unordered_map<std::string_view, GuestFunctionSignature> expected{
        {"ext_api_version", {{}, {WasmValueType::I32}}},
        {"ext_init", {{}, {WasmValueType::I32}}},
        {"ext_on_tick", {{WasmValueType::F64}, {}}},
        {"ext_on_event", {{WasmValueType::I32, WasmValueType::I32, WasmValueType::I32}, {}}},
        {"ext_on_event_named", {{WasmValueType::I32, WasmValueType::I32, WasmValueType::I32, WasmValueType::I32}, {}}},
        {"ext_on_unload", {{}, {}}},
        {"ext_on_command", {{WasmValueType::I32, WasmValueType::I32, WasmValueType::I32, WasmValueType::I32}, {WasmValueType::I32}}},
        {"ext_alloc", {{WasmValueType::I32}, {WasmValueType::I32}}},
        {"ext_free", {{WasmValueType::I32, WasmValueType::I32}, {}}},
    };
    for (const ExportRecord& exported : exports) {
        if (exported.name == "memory") {
            if (exported.kind != 2 || exported.index >= memory_count)
                return Err(ErrorCode::ValidationInvalidState, "Extension export 'memory' is not linear memory.");
            info.memory = true;
            continue;
        }
        const auto contract = expected.find(exported.name);
        if (contract == expected.end()) {
            if (exported.name.starts_with("ext_"))
                return Err(ErrorCode::ValidationInvalidState, "Unknown extension ABI export '" + exported.name + "'.");
            continue;
        }
        if (exported.kind != 0 || exported.index >= function_types.size() || types[function_types[exported.index]] != contract->second)
            return Err(ErrorCode::ValidationInvalidState, "Extension export signature mismatch for '" + exported.name + "'.");
        MarkExport(info, exported.name);
    }
    return Ok(std::move(info));
}

[[nodiscard]] GuestFunctionSignature Signature(std::initializer_list<WasmValueType> parameters, std::initializer_list<WasmValueType> results = {WasmValueType::I32}) {
    return {parameters, results};
}

struct HostContract {
    GuestFunctionSignature signature;
    Permission permission;
};

const std::unordered_map<std::string_view, HostContract> kHostImports{
    {"host_log", {Signature({WasmValueType::I32, WasmValueType::I32, WasmValueType::I32}), Permission::Log}},
    {"host_path_data", {Signature({WasmValueType::I32, WasmValueType::I32}), Permission::Paths}},
    {"host_path_cache", {Signature({WasmValueType::I32, WasmValueType::I32}), Permission::Paths}},
    {"host_file_read", {Signature({WasmValueType::I32, WasmValueType::I32, WasmValueType::I32}), Permission::Storage}},
    {"host_file_write", {Signature({WasmValueType::I32, WasmValueType::I32, WasmValueType::I32}), Permission::Storage}},
    {"host_file_append", {Signature({WasmValueType::I32, WasmValueType::I32, WasmValueType::I32}), Permission::Storage}},
    {"host_file_read_n", {Signature({WasmValueType::I32, WasmValueType::I32, WasmValueType::I32, WasmValueType::I32}), Permission::Storage}},
    {"host_file_write_n", {Signature({WasmValueType::I32, WasmValueType::I32, WasmValueType::I32, WasmValueType::I32}), Permission::Storage}},
    {"host_file_append_n", {Signature({WasmValueType::I32, WasmValueType::I32, WasmValueType::I32, WasmValueType::I32}), Permission::Storage}},
    {"host_config_get", {Signature({WasmValueType::I32, WasmValueType::I32, WasmValueType::I32}), Permission::Config}},
    {"host_config_set", {Signature({WasmValueType::I32, WasmValueType::I32, WasmValueType::I32}), Permission::Config}},
    {"host_event_subscribe", {Signature({WasmValueType::I32}), Permission::Events}},
    {"host_event_emit", {Signature({WasmValueType::I32, WasmValueType::I32, WasmValueType::I32}), Permission::Events}},
    {"host_event_subscribe_named", {Signature({WasmValueType::I32, WasmValueType::I32}), Permission::Events}},
    {"host_event_emit_named", {Signature({WasmValueType::I32, WasmValueType::I32, WasmValueType::I32, WasmValueType::I32}), Permission::Events}},
};

} // namespace

Result<void> ValidateWasmMagic(const fs::path& wasm_path) {
    std::ifstream input(wasm_path, std::ios::binary);
    if (!input.good()) {
        return Err(ErrorCode::FileReadError, "Failed to open wasm module: " + wasm_path.string());
    }

    std::array<unsigned char, 4> magic{};
    input.read(reinterpret_cast<char*>(magic.data()), static_cast<std::streamsize>(magic.size()));
    if (input.gcount() != static_cast<std::streamsize>(magic.size())) {
        return Err(ErrorCode::ParseInvalidFormat, "Wasm module is too small: " + wasm_path.string());
    }

    constexpr std::array<unsigned char, 4> kWasmMagic{0x00, 0x61, 0x73, 0x6d};
    if (magic != kWasmMagic) {
        return Err(ErrorCode::ParseInvalidFormat, "Wasm module has invalid magic header. Expected '\\0asm': " + wasm_path.string());
    }

    return Ok();
}

Result<std::vector<u8>> LoadGuestModule(const fs::path& wasm_path) {
    return ReadWasmBytes(wasm_path);
}

Result<GuestModuleInfo> InspectGuestModule(std::span<const u8> bytes) {
    if (bytes.empty())
        return Err(ErrorCode::ParseInvalidFormat, "Wasm module is empty.");
    if (bytes.size() > limits::kMaxWasmBytes)
        return Err(ErrorCode::ValidationOutOfRange, "Wasm module exceeds the 32 MiB host cap.");
    return ParseModule(bytes);
}

Result<GuestModuleInfo> InspectGuestModule(const fs::path& wasm_path) {
    auto bytes = ReadWasmBytes(wasm_path);
    if (!bytes)
        return Err(bytes.error());
    return InspectGuestModule(*bytes);
}

Result<void> ValidateGuestModule(std::span<const u8> bytes, const Manifest& manifest) {
    auto info = InspectGuestModule(bytes);
    if (!info) {
        return Err(info.error());
    }

    if (!info->memory)
        return Err(ErrorCode::ValidationInvalidState, "Wasm module is missing exported linear memory 'memory'.");
    if (info->has_start)
        return Err(ErrorCode::ValidationInvalidState, "Extension wasm modules must not define a start section.");
    if (!info->memory_maximum_pages)
        return Err(ErrorCode::ValidationInvalidState, "Extension linear memory must declare a maximum.");
    if (info->memory_minimum_pages > limits::kMaxMemoryPages || *info->memory_maximum_pages > limits::kMaxMemoryPages)
        return Err(ErrorCode::ValidationOutOfRange, "Extension linear memory exceeds the 32 MiB host cap.");
    if (!info->ext_api_version) {
        return Err(ErrorCode::ValidationInvalidState, "Wasm module is missing export ext_api_version.");
    }
    if (!info->ext_init) {
        return Err(ErrorCode::ValidationInvalidState, "Wasm module is missing export ext_init.");
    }
    if (!info->ext_on_tick) {
        return Err(ErrorCode::ValidationInvalidState, "Wasm module is missing export ext_on_tick.");
    }
    if (!info->ext_on_event) {
        return Err(ErrorCode::ValidationInvalidState, "Wasm module is missing export ext_on_event.");
    }
    if (!info->ext_on_unload) {
        return Err(ErrorCode::ValidationInvalidState, "Wasm module is missing export ext_on_unload.");
    }
    if (!manifest.commands.empty() && !info->ext_on_command) {
        return Err(ErrorCode::ValidationInvalidState, "Manifest contributes commands but wasm does not export ext_on_command.");
    }
    if (info->ext_alloc != info->ext_free)
        return Err(ErrorCode::ValidationInvalidState, "Wasm module must export ext_alloc and ext_free as a pair.");
    if ((!manifest.commands.empty() || HasPermission(manifest, Permission::Events)) && !info->ext_alloc)
        return Err(ErrorCode::ValidationInvalidState, "Event and command manifests must export the ext_alloc/ext_free pair.");
    for (const GuestImport& imported : info->imports) {
        if (imported.module != "woki_host")
            return Err(ErrorCode::ValidationInvalidState, "Unknown wasm import module '" + imported.module + "' for '" + imported.name + "'.");
        const auto contract = kHostImports.find(imported.name);
        if (contract == kHostImports.end())
            return Err(ErrorCode::ValidationInvalidState, "Unknown woki_host import '" + imported.name + "'.");
        if (imported.signature != contract->second.signature)
            return Err(ErrorCode::ValidationInvalidState, "Host import signature mismatch for 'woki_host::" + imported.name + "'.");
        if (!HasPermission(manifest, contract->second.permission))
            return Err(ErrorCode::ValidationInvalidState, "Host import 'woki_host::" + imported.name + "' requires its manifest permission.");
    }

#if defined(WOKI_EXTENSION_WITH_WASMTIME)
    wasmtime::Config config;
    config.consume_fuel(true);
    config.max_wasm_stack(1u * 1024u * 1024u);
    wasmtime::Engine engine(std::move(config));
    auto module = wasmtime::Module::compile(engine, wasmtime::Span<uint8_t>{const_cast<u8*>(bytes.data()), bytes.size()});
    if (!module)
        return Err(ErrorCode::ParseInvalidFormat, "Failed to compile wasm module: " + module.err_ref().message());

    // Import-free modules can also expose their exact version under bounded
    // execution. Imported modules are still standards-validated above.
    if (info->imports.empty()) {
        wasmtime::Store store(engine);
        store.limiter(32 * 1024 * 1024, 10'000, 1, 1, 1);
        auto fueled = store.context().set_fuel(1'000'000);
        if (!fueled)
            return Err(ErrorCode::InvalidState, "Failed to set fuel for apiVersion verification: " + fueled.err_ref().message());
        wasmtime::Linker linker(engine);
        auto instance = linker.instantiate(store, module.ok_ref());
        if (!instance)
            return Err(ErrorCode::ValidationInvalidState, "Failed to instantiate wasm for apiVersion verification: " + instance.err_ref().message());
        auto exported = instance.ok_ref().get(store, "ext_api_version");
        const auto* function = exported ? std::get_if<wasmtime::Func>(&*exported) : nullptr;
        if (function == nullptr)
            return Err(ErrorCode::ValidationInvalidState, "Wasm export 'ext_api_version' is not callable.");
        auto typed = function->typed<std::monostate, u32>(store);
        if (!typed)
            return Err(ErrorCode::ValidationInvalidState, "Wasm export signature mismatch for 'ext_api_version'.");
        auto version = typed.ok_ref().call(store, std::monostate{});
        if (!version)
            return Err(ErrorCode::ValidationInvalidState, "ext_api_version trapped during verification: " + version.err_ref().message());
        if (version.ok_ref() != manifest.api_version)
            return Err(ErrorCode::ValidationInvalidState, "Extension apiVersion mismatch. Manifest declares " + std::to_string(manifest.api_version) + ", wasm exports " + std::to_string(version.ok_ref()) + ".");
    }
#endif

    return Ok();
}

Result<void> ValidateGuestModule(const fs::path& wasm_path, const Manifest& manifest) {
    auto bytes = LoadGuestModule(wasm_path);
    if (!bytes)
        return Err(bytes.error());
    return ValidateGuestModule(*bytes, manifest);
}

} // namespace woki::ext::wasm
