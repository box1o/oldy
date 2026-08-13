#include <charconv>
#include <cmath>
#include <fstream>
#include <map>

#include <yaml-cpp/yaml.h>

#include <woki/config.hpp>

namespace woki::config {

class YamlParser {
public:
    YamlParser(std::string_view text, std::string source, Limits limits)
        : text_(text),
          source_(std::move(source)),
          limits_(limits) {}

    std::expected<Document, std::vector<Diagnostic>> Run() {
        if (text_.size() > limits_.bytes)
            return Error("CFG5001", {}, "YAML byte limit exceeded");
        if (ForbiddenSyntax())
            return Error("CFG5002", {}, "YAML aliases, anchors, tags, merge keys, and complex keys are not supported");
        try {
            YAML::Node yaml = YAML::Load(std::string(text_));
            auto root = Convert(yaml, {}, 0);
            if (!diagnostics_.empty())
                return std::unexpected(std::move(diagnostics_));
            Document document;
            document.source_ = std::move(source_);
            document.text_ = std::string(text_);
            document.root_ = std::move(root);
            return document;
        } catch (const YAML::Exception& error) {
            return Error("CFG5003", error.mark, error.what());
        }
    }

private:
    std::expected<Document, std::vector<Diagnostic>> Error(std::string code, YAML::Mark mark, std::string message) {
        SourcePosition position{mark.is_null() ? 0U : static_cast<u64>(mark.pos), mark.is_null() ? 1U : static_cast<u32>(mark.line + 1), mark.is_null() ? 1U : static_cast<u32>(mark.column + 1)};
        return std::unexpected(std::vector{Diagnostic{std::move(code), Severity::Error, source_, {position, position}, {}, std::move(message), {}}});
    }

    bool ForbiddenSyntax() const {
        bool quote = false;
        for (size_t i = 0; i < text_.size(); ++i) {
            char c = text_[i];
            if (c == '"' && (i == 0 || text_[i - 1] != '\\'))
                quote = !quote;
            if (!quote && (c == '&' || c == '*' || c == '!' || (c == '<' && i + 1 < text_.size() && text_[i + 1] == '<')))
                return true;
        }
        return false;
    }

    SourceRange Range(const YAML::Node& node) const {
        const auto mark = node.Mark();
        SourcePosition begin{mark.is_null() ? 0U : static_cast<u64>(mark.pos), mark.is_null() ? 1U : static_cast<u32>(mark.line + 1), mark.is_null() ? 1U : static_cast<u32>(mark.column + 1)};
        return {begin, begin};
    }

    std::shared_ptr<const Value> Convert(const YAML::Node& node, std::string pointer, u32 depth) {
        if (depth > limits_.depth || ++nodes_ > limits_.nodes) {
            diagnostics_.push_back({"CFG5004", Severity::Error, source_, Range(node), pointer, "YAML structural limit exceeded", {}});
            return {};
        }
        auto value = std::make_shared<Value>();
        value->pointer_ = std::move(pointer);
        value->range_ = Range(node);
        if (!node || node.IsNull())
            return value;
        if (node.IsSequence()) {
            value->kind_ = Value::Kind::Array;
            Value::Array array;
            for (size_t i = 0; i < node.size(); ++i) {
                auto child = Convert(node[i], value->pointer_ + '/' + std::to_string(i), depth + 1);
                if (child)
                    array.push_back(std::move(child));
            }
            value->data_ = std::move(array);
            return value;
        }
        if (node.IsMap()) {
            value->kind_ = Value::Kind::Object;
            Value::Object object;
            std::map<std::string, SourceRange, std::less<>> keys;
            for (const auto& item : node) {
                if (!item.first.IsScalar()) {
                    diagnostics_.push_back({"CFG5005", Severity::Error, source_, Range(item.first), value->pointer_, "YAML map keys must be strings", {}});
                    continue;
                }
                if (++members_ > limits_.members) {
                    diagnostics_.push_back({"CFG5004", Severity::Error, source_, Range(item.first), value->pointer_, "YAML member limit exceeded", {}});
                    break;
                }
                std::string key = item.first.Scalar();
                auto [found, inserted] = keys.emplace(key, Range(item.first));
                if (!inserted) {
                    Diagnostic diagnostic{"CFG5006", Severity::Error, source_, Range(item.first), value->pointer_, "duplicate YAML key: " + key, {}};
                    diagnostic.notes.push_back({"first key is here", found->second});
                    diagnostics_.push_back(std::move(diagnostic));
                    continue;
                }
                auto child = Convert(item.second, value->pointer_ + '/' + key, depth + 1);
                if (child)
                    object.push_back({std::move(key), Range(item.first), std::move(child)});
            }
            value->data_ = std::move(object);
            return value;
        }
        const std::string scalar = node.Scalar();
        if (scalar.size() > limits_.string_bytes)
            diagnostics_.push_back({"CFG5004", Severity::Error, source_, value->range_, value->pointer_, "YAML string limit exceeded", {}});
        std::string tag = node.Tag();
        if (tag == "?") {
            if (scalar == "null" || scalar == "Null" || scalar == "NULL" || scalar == "~")
                tag = "tag:yaml.org,2002:null";
            else if (scalar == "true" || scalar == "True" || scalar == "TRUE" || scalar == "false" || scalar == "False" || scalar == "FALSE")
                tag = "tag:yaml.org,2002:bool";
            else {
                i64 integer{};
                const auto parsed_integer = std::from_chars(scalar.data(), scalar.data() + scalar.size(), integer);
                if (parsed_integer.ec == std::errc{} && parsed_integer.ptr == scalar.data() + scalar.size())
                    tag = "tag:yaml.org,2002:int";
                else if (scalar.find_first_of(".eE") != std::string::npos) {
                    f64 real{};
                    const auto parsed_real = std::from_chars(scalar.data(), scalar.data() + scalar.size(), real);
                    if (parsed_real.ec == std::errc{} && parsed_real.ptr == scalar.data() + scalar.size())
                        tag = "tag:yaml.org,2002:float";
                }
            }
        }
        if (tag == "tag:yaml.org,2002:null")
            return value;
        if (tag == "tag:yaml.org,2002:bool") {
            value->kind_ = Value::Kind::Boolean;
            value->data_ = scalar == "true" || scalar == "True" || scalar == "TRUE";
            return value;
        }
        if (tag == "tag:yaml.org,2002:int") {
            Number number;
            number.lexeme = scalar;
            if (scalar.starts_with('-')) {
                i64 parsed{};
                auto result = std::from_chars(scalar.data(), scalar.data() + scalar.size(), parsed);
                if (result.ec != std::errc{})
                    diagnostics_.push_back({"CFG5007", Severity::Error, source_, value->range_, value->pointer_, "YAML integer is out of range", {}});
                number.kind = NumberKind::Signed;
                number.value = parsed;
            } else {
                u64 parsed{};
                auto result = std::from_chars(scalar.data(), scalar.data() + scalar.size(), parsed);
                if (result.ec != std::errc{})
                    diagnostics_.push_back({"CFG5007", Severity::Error, source_, value->range_, value->pointer_, "YAML integer is out of range", {}});
                number.kind = NumberKind::Unsigned;
                number.value = parsed;
            }
            value->kind_ = Value::Kind::Number;
            value->data_ = std::move(number);
            return value;
        }
        if (tag == "tag:yaml.org,2002:float") {
            f64 parsed{};
            auto result = std::from_chars(scalar.data(), scalar.data() + scalar.size(), parsed);
            if (result.ec != std::errc{} || !std::isfinite(parsed))
                diagnostics_.push_back({"CFG5008", Severity::Error, source_, value->range_, value->pointer_, "YAML number must be finite", {}});
            Number number{NumberKind::Real, scalar, parsed};
            value->kind_ = Value::Kind::Number;
            value->data_ = std::move(number);
            return value;
        }
        value->kind_ = Value::Kind::String;
        value->data_ = scalar;
        return value;
    }

    std::string_view text_;
    std::string source_;
    Limits limits_;
    u64 nodes_{}, members_{};
    std::vector<Diagnostic> diagnostics_;
};

std::expected<Document, std::vector<Diagnostic>> Document::ParseYaml(std::string_view text, std::string source, Limits limits) {
    return YamlParser(text, std::move(source), limits).Run();
}

std::expected<Document, std::vector<Diagnostic>> Document::ParseYamlFile(const std::filesystem::path& path, Limits limits) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return std::unexpected(std::vector{Diagnostic{"CFG5020", Severity::Error, path.string(), {}, {}, "cannot open YAML file", {}}});
    std::string text((std::istreambuf_iterator<char>(input)), {});
    return ParseYaml(text, path.string(), limits);
}

} // namespace woki::config
