#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <cctype>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <ranges>
#include <stdexcept>

#include <woki/config.hpp>

namespace {
using Json = woki::config::Json;

struct Type {
    std::string kind;
    std::string scalar;
    std::string reference;
    unsigned columns{};
    unsigned rows{};
    unsigned count{};
    unsigned explicit_size{};
    std::shared_ptr<Type> element;
};

struct Field {
    std::string name;
    Type type;
};

struct Record {
    std::string name;
    std::string address_space;
    std::string layout_class;
    std::vector<Field> fields;
    unsigned alignment{};
    unsigned size{};
};

struct Layout {
    unsigned alignment{};
    unsigned size{};
    unsigned stride{};
};

unsigned Align(const unsigned value, const unsigned alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

bool Identifier(const std::string_view value) {
    return !value.empty() && (std::isalpha(static_cast<unsigned char>(value.front())) || value.front() == '_')
           && std::ranges::all_of(value.substr(1), [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; });
}

bool Keys(const Json& value, std::initializer_list<std::string_view> allowed) {
    return value.is_object() && std::ranges::all_of(value.items(), [&](const auto& item) { return std::ranges::find(allowed, item.first) != allowed.end(); });
}

Type ParseType(const Json& value) {
    if (value.is_string()) {
        const auto text = value.get<std::string>();
        if (text == "f32" || text == "i32" || text == "u32") {
            Type result;
            result.kind = "scalar";
            result.scalar = text;
            return result;
        }
        if (text == "bool")
            throw std::runtime_error("bool is not host-shareable; encode it as u32");
        throw std::runtime_error("unknown scalar type '" + text + "'");
    }
    if (!Keys(value, {"type", "element", "width", "columns", "rows", "count", "name", "size"}) || !value.contains("type") || !value["type"].is_string())
        throw std::runtime_error("field type must be a strict type object or scalar string");
    Type result;
    result.kind = value["type"].get<std::string>();
    if (result.kind == "vector") {
        if (!value.contains("element") || !value["element"].is_string() || !value.contains("width") || !value["width"].is_number_unsigned())
            throw std::runtime_error("vector requires element and width");
        result.scalar = value["element"].get<std::string>();
        result.rows = value["width"].get<unsigned>();
        if ((result.scalar != "f32" && result.scalar != "i32" && result.scalar != "u32") || result.rows < 2 || result.rows > 4)
            throw std::runtime_error("invalid vector type");
    } else if (result.kind == "matrix") {
        result.scalar = value.value("element", "f32");
        if (!value.contains("columns") || !value.contains("rows") || !value["columns"].is_number_unsigned() || !value["rows"].is_number_unsigned())
            throw std::runtime_error("matrix requires columns and rows");
        result.columns = value["columns"].get<unsigned>();
        result.rows = value["rows"].get<unsigned>();
        if (result.scalar != "f32" || result.columns < 2 || result.columns > 4 || result.rows < 2 || result.rows > 4)
            throw std::runtime_error("host matrices currently require f32 elements");
    } else if (result.kind == "array") {
        if (!value.contains("element") || !value.contains("count") || !value["count"].is_number_unsigned() || value["count"].get<unsigned>() == 0)
            throw std::runtime_error("array requires a positive count and element");
        result.element = std::make_shared<Type>(ParseType(value["element"]));
        result.count = value["count"].get<unsigned>();
    } else if (result.kind == "ref") {
        if (!value.contains("name") || !value["name"].is_string() || !Identifier(value["name"].get_ref<const std::string&>()))
            throw std::runtime_error("ref requires a valid name");
        result.reference = value["name"].get<std::string>();
    } else if (result.kind == "padding") {
        if (!value.contains("size") || !value["size"].is_number_unsigned() || value["size"].get<unsigned>() == 0 || value["size"].get<unsigned>() % 4 != 0)
            throw std::runtime_error("padding requires a positive multiple-of-four byte size");
        result.explicit_size = value["size"].get<unsigned>();
    } else
        throw std::runtime_error("unknown compound type '" + result.kind + "'");
    return result;
}

Layout TypeLayout(const Type& type, const bool uniform, const std::map<std::string, Record>& records) {
    if (type.kind == "scalar")
        return {4, 4, 4};
    if (type.kind == "padding")
        return {4, type.explicit_size, type.explicit_size};
    if (type.kind == "vector") {
        const unsigned align = type.rows == 2 ? 8 : 16;
        return {align, type.rows * 4, Align(type.rows * 4, align)};
    }
    if (type.kind == "matrix") {
        Type column;
        column.kind = "vector";
        column.scalar = type.scalar;
        column.rows = type.rows;
        const auto layout = TypeLayout(column, uniform, records);
        const unsigned stride = Align(layout.size, layout.alignment);
        return {layout.alignment, type.columns * stride, stride};
    }
    if (type.kind == "ref") {
        const auto found = records.find(type.reference);
        if (found == records.end() || found->second.size == 0)
            throw std::runtime_error("unresolved or forward reference '" + type.reference + "'");
        return {uniform ? std::max(16U, found->second.alignment) : found->second.alignment, found->second.size, found->second.size};
    }
    if (type.element->kind == "padding")
        throw std::runtime_error("padding is only valid as a direct record field");
    const auto element = TypeLayout(*type.element, uniform, records);
    const unsigned alignment = uniform ? std::max(16U, element.alignment) : element.alignment;
    const unsigned stride = Align(element.size, alignment);
    if (stride != element.size)
        throw std::runtime_error("array element requires explicit packed padding or a padded nested record");
    return {alignment, stride * type.count, stride};
}

std::string CppType(const Type& type) {
    if (type.kind == "scalar")
        return type.scalar == "f32" ? "float" : type.scalar == "i32" ? "std::int32_t" : "std::uint32_t";
    if (type.kind == "vector")
        return "std::array<" + (type.scalar == "f32" ? std::string("float") : type.scalar == "i32" ? "std::int32_t" : "std::uint32_t") + ", " + std::to_string(type.rows) + ">";
    if (type.kind == "matrix")
        return "std::array<float, " + std::to_string(type.columns * Align(type.rows * 4, type.rows == 2 ? 8 : 16) / 4) + ">";
    if (type.kind == "array")
        return "std::array<" + CppType(*type.element) + ", " + std::to_string(type.count) + ">";
    if (type.kind == "ref")
        return type.reference;
    return "std::array<std::byte, " + std::to_string(type.explicit_size) + ">";
}

std::string WgslType(const Type& type) {
    if (type.kind == "scalar")
        return type.scalar;
    if (type.kind == "vector")
        return "vec" + std::to_string(type.rows) + "<" + type.scalar + ">";
    if (type.kind == "matrix")
        return "mat" + std::to_string(type.columns) + "x" + std::to_string(type.rows) + "<" + type.scalar + ">";
    if (type.kind == "array")
        return "array<" + WgslType(*type.element) + ", " + std::to_string(type.count) + ">";
    if (type.kind == "ref")
        return type.reference;
    return "array<u32, " + std::to_string((type.explicit_size + 3) / 4) + ">";
}

void WriteFile(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
    if (!output)
        throw std::runtime_error("cannot write '" + path.string() + "'");
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 5 || std::string_view(argv[1]) != "generate") {
            std::cerr << "usage: woki-gpu-struct generate <header> <wgsl> <schema>...\n";
            return 2;
        }
        std::map<std::string, Record> records;
        std::vector<std::string> order;
        for (int index = 4; index < argc; ++index) {
            std::ifstream input(argv[index], std::ios::binary);
            if (!input)
                throw std::runtime_error("cannot open schema");
            std::string source((std::istreambuf_iterator<char>(input)), {});
            auto parsed = Json::Parse(source, argv[index]);
            if (!parsed)
                throw std::runtime_error(woki::config::FormatDiagnostics(parsed.error()));
            const Json root = std::move(*parsed);
            if (!Keys(root, {"$schema", "schema", "name", "address_space", "layout_class", "fields"}) || root.value("schema", 0) != 1 || !root.contains("name") || !root["name"].is_string()
                || !root.contains("address_space") || !root["address_space"].is_string() || !root.contains("layout_class") || !root["layout_class"].is_string() || !root.contains("fields") || !root["fields"].is_array()
                || root["fields"].empty())
                throw std::runtime_error("schema must be a strict .woki-gpu-struct v1 object");
            Record record;
            record.name = root["name"].get<std::string>();
            record.address_space = root["address_space"].get<std::string>();
            record.layout_class = root["layout_class"].get<std::string>();
            if (!Identifier(record.name) || (record.address_space != "uniform" && record.address_space != "storage") || record.layout_class != "wgsl-host-shareable")
                throw std::runtime_error("invalid schema identity or layout class");
            std::set<std::string> names;
            for (const auto& field : root["fields"]) {
                if (!Keys(field, {"name", "type"}) || !field.contains("name") || !field["name"].is_string() || !field.contains("type"))
                    throw std::runtime_error("field requires only name and type");
                const auto name = field["name"].get<std::string>();
                if (!Identifier(name) || !names.insert(name).second)
                    throw std::runtime_error("field names must be unique identifiers");
                record.fields.push_back({name, ParseType(field["type"])});
            }
            if (!records.emplace(record.name, std::move(record)).second)
                throw std::runtime_error("duplicate record name");
            order.push_back(root["name"].get<std::string>());
        }
        for (const auto& name : order) {
            auto& record = records.at(name);
            unsigned cursor = 0, alignment = 1;
            const bool uniform = record.address_space == "uniform";
            for (const auto& field : record.fields) {
                const auto layout = TypeLayout(field.type, uniform, records);
                cursor = Align(cursor, layout.alignment);
                cursor += layout.size;
                alignment = std::max(alignment, layout.alignment);
            }
            record.alignment = uniform ? std::max(16U, alignment) : alignment;
            record.size = Align(cursor, record.alignment);
        }
        std::ostringstream cpp, wgsl;
        cpp << "#pragma once\n#include <array>\n#include <cstddef>\n#include <cstdint>\n\nnamespace woki::gfx::abi {\n";
        wgsl << "// Generated by woki-gpu-struct. Do not edit.\n\n";
        for (const auto& name : order) {
            const auto& record = records.at(name);
            unsigned cursor = 0, padding = 0;
            const bool uniform = record.address_space == "uniform";
            cpp << "struct alignas(" << record.alignment << ") " << name << " final {\n";
            wgsl << "struct " << name << " {\n";
            for (const auto& field : record.fields) {
                const auto layout = TypeLayout(field.type, uniform, records);
                const unsigned offset = Align(cursor, layout.alignment);
                if (offset != cursor)
                    cpp << "    std::array<std::byte, " << offset - cursor << "> _padding" << padding++ << "{};\n";
                cpp << "    " << CppType(field.type) << " " << field.name << "{};\n";
                if (field.type.kind == "padding")
                    wgsl << "    @size(" << field.type.explicit_size << ") " << field.name << ": u32,\n";
                else
                    wgsl << "    " << field.name << ": " << WgslType(field.type) << ",\n";
                cursor = offset + layout.size;
            }
            if (cursor != record.size)
                cpp << "    std::array<std::byte, " << record.size - cursor << "> _padding" << padding++ << "{};\n";
            cpp << "};\ninline constexpr std::size_t k" << name << "Size = " << record.size << ";\n";
            cursor = 0;
            for (const auto& field : record.fields) {
                const auto layout = TypeLayout(field.type, uniform, records);
                cursor = Align(cursor, layout.alignment);
                cpp << "inline constexpr std::size_t k" << name << "_" << field.name << "Offset = " << cursor << ";\n";
                cursor += layout.size;
            }
            cpp << "static_assert(sizeof(" << name << ") == k" << name << "Size);\nstatic_assert(alignof(" << name << ") == " << record.alignment << ");\n";
            for (const auto& field : record.fields)
                cpp << "static_assert(offsetof(" << name << ", " << field.name << ") == k" << name << "_" << field.name << "Offset);\n";
            cpp << "\n";
            wgsl << "};\n\n";
        }
        cpp << "} // namespace woki::gfx::abi\n";
        WriteFile(argv[2], cpp.str());
        WriteFile(argv[3], wgsl.str());
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "woki-gpu-struct: " << error.what() << '\n';
        return 1;
    }
}
