#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <queue>
#include <set>
#include <sstream>

#include <woki/config.hpp>

namespace woki::config {
namespace {

std::string EscapePointer(std::string_view value) {
    std::string out;
    for (char c : value)
        c == '~' ? out += "~0" : c == '/' ? out += "~1" : out += c;
    return out;
}

bool Utf8(std::string_view text) {
    for (size_t i = 0; i < text.size();) {
        const u8 c = static_cast<u8>(text[i]);
        if (c < 0x80) {
            ++i;
            continue;
        }
        size_t count = c >= 0xf0 ? 4 : c >= 0xe0 ? 3 : c >= 0xc2 ? 2 : 0;
        if (!count || i + count > text.size())
            return false;
        u32 value = c & ((1U << (7U - count)) - 1U);
        for (size_t j = 1; j < count; ++j) {
            const u8 next = static_cast<u8>(text[i + j]);
            if ((next & 0xc0) != 0x80)
                return false;
            value = (value << 6U) | (next & 0x3fU);
        }
        if ((count == 2 && value < 0x80) || (count == 3 && value < 0x800) || (count == 4 && value < 0x10000) || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
            return false;
        i += count;
    }
    return true;
}

void AppendUtf8(std::string& out, u32 value) {
    if (value <= 0x7f)
        out.push_back(static_cast<char>(value));
    else if (value <= 0x7ff) {
        out.push_back(static_cast<char>(0xc0 | value >> 6));
        out.push_back(static_cast<char>(0x80 | (value & 0x3f)));
    } else if (value <= 0xffff) {
        out.push_back(static_cast<char>(0xe0 | value >> 12));
        out.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (value & 0x3f)));
    } else {
        out.push_back(static_cast<char>(0xf0 | value >> 18));
        out.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (value & 0x3f)));
    }
}

void Quote(std::string& out, std::string_view value) {
    static constexpr char hex[] = "0123456789abcdef";
    out.push_back('"');
    for (const char raw : value) {
        const auto c = static_cast<unsigned char>(raw);
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
                    out.push_back(hex[c & 15]);
                } else
                    out.push_back(static_cast<char>(c));
        }
    }
    out.push_back('"');
}

void Canonical(const Value& value, std::string& out) {
    switch (value.Type()) {
        case Value::Kind::Null:
            out += "null";
            break;
        case Value::Kind::Boolean:
            out += *value.Boolean() ? "true" : "false";
            break;
        case Value::Kind::Number: {
            const auto& number = *value.Numeric();
            if (number.kind == NumberKind::Signed)
                out += std::to_string(std::get<i64>(number.value));
            else if (number.kind == NumberKind::Unsigned)
                out += std::to_string(std::get<u64>(number.value));
            else {
                const f64 real = std::get<f64>(number.value);
                if (real == 0)
                    out.push_back('0');
                else {
                    char buffer[64];
                    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), real, std::chars_format::general, std::numeric_limits<f64>::max_digits10);
                    out.append(buffer, result.ptr);
                }
            }
            break;
        }
        case Value::Kind::String:
            Quote(out, *value.String());
            break;
        case Value::Kind::Array: {
            out.push_back('[');
            bool first = true;
            for (const auto& item : *value.Elements()) {
                if (!first)
                    out.push_back(',');
                first = false;
                Canonical(*item, out);
            }
            out.push_back(']');
            break;
        }
        case Value::Kind::Object: {
            std::vector<const Member*> members;
            for (const auto& member : *value.Members())
                members.push_back(&member);
            std::ranges::sort(members, {}, [](const Member* member) -> const std::string& { return member->key; });
            out.push_back('{');
            bool first = true;
            for (const Member* member : members) {
                if (!first)
                    out.push_back(',');
                first = false;
                Quote(out, member->key);
                out.push_back(':');
                Canonical(*member->value, out);
            }
            out.push_back('}');
            break;
        }
    }
}

Diagnostic At(const Value& value, std::string code, std::string message) {
    return {std::move(code), Severity::Error, {}, value.Range(), value.Pointer(), std::move(message), {}};
}

} // namespace

class Parser {
public:
    Parser(std::string_view text, std::string source, ParsePolicy policy)
        : text_(text),
          source_(std::move(source)),
          policy_(policy) {}

    std::expected<Document, std::vector<Diagnostic>> Run() {
        if (text_.size() > policy_.limits.bytes)
            Fail("CFG1001", "document byte limit exceeded");
        else if (!Utf8(text_))
            Fail("CFG1002", "source is not valid UTF-8");
        auto root = diagnostics_.empty() ? ParseValue({}, 0) : nullptr;
        Space();
        if (root && offset_ != text_.size())
            Fail("CFG1003", "unexpected content after root value");
        if (root && policy_.require_schema) {
            ObjectView object(*root);
            if (!object.Valid() || !object.String("$schema"))
                Fail("CFG1018", "authored document requires a string $schema URI", root->Range(), root->Pointer());
        }
        if (!diagnostics_.empty())
            return std::unexpected(std::move(diagnostics_));
        Document document;
        document.source_ = std::move(source_);
        document.text_ = std::string(text_);
        document.root_ = std::move(root);
        return document;
    }

private:
    SourcePosition Position() const noexcept {
        return {offset_, line_, column_};
    }

    char Peek(size_t ahead = 0) const noexcept {
        return offset_ + ahead < text_.size() ? text_[offset_ + ahead] : '\0';
    }

    char Take() {
        const char c = Peek();
        if (c) {
            ++offset_;
            if (c == '\n') {
                ++line_;
                column_ = 1;
            } else
                ++column_;
        }
        return c;
    }

    void Fail(std::string code, std::string message, SourceRange range = {}, std::string pointer = {}) {
        if (range.begin.byte == 0 && range.end.byte == 0)
            range = {Position(), Position()};
        diagnostics_.push_back({std::move(code), Severity::Error, source_, range, std::move(pointer), std::move(message), {}});
    }

    void Space() {
        for (;;) {
            while (Peek() == ' ' || Peek() == '\t' || Peek() == '\r' || Peek() == '\n')
                Take();
            if (Peek() != '/' || (Peek(1) != '/' && Peek(1) != '*'))
                return;
            if (!policy_.comments) {
                Fail("CFG1004", "comments are not allowed by the strict policy");
                return;
            }
            if (Peek(1) == '/') {
                Take();
                Take();
                while (Peek() && Peek() != '\n')
                    Take();
            } else {
                Take();
                Take();
                while (Peek() && !(Peek() == '*' && Peek(1) == '/'))
                    Take();
                if (!Peek()) {
                    Fail("CFG1005", "unterminated block comment");
                    return;
                }
                Take();
                Take();
            }
        }
    }

    std::shared_ptr<const Value> ParseValue(std::string pointer, u32 depth) {
        Space();
        const SourcePosition begin = Position();
        if (depth > policy_.limits.depth) {
            Fail("CFG1006", "document depth limit exceeded", {begin, begin}, pointer);
            return {};
        }
        if (++nodes_ > policy_.limits.nodes) {
            Fail("CFG1007", "document node limit exceeded", {begin, begin}, pointer);
            return {};
        }
        auto value = std::make_shared<Value>();
        value->pointer_ = std::move(pointer);
        if (Peek() == '{')
            ParseObject(*value, depth);
        else if (Peek() == '[')
            ParseArray(*value, depth);
        else if (Peek() == '"') {
            value->kind_ = Value::Kind::String;
            value->data_ = ParseString();
        } else if (Peek() == '-' || (Peek() >= '0' && Peek() <= '9'))
            ParseNumber(*value);
        else if (text_.substr(offset_, 4) == "true") {
            for (int i = 0; i < 4; ++i)
                Take();
            value->kind_ = Value::Kind::Boolean;
            value->data_ = true;
        } else if (text_.substr(offset_, 5) == "false") {
            for (int i = 0; i < 5; ++i)
                Take();
            value->kind_ = Value::Kind::Boolean;
            value->data_ = false;
        } else if (text_.substr(offset_, 4) == "null") {
            for (int i = 0; i < 4; ++i)
                Take();
        } else {
            Fail("CFG1008", "expected a JSON value", {begin, Position()}, value->pointer_);
            return {};
        }
        value->range_ = {begin, Position()};
        return value;
    }

    std::string ParseString() {
        std::string out;
        const auto begin = Position();
        Take();
        while (Peek() && Peek() != '"') {
            unsigned char c = static_cast<unsigned char>(Take());
            if (c < 0x20) {
                Fail("CFG1009", "unescaped control character in string", {begin, Position()});
                break;
            }
            if (c != '\\') {
                out.push_back(static_cast<char>(c));
                continue;
            }
            const char escape = Take();
            switch (escape) {
                case '"':
                    out.push_back('"');
                    break;
                case '\\':
                    out.push_back('\\');
                    break;
                case '/':
                    out.push_back('/');
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                case 'u': {
                    auto hex = [&]() -> std::optional<u32> {
                        u32 v{};
                        for (int i = 0; i < 4; ++i) {
                            char h = Take();
                            v <<= 4;
                            if (h >= '0' && h <= '9')
                                v += static_cast<u32>(h - '0');
                            else if (h >= 'a' && h <= 'f')
                                v += static_cast<u32>(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F')
                                v += static_cast<u32>(h - 'A' + 10);
                            else
                                return std::nullopt;
                        }
                        return v;
                    };
                    auto code = hex();
                    if (!code) {
                        Fail("CFG1010", "invalid Unicode escape", {begin, Position()});
                        break;
                    }
                    if (*code >= 0xd800 && *code <= 0xdbff) {
                        if (Take() != '\\' || Take() != 'u') {
                            Fail("CFG1011", "high surrogate requires a low surrogate", {begin, Position()});
                            break;
                        }
                        auto low = hex();
                        if (!low || *low < 0xdc00 || *low > 0xdfff) {
                            Fail("CFG1011", "invalid low surrogate", {begin, Position()});
                            break;
                        }
                        *code = 0x10000 + ((*code - 0xd800) << 10) + (*low - 0xdc00);
                    } else if (*code >= 0xdc00 && *code <= 0xdfff) {
                        Fail("CFG1011", "unpaired low surrogate", {begin, Position()});
                        break;
                    }
                    AppendUtf8(out, *code);
                    break;
                }
                default:
                    Fail("CFG1012", "invalid string escape", {begin, Position()});
            }
            if (out.size() > policy_.limits.string_bytes) {
                Fail("CFG1013", "decoded string limit exceeded", {begin, Position()});
                break;
            }
        }
        if (Peek() != '"')
            Fail("CFG1014", "unterminated string", {begin, Position()});
        else
            Take();
        if (!Utf8(out))
            Fail("CFG1002", "decoded string is not valid UTF-8", {begin, Position()});
        return out;
    }

    void ParseNumber(Value& value) {
        const size_t begin = offset_;
        if (Peek() == '-')
            Take();
        if (Peek() == '0') {
            Take();
            if (Peek() >= '0' && Peek() <= '9')
                Fail("CFG1015", "leading zeros are not allowed");
        } else {
            if (Peek() < '1' || Peek() > '9')
                Fail("CFG1015", "invalid number");
            while (Peek() >= '0' && Peek() <= '9')
                Take();
        }
        bool real = false;
        if (Peek() == '.') {
            real = true;
            Take();
            if (Peek() < '0' || Peek() > '9')
                Fail("CFG1015", "fraction requires digits");
            while (Peek() >= '0' && Peek() <= '9')
                Take();
        }
        if (Peek() == 'e' || Peek() == 'E') {
            real = true;
            Take();
            if (Peek() == '+' || Peek() == '-')
                Take();
            if (Peek() < '0' || Peek() > '9')
                Fail("CFG1015", "exponent requires digits");
            while (Peek() >= '0' && Peek() <= '9')
                Take();
        }
        Number number;
        number.lexeme = std::string(text_.substr(begin, offset_ - begin));
        if (real) {
            f64 parsed{};
            auto result = std::from_chars(number.lexeme.data(), number.lexeme.data() + number.lexeme.size(), parsed);
            if (result.ec != std::errc{} || !std::isfinite(parsed))
                Fail("CFG1016", "number is outside the finite double range");
            number.kind = NumberKind::Real;
            number.value = parsed;
        } else if (number.lexeme.starts_with('-')) {
            i64 parsed{};
            auto result = std::from_chars(number.lexeme.data(), number.lexeme.data() + number.lexeme.size(), parsed);
            if (result.ec != std::errc{})
                Fail("CFG1016", "signed integer is out of range");
            number.kind = NumberKind::Signed;
            number.value = parsed;
        } else {
            u64 parsed{};
            auto result = std::from_chars(number.lexeme.data(), number.lexeme.data() + number.lexeme.size(), parsed);
            if (result.ec != std::errc{})
                Fail("CFG1016", "unsigned integer is out of range");
            number.kind = NumberKind::Unsigned;
            number.value = parsed;
        }
        value.kind_ = Value::Kind::Number;
        value.data_ = std::move(number);
    }

    void ParseArray(Value& value, u32 depth) {
        value.kind_ = Value::Kind::Array;
        Value::Array array;
        Take();
        Space();
        if (Peek() == ']') {
            Take();
            value.data_ = std::move(array);
            return;
        }
        for (size_t index = 0;; ++index) {
            auto child = ParseValue(value.pointer_ + '/' + std::to_string(index), depth + 1);
            if (!child)
                break;
            array.push_back(std::move(child));
            Space();
            if (Peek() == ']') {
                Take();
                break;
            }
            if (Peek() != ',') {
                Fail("CFG1003", "expected ',' or ']' in array");
                break;
            }
            Take();
            Space();
            if (Peek() == ']') {
                if (!policy_.trailing_commas)
                    Fail("CFG1017", "trailing comma is not allowed");
                Take();
                break;
            }
        }
        value.data_ = std::move(array);
    }

    void ParseObject(Value& value, u32 depth) {
        value.kind_ = Value::Kind::Object;
        Value::Object object;
        std::map<std::string, SourceRange, std::less<>> keys;
        Take();
        Space();
        if (Peek() == '}') {
            Take();
            value.data_ = std::move(object);
            return;
        }
        for (;;) {
            if (Peek() != '"') {
                Fail("CFG1003", "object key must be a string");
                break;
            }
            const SourcePosition key_begin = Position();
            std::string key = ParseString();
            SourceRange key_range{key_begin, Position()};
            if (++members_ > policy_.limits.members) {
                Fail("CFG1007", "object member limit exceeded", key_range);
                break;
            }
            if (auto [it, inserted] = keys.emplace(key, key_range); !inserted) {
                Diagnostic diagnostic{"CFG1019", Severity::Error, source_, key_range, value.pointer_ + '/' + EscapePointer(key), "duplicate decoded object key: " + key, {}};
                diagnostic.notes.push_back({"first key is here", it->second});
                diagnostics_.push_back(std::move(diagnostic));
            }
            Space();
            if (Take() != ':') {
                Fail("CFG1003", "expected ':' after object key");
                break;
            }
            auto child = ParseValue(value.pointer_ + '/' + EscapePointer(key), depth + 1);
            if (!child)
                break;
            object.push_back({std::move(key), key_range, std::move(child)});
            Space();
            if (Peek() == '}') {
                Take();
                break;
            }
            if (Peek() != ',') {
                Fail("CFG1003", "expected ',' or '}' in object");
                break;
            }
            Take();
            Space();
            if (Peek() == '}') {
                if (!policy_.trailing_commas)
                    Fail("CFG1017", "trailing comma is not allowed");
                Take();
                break;
            }
        }
        value.data_ = std::move(object);
    }

    std::string_view text_;
    std::string source_;
    ParsePolicy policy_;
    size_t offset_{};
    u32 line_{1}, column_{1};
    u64 nodes_{}, members_{};
    std::vector<Diagnostic> diagnostics_;
};

ParsePolicy ParsePolicy::Strict() noexcept {
    return {};
}

ParsePolicy ParsePolicy::Authored() noexcept {
    ParsePolicy result;
    result.comments = true;
    result.trailing_commas = true;
    result.require_schema = true;
    return result;
}

std::expected<Document, std::vector<Diagnostic>> Document::Parse(std::string_view text, std::string source, ParsePolicy policy) {
    return Parser(text, std::move(source), policy).Run();
}

std::expected<Document, std::vector<Diagnostic>> Document::ParseFile(const std::filesystem::path& path, ParsePolicy policy) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return std::unexpected(std::vector{Diagnostic{"CFG1020", Severity::Error, path.string(), {}, {}, "cannot open configuration file", {}}});
    std::string text((std::istreambuf_iterator<char>(input)), {});
    return Parse(text, path.string(), policy);
}

const Member* ObjectView::FindMember(std::string_view key) const noexcept {
    const auto* members = value_ ? value_->Members() : nullptr;
    if (!members)
        return nullptr;
    const auto found = std::ranges::find(*members, key, &Member::key);
    return found == members->end() ? nullptr : &*found;
}

const Value* ObjectView::Find(std::string_view key) const noexcept {
    const auto* member = FindMember(key);
    return member ? member->value.get() : nullptr;
}

std::span<const Member> ObjectView::Members() const noexcept {
    const auto* members = value_ ? value_->Members() : nullptr;
    return members ? std::span<const Member>(*members) : std::span<const Member>{};
}

std::optional<std::string_view> ObjectView::String(std::string_view key) const noexcept {
    const auto* value = Find(key);
    const auto* string = value ? value->String() : nullptr;
    return string ? std::optional<std::string_view>(*string) : std::nullopt;
}

std::optional<bool> ObjectView::Boolean(std::string_view key) const noexcept {
    const auto* value = Find(key);
    const auto* boolean = value ? value->Boolean() : nullptr;
    return boolean ? std::optional(*boolean) : std::nullopt;
}

std::optional<i64> ObjectView::Integer(std::string_view key) const noexcept {
    const auto* value = Find(key);
    const auto* number = value ? value->Numeric() : nullptr;
    if (!number)
        return {};
    if (number->kind == NumberKind::Signed)
        return std::get<i64>(number->value);
    if (number->kind == NumberKind::Unsigned && std::get<u64>(number->value) <= static_cast<u64>(std::numeric_limits<i64>::max()))
        return static_cast<i64>(std::get<u64>(number->value));
    return {};
}

std::optional<u64> ObjectView::Unsigned(std::string_view key) const noexcept {
    const auto* value = Find(key);
    const auto* number = value ? value->Numeric() : nullptr;
    if (!number)
        return {};
    if (number->kind == NumberKind::Unsigned)
        return std::get<u64>(number->value);
    if (number->kind == NumberKind::Signed && std::get<i64>(number->value) >= 0)
        return static_cast<u64>(std::get<i64>(number->value));
    return {};
}

std::optional<f64> ObjectView::Real(std::string_view key) const noexcept {
    const auto* value = Find(key);
    const auto* number = value ? value->Numeric() : nullptr;
    if (!number)
        return {};
    if (number->kind == NumberKind::Real)
        return std::get<f64>(number->value);
    if (number->kind == NumberKind::Signed)
        return static_cast<f64>(std::get<i64>(number->value));
    return static_cast<f64>(std::get<u64>(number->value));
}

std::optional<ObjectView> ObjectView::Object(std::string_view key) const noexcept {
    const auto* value = Find(key);
    return value && value->Members() ? std::optional(ObjectView(*value)) : std::nullopt;
}

std::optional<ArrayView> ObjectView::Array(std::string_view key) const noexcept {
    const auto* value = Find(key);
    return value && value->Elements() ? std::optional(ArrayView(*value)) : std::nullopt;
}

std::expected<Json, std::vector<Diagnostic>> Json::Parse(std::string_view text, std::string source, ParsePolicy policy) {
    auto document = Document::Parse(text, std::move(source), policy);
    if (!document)
        return std::unexpected(std::move(document.error()));
    auto owner = std::make_shared<Document>(std::move(*document));
    return Json(owner, &owner->Root());
}

Json Json::FromDocument(Document document) {
    auto owner = std::make_shared<Document>(std::move(document));
    return Json(owner, &owner->Root());
}

Json Json::operator[](std::string_view key) const noexcept {
    if (!is_object())
        return {};
    const Value* child = ObjectView(*value_).Find(key);
    return child ? Json(owner_, child) : Json{};
}

Json Json::operator[](size_t index) const noexcept {
    if (!is_array() || index >= value_->Elements()->size())
        return {};
    return Json(owner_, (*value_->Elements())[index].get());
}

std::vector<std::pair<std::string, Json>> Json::items() const {
    std::vector<std::pair<std::string, Json>> result;
    if (is_object())
        for (const auto& member : *value_->Members())
            result.emplace_back(member.key, Json(owner_, member.value.get()));
    return result;
}

bool RejectUnknown(const ObjectView& object, std::span<const std::string_view> allowed, std::vector<Diagnostic>& diagnostics, std::string_view code) {
    bool valid = true;
    for (const auto& member : object.Members())
        if (std::ranges::find(allowed, member.key) == allowed.end()) {
            diagnostics.push_back({std::string(code), Severity::Error, {}, member.key_range, member.value->Pointer(), "unknown key: " + member.key, {}});
            valid = false;
        }
    return valid;
}

std::optional<std::string_view> RequiredString(const ObjectView& object, std::string_view key, std::vector<Diagnostic>& diagnostics, std::string_view code) {
    auto value = object.String(key);
    if (!value) {
        const Value* found = object.Find(key);
        diagnostics
            .push_back(found ? At(*found, std::string(code), std::string(key) + " must be a string") : Diagnostic{std::string(code), Severity::Error, {}, {}, {}, "missing required string: " + std::string(key), {}});
    }
    return value;
}

std::optional<u64> RequiredUnsigned(const ObjectView& object, std::string_view key, std::vector<Diagnostic>& diagnostics, std::string_view code) {
    auto value = object.Unsigned(key);
    if (!value) {
        const Value* found = object.Find(key);
        diagnostics.push_back(found ? At(*found, std::string(code), std::string(key) + " must be an unsigned integer")
                                    : Diagnostic{std::string(code), Severity::Error, {}, {}, {}, "missing required integer: " + std::string(key), {}});
    }
    return value;
}

std::optional<std::vector<std::string>> OptionalStrings(const ObjectView& object, std::string_view key, std::vector<Diagnostic>& diagnostics, std::string_view code) {
    const Value* value = object.Find(key);
    if (!value)
        return std::vector<std::string>{};
    ArrayView array(*value);
    if (!array.Valid()) {
        diagnostics.push_back(At(*value, std::string(code), std::string(key) + " must be an array"));
        return {};
    }
    std::vector<std::string> result;
    for (size_t i = 0; i < array.Size(); ++i) {
        const auto* text = array.At(i)->String();
        if (!text)
            diagnostics.push_back(At(*array.At(i), std::string(code), "array member must be a string"));
        else
            result.push_back(*text);
    }
    return result;
}

namespace {
bool TypeMatches(const Value& value, SchemaType type) {
    if (type == SchemaType::Any)
        return true;
    if (type == SchemaType::Null)
        return value.Type() == Value::Kind::Null;
    if (type == SchemaType::Boolean)
        return value.Boolean();
    if (type == SchemaType::String)
        return value.String();
    if (type == SchemaType::Array)
        return value.Elements();
    if (type == SchemaType::Object)
        return value.Members();
    if (type == SchemaType::Integer)
        return value.Numeric() && value.Numeric()->kind != NumberKind::Real;
    return value.Numeric();
}

void ValidateValue(const Value& value, const Schema& schema, std::vector<Diagnostic>& out) {
    if (!TypeMatches(value, schema.type)) {
        out.push_back(At(value, "CFG3001", "value has the wrong schema type"));
        return;
    }
    if (!schema.one_of.empty()) {
        size_t matches = 0;
        for (const auto& candidate : schema.one_of) {
            std::vector<Diagnostic> local;
            ValidateValue(value, *candidate, local);
            if (local.empty())
                ++matches;
        }
        if (matches != 1)
            out.push_back(At(value, "CFG3002", "value must match exactly one union branch"));
    }
    if (const auto* string = value.String()) {
        if (schema.min_length && string->size() < *schema.min_length)
            out.push_back(At(value, "CFG3003", "string is shorter than allowed"));
        if (schema.max_length && string->size() > *schema.max_length)
            out.push_back(At(value, "CFG3003", "string is longer than allowed"));
        if (!schema.enum_values.empty() && std::ranges::find(schema.enum_values, *string) == schema.enum_values.end())
            out.push_back(At(value, "CFG3004", "string is not an enum value"));
        if (schema.const_value && *schema.const_value != *string)
            out.push_back(At(value, "CFG3005", "value does not equal const"));
    }
    if (const auto* number = value.Numeric()) {
        f64 n = number->kind == NumberKind::Real ? std::get<f64>(number->value) : number->kind == NumberKind::Signed ? static_cast<f64>(std::get<i64>(number->value)) : static_cast<f64>(std::get<u64>(number->value));
        if (schema.minimum && n < *schema.minimum)
            out.push_back(At(value, "CFG3006", "number is below minimum"));
        if (schema.maximum && n > *schema.maximum)
            out.push_back(At(value, "CFG3006", "number is above maximum"));
    }
    if (const auto* array = value.Elements()) {
        if (schema.min_items && array->size() < *schema.min_items)
            out.push_back(At(value, "CFG3007", "array has too few items"));
        if (schema.max_items && array->size() > *schema.max_items)
            out.push_back(At(value, "CFG3007", "array has too many items"));
        std::set<std::string> seen;
        for (size_t i = 0; i < array->size(); ++i) {
            const SchemaPtr item = i < schema.tuple.size() ? schema.tuple[i] : schema.items;
            if (item)
                ValidateValue(*(*array)[i], *item, out);
            if (schema.unique_items && !seen.insert(CanonicalJson(*(*array)[i])).second)
                out.push_back(At(*(*array)[i], "CFG3008", "array item is duplicated"));
        }
    }
    if (value.Members()) {
        ObjectView object(value);
        for (const auto& required : schema.required)
            if (!object.Find(required))
                out.push_back(At(value, "CFG3009", "missing required property: " + required));
        for (const auto& member : object.Members()) {
            const auto found = schema.properties.find(member.key);
            if (found != schema.properties.end())
                ValidateValue(*member.value, *found->second, out);
            else if (!schema.additional_properties)
                out.push_back(At(*member.value, "CFG3010", "unknown property: " + member.key));
        }
    }
    if (schema.predicate)
        schema.predicate(value, out);
}

void SchemaJson(const Schema& schema, Writer& out) {
    out.BeginObject();
    if (schema.type != SchemaType::Any) {
        static constexpr std::string_view names[]{"any", "null", "boolean", "integer", "number", "string", "array", "object"};
        out.Key("type");
        out.String(names[static_cast<size_t>(schema.type)]);
    }
    if (!schema.properties.empty()) {
        out.Key("properties");
        out.BeginObject();
        for (const auto& [name, child] : schema.properties) {
            out.Key(name);
            SchemaJson(*child, out);
        }
        out.EndObject();
    }
    if (!schema.required.empty()) {
        out.Key("required");
        out.BeginArray();
        for (const auto& name : schema.required)
            out.String(name);
        out.EndArray();
    }
    if (!schema.additional_properties) {
        out.Key("additionalProperties");
        out.Boolean(false);
    }
    if (schema.items) {
        out.Key("items");
        SchemaJson(*schema.items, out);
    }
    if (!schema.enum_values.empty()) {
        out.Key("enum");
        out.BeginArray();
        for (const auto& value : schema.enum_values)
            out.String(value);
        out.EndArray();
    }
    out.EndObject();
}

SchemaPtr StringSchema() {
    auto s = std::make_shared<Schema>();
    s->type = SchemaType::String;
    return s;
}

SchemaPtr TypeSchema(SchemaType type) {
    auto schema = std::make_shared<Schema>();
    schema->type = type;
    return schema;
}

Schema FamilySchema(std::string family, u32 version) {
    Schema root;
    root.type = SchemaType::Object;
    root.additional_properties = true;
    root.required = {"$schema"};
    root.properties["$schema"] = StringSchema();
    static_cast<void>(family);
    static_cast<void>(version);
    return root;
}
} // namespace

std::vector<Diagnostic> Validate(const Document& document, const Schema& schema) {
    std::vector<Diagnostic> out;
    ValidateValue(document.Root(), schema, out);
    for (auto& diagnostic : out)
        diagnostic.source = std::string(document.Source());
    return out;
}

Registry& Registry::Global() {
    static Registry registry = [] {
        Registry value;
        RegisterBuiltinSchemas(value);
        return value;
    }();
    return registry;
}

void Registry::Register(SchemaId id, Schema schema, std::vector<std::string> extensions, bool current) {
    entries_.push_back({std::move(id), std::move(schema), std::move(extensions), current});
}

void Registry::RegisterMigration(std::string family, u32 from, u32 to, Migration migration) {
    edges_.push_back({std::move(family), from, to, std::move(migration)});
}

const Schema* Registry::Find(std::string_view family, u32 version) const noexcept {
    const auto found = std::ranges::find_if(entries_, [&](const Entry& entry) { return entry.id.family == family && entry.id.version == version; });
    return found == entries_.end() ? nullptr : &found->schema;
}

std::optional<u32> Registry::Current(std::string_view family) const noexcept {
    std::optional<u32> result;
    for (const auto& entry : entries_)
        if (entry.id.family == family && entry.current && (!result || entry.id.version > *result))
            result = entry.id.version;
    return result;
}

bool Registry::Maintained(const std::string_view family, const u32 version) const noexcept {
    const auto current = Current(family);
    if (!current || !Find(family, version))
        return false;
    if (version == *current)
        return true;
    std::set<u32> reachable{version};
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& edge : edges_)
            if (edge.family == family && reachable.contains(edge.from) && reachable.insert(edge.to).second)
                changed = true;
    }
    return reachable.contains(*current);
}

std::expected<Document, std::vector<Diagnostic>> Registry::Migrate(const Document& document, std::string_view family, u32 from, u32 to) const {
    if (from == to)
        return document;
    std::queue<std::pair<u32, Document>> queue;
    std::set<u32> visited{from};
    queue.emplace(from, document);
    while (!queue.empty()) {
        auto [version, current] = std::move(queue.front());
        queue.pop();
        for (const auto& edge : edges_)
            if (edge.family == family && edge.from == version && visited.insert(edge.to).second) {
                auto migrated = edge.migration(current);
                if (!migrated)
                    return migrated;
                if (edge.to == to)
                    return migrated;
                queue.emplace(edge.to, std::move(*migrated));
            }
    }
    return std::unexpected(std::vector{Diagnostic{"CFG4001", Severity::Error, std::string(document.Source()), {}, {}, "no registered migration path", {}}});
}

std::string Registry::JsonSchema(std::string_view family, u32 version) const {
    const Schema* schema = Find(family, version);
    if (!schema)
        return {};
    Writer out(true);
    SchemaJson(*schema, out);
    return out.Str();
}

std::string Registry::Catalog() const {
    Writer out(true);
    out.BeginObject();
    out.Key("schemas");
    out.BeginArray();
    for (const auto& entry : entries_) {
        out.BeginObject();
        out.Key("name");
        out.String(entry.id.family);
        out.Key("version");
        out.Unsigned(entry.id.version);
        out.Key("url");
        out.String("https://schemas.woki.dev/" + entry.id.family + "/v" + std::to_string(entry.id.version) + ".schema.json");
        out.EndObject();
    }
    out.EndArray();
    out.EndObject();
    return out.Str();
}

void RegisterBuiltinSchemas(Registry& registry) {
    const std::pair<std::string_view, u32> families[] = {{"studio.settings", 1}, {"extension.manifest", 1}, {"gfx.shader", 2}, {"gpu-struct", 1}, {"shader-pack", 1}, {"pipeline", 1}, {"feature", 1}, {"quality", 1},
        {"material-type", 1}, {"material", 1}, {"texture", 1}, {"mesh-import", 1}, {"cook-manifest", 1}, {"mesh-manifest", 1}, {"ui.theme", 1}, {"ui.dock", 1}, {"ui.debug-tree", 1}};
    const std::map<std::string_view, std::vector<std::string_view>, std::less<>> keys{
        {"studio.settings", {"app", "window", "ui"}},
        {"extension.manifest", {"id", "name", "version", "apiVersion", "runtime", "libraries", "permissions", "activation", "contributes"}},
        {"gfx.shader", {"name", "language", "sources", "entry_points", "permutations", "capabilities", "bindings", "groups", "compile_options"}},
        {"gpu-struct", {"name", "address_space", "layout_class", "fields"}},
        {"shader-pack", {"id", "version", "dependencies", "shaders"}},
        {"pipeline", {"name", "renderPath", "targets", "features", "extensionPoints", "qualityProfile", "fallbacks"}},
        {"feature", {"feature", "settings"}},
        {"quality", {"name", "features"}},
        {"material-type", {"id", "name", "shader", "product_family", "passes", "entry_points", "render_state", "properties", "textures", "samplers", "overrides"}},
        {"material", {"id", "type", "overrides"}},
        {"texture", {"asset_id", "source", "semantic", "color_space", "dimension", "mips", "streaming", "max_size", "target", "sampler"}},
        {"mesh-import", {"id", "source", "importer", "coordinates", "unit_scale", "tangents", "optimize", "lod_ratios", "lod_errors", "meshlets", "quantize", "compress", "animations"}},
        {"cook-manifest",
            {"package", "shaders", "shaderProducts", "pipelines", "pipelineProducts", "materials", "materialProducts", "textures", "textureProducts", "meshes", "meshProducts", "environmentFallbacks", "runtimeFiles"}},
        {"mesh-manifest", {"assets", "products"}},
        {"ui.theme", {"name", "color", "space", "radius", "type", "motion"}},
        {"ui.dock", {"root"}},
        {"ui.debug-tree", {"key", "token", "kind", "content", "bounds", "dirty", "stats", "semantics", "children"}},
    };
    for (const auto& [family, version] : families) {
        Schema schema = FamilySchema(std::string(family), version);
        schema.additional_properties = false;
        schema.properties["schema"] = TypeSchema(SchemaType::Integer);
        if (const auto found = keys.find(family); found != keys.end())
            for (const auto key : found->second)
                schema.properties[std::string(key)] = TypeSchema(SchemaType::Any);
        registry.Register({std::string(family), version}, std::move(schema));
    }
    registry.Register({"gfx.shader", 1}, FamilySchema("gfx.shader", 1), {}, false);
    registry.RegisterMigration("gfx.shader", 1, 2, [](const Document& source) -> std::expected<Document, std::vector<Diagnostic>> {
        Writer out(true);
        std::function<void(const Value&)> write = [&](const Value& value) {
            if (value.Type() == Value::Kind::Null)
                out.Null();
            else if (value.Boolean())
                out.Boolean(*value.Boolean());
            else if (value.String())
                out.String(*value.String());
            else if (const Number* number = value.Numeric()) {
                if (number->kind == NumberKind::Signed)
                    out.Integer(std::get<i64>(number->value));
                else if (number->kind == NumberKind::Unsigned)
                    out.Unsigned(std::get<u64>(number->value));
                else
                    out.Real(std::get<f64>(number->value));
            } else if (value.Elements()) {
                out.BeginArray();
                for (const auto& child : *value.Elements())
                    write(*child);
                out.EndArray();
            } else {
                out.BeginObject();
                if (value.Pointer().empty()) {
                    out.Key("$schema");
                    out.String("https://schemas.woki.dev/gfx.shader/v2.schema.json");
                    out.Key("schema");
                    out.Unsigned(2);
                }
                for (const auto& member : *value.Members()) {
                    if (value.Pointer().empty() && (member.key == "$schema" || member.key == "schema"))
                        continue;
                    out.Key(member.key);
                    write(*member.value);
                }
                out.EndObject();
            }
        };
        write(source.Root());
        return Document::Parse(out.Str(), std::string(source.Source()), ParsePolicy::Strict());
    });
}

void Writer::Indent() {
    if (pretty_)
        output_.append(stack_.size() * 2, ' ');
}

void Writer::Prefix(bool key) {
    if (stack_.empty())
        return;
    Frame& frame = stack_.back();
    if (frame.type == '{' && !key && frame.after_key) {
        frame.after_key = false;
        return;
    }
    if (!frame.first)
        output_ += ',';
    frame.first = false;
    if (pretty_) {
        output_ += '\n';
        Indent();
    }
}

void Writer::Null() {
    Prefix();
    output_ += "null";
}

void Writer::Boolean(bool value) {
    Prefix();
    output_ += value ? "true" : "false";
}

void Writer::Integer(i64 value) {
    Prefix();
    output_ += std::to_string(value);
}

void Writer::Unsigned(u64 value) {
    Prefix();
    output_ += std::to_string(value);
}

void Writer::Real(f64 value) {
    Prefix();
    if (!std::isfinite(value)) {
        output_ += "null";
        return;
    }
    if (value == 0) {
        output_.push_back('0');
        return;
    }
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general, std::numeric_limits<f64>::max_digits10);
    output_.append(buffer, result.ptr);
}

void Writer::String(std::string_view value) {
    Prefix();
    Quote(output_, value);
}

void Writer::BeginObject() {
    Prefix();
    output_ += '{';
    stack_.push_back({'{'});
}

void Writer::BeginArray() {
    Prefix();
    output_ += '[';
    stack_.push_back({'['});
}

void Writer::Key(std::string_view key) {
    Prefix(true);
    Quote(output_, key);
    output_ += pretty_ ? ": " : ":";
    stack_.back().after_key = true;
}

void Writer::EndObject() {
    const bool nonempty = !stack_.back().first;
    stack_.pop_back();
    if (pretty_ && nonempty) {
        output_ += '\n';
        Indent();
    }
    output_ += '}';
}

void Writer::EndArray() {
    const bool nonempty = !stack_.back().first;
    stack_.pop_back();
    if (pretty_ && nonempty) {
        output_ += '\n';
        Indent();
    }
    output_ += ']';
}

std::string CanonicalJson(const Value& value) {
    std::string out;
    Canonical(value, out);
    return out;
}

ContentHash CanonicalHash(const Document& document, std::string_view family, u32 version) {
    return Sha256("woki.config.semantic.v1\n" + std::string(family) + "\n" + std::to_string(version) + "\n" + CanonicalJson(document.Root()));
}

std::string FormatDiagnostics(std::span<const Diagnostic> diagnostics) {
    std::string out;
    for (const auto& diagnostic : diagnostics)
        out += diagnostic.source + ':' + std::to_string(diagnostic.range.begin.line) + ':' + std::to_string(diagnostic.range.begin.column) + ": " + diagnostic.code + ": " + diagnostic.message
               + (diagnostic.pointer.empty() ? "" : " [" + diagnostic.pointer + "]") + '\n';
    return out;
}

} // namespace woki::config
