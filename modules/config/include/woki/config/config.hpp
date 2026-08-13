#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <filesystem>
#include <expected>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <woki/core.hpp>

namespace woki::config {

struct SourcePosition {
    u64 byte{};
    u32 line{1};
    u32 column{1};
};

struct SourceRange {
    SourcePosition begin{};
    SourcePosition end{};
};
enum class Severity : u8 { Note, Warning, Error };

struct Note {
    std::string message;
    std::optional<SourceRange> range;
};

struct Diagnostic {
    std::string code;
    Severity severity{Severity::Error};
    std::string source;
    SourceRange range{};
    std::string pointer;
    std::string message;
    std::vector<Note> notes;
};

struct Limits {
    u64 bytes{4 * 1024 * 1024};
    u32 depth{64};
    u64 nodes{1'000'000};
    u64 members{250'000};
    u64 string_bytes{1024 * 1024};
};

struct ParsePolicy {
    bool comments{};
    bool trailing_commas{};
    bool require_schema{};
    Limits limits{};
    static ParsePolicy Strict() noexcept;
    static ParsePolicy Authored() noexcept;
};

enum class NumberKind : u8 { Signed, Unsigned, Real };

struct Number {
    NumberKind kind{};
    std::string lexeme;
    std::variant<i64, u64, f64> value;
};

class Value;

struct Member {
    std::string key;
    SourceRange key_range{};
    std::shared_ptr<const Value> value;
};

class Value {
public:
    using Array = std::vector<std::shared_ptr<const Value>>;
    using Object = std::vector<Member>;
    enum class Kind : u8 { Null, Boolean, Number, String, Array, Object };

    [[nodiscard]] Kind Type() const noexcept {
        return kind_;
    }

    [[nodiscard]] const SourceRange& Range() const noexcept {
        return range_;
    }

    [[nodiscard]] const std::string& Pointer() const noexcept {
        return pointer_;
    }

    [[nodiscard]] const bool* Boolean() const noexcept {
        return std::get_if<bool>(&data_);
    }

    [[nodiscard]] const Number* Numeric() const noexcept {
        return std::get_if<Number>(&data_);
    }

    [[nodiscard]] const std::string* String() const noexcept {
        return std::get_if<std::string>(&data_);
    }

    [[nodiscard]] const Array* Elements() const noexcept {
        return std::get_if<Array>(&data_);
    }

    [[nodiscard]] const Object* Members() const noexcept {
        return std::get_if<Object>(&data_);
    }

private:
    friend class Parser;
    friend class YamlParser;
    friend class Writer;
    Kind kind_{Kind::Null};
    SourceRange range_{};
    std::string pointer_;
    std::variant<std::monostate, bool, Number, std::string, Array, Object> data_;
};

class Document {
public:
    Document() = default;
    [[nodiscard]] static std::expected<Document, std::vector<Diagnostic>> Parse(std::string_view text, std::string source = {}, ParsePolicy policy = ParsePolicy::Strict());
    [[nodiscard]] static std::expected<Document, std::vector<Diagnostic>> ParseFile(const std::filesystem::path& path, ParsePolicy policy = ParsePolicy::Strict());
    [[nodiscard]] static std::expected<Document, std::vector<Diagnostic>> ParseYaml(std::string_view text, std::string source = {}, Limits limits = {});
    [[nodiscard]] static std::expected<Document, std::vector<Diagnostic>> ParseYamlFile(const std::filesystem::path& path, Limits limits = {});

    [[nodiscard]] const Value& Root() const noexcept {
        return *root_;
    }

    [[nodiscard]] std::string_view Source() const noexcept {
        return source_;
    }

    [[nodiscard]] std::string_view Text() const noexcept {
        return text_;
    }

    [[nodiscard]] ContentHash ExactSourceHash() const noexcept {
        return Sha256(text_);
    }

private:
    friend class Parser;
    friend class YamlParser;
    std::string source_;
    std::string text_;
    std::shared_ptr<const Value> root_;
};

class ArrayView {
public:
    explicit ArrayView(const Value& value) noexcept
        : value_(&value) {}

    [[nodiscard]] bool Valid() const noexcept {
        return value_ && value_->Elements();
    }

    [[nodiscard]] size_t Size() const noexcept {
        const auto* v = value_ ? value_->Elements() : nullptr;
        return v ? v->size() : 0;
    }

    [[nodiscard]] const Value* At(size_t index) const noexcept {
        const auto* v = value_ ? value_->Elements() : nullptr;
        return v && index < v->size() ? (*v)[index].get() : nullptr;
    }

private:
    const Value* value_{};
};

class ObjectView {
public:
    explicit ObjectView(const Value& value) noexcept
        : value_(&value) {}

    [[nodiscard]] bool Valid() const noexcept {
        return value_ && value_->Members();
    }

    [[nodiscard]] const Value* Find(std::string_view key) const noexcept;
    [[nodiscard]] const Member* FindMember(std::string_view key) const noexcept;
    [[nodiscard]] std::span<const Member> Members() const noexcept;
    [[nodiscard]] std::optional<std::string_view> String(std::string_view key) const noexcept;
    [[nodiscard]] std::optional<bool> Boolean(std::string_view key) const noexcept;
    [[nodiscard]] std::optional<i64> Integer(std::string_view key) const noexcept;
    [[nodiscard]] std::optional<u64> Unsigned(std::string_view key) const noexcept;
    [[nodiscard]] std::optional<f64> Real(std::string_view key) const noexcept;
    [[nodiscard]] std::optional<ObjectView> Object(std::string_view key) const noexcept;
    [[nodiscard]] std::optional<ArrayView> Array(std::string_view key) const noexcept;

private:
    const Value* value_{};
};

// Transitional view for domain converters. It owns a Document and never
// exposes a third-party JSON type; new code should prefer ObjectView.
class Json {
public:
    Json() = default;
    [[nodiscard]] static std::expected<Json, std::vector<Diagnostic>> Parse(std::string_view text, std::string source = {}, ParsePolicy policy = ParsePolicy{true, true, false, {}});
    [[nodiscard]] static Json FromDocument(Document document);

    explicit operator bool() const noexcept {
        return value_ != nullptr;
    }

    [[nodiscard]] bool is_null() const noexcept {
        return !value_ || value_->Type() == Value::Kind::Null;
    }

    [[nodiscard]] bool is_object() const noexcept {
        return value_ && value_->Members();
    }

    [[nodiscard]] bool is_array() const noexcept {
        return value_ && value_->Elements();
    }

    [[nodiscard]] bool is_string() const noexcept {
        return value_ && value_->String();
    }

    [[nodiscard]] bool is_boolean() const noexcept {
        return value_ && value_->Boolean();
    }

    [[nodiscard]] bool is_number() const noexcept {
        return value_ && value_->Numeric();
    }

    [[nodiscard]] bool is_number_integer() const noexcept {
        return is_number() && value_->Numeric()->kind != NumberKind::Real;
    }

    [[nodiscard]] bool is_number_unsigned() const noexcept {
        return is_number() && value_->Numeric()->kind == NumberKind::Unsigned;
    }

    [[nodiscard]] bool is_number_float() const noexcept {
        return is_number() && value_->Numeric()->kind == NumberKind::Real;
    }

    [[nodiscard]] bool contains(std::string_view key) const noexcept {
        return is_object() && ObjectView(*value_).Find(key);
    }

    [[nodiscard]] Json operator[](std::string_view key) const noexcept;
    [[nodiscard]] Json operator[](size_t index) const noexcept;

    [[nodiscard]] Json at(std::string_view key) const noexcept {
        return (*this)[key];
    }

    [[nodiscard]] size_t size() const noexcept {
        return is_array() ? value_->Elements()->size() : is_object() ? value_->Members()->size() : 0;
    }

    [[nodiscard]] bool empty() const noexcept {
        return size() == 0;
    }

    template <typename T>
    [[nodiscard]] T get() const {
        if constexpr (std::same_as<T, std::string>)
            return is_string() ? *value_->String() : std::string{};
        else if constexpr (std::same_as<T, bool>)
            return is_boolean() && *value_->Boolean();
        else if constexpr (std::integral<T>) {
            if (!is_number())
                return {};
            const auto& number = *value_->Numeric();
            if (number.kind == NumberKind::Signed)
                return static_cast<T>(std::get<i64>(number.value));
            if (number.kind == NumberKind::Unsigned)
                return static_cast<T>(std::get<u64>(number.value));
            return static_cast<T>(std::get<f64>(number.value));
        } else if constexpr (std::floating_point<T>) {
            if (!is_number())
                return {};
            const auto& number = *value_->Numeric();
            if (number.kind == NumberKind::Signed)
                return static_cast<T>(std::get<i64>(number.value));
            if (number.kind == NumberKind::Unsigned)
                return static_cast<T>(std::get<u64>(number.value));
            return static_cast<T>(std::get<f64>(number.value));
        } else if constexpr (requires(T result) { result.push_back(typename T::value_type{}); }) {
            T result;
            if (is_array())
                for (const auto& item : *value_->Elements())
                    result.push_back(Json(owner_, item.get()).template get<typename T::value_type>());
            return result;
        }
    }

    template <typename T>
    [[nodiscard]] const T& get_ref() const {
        static_assert(std::same_as<T, const std::string&>);
        return *value_->String();
    }

    template <typename T>
    [[nodiscard]] T value(std::string_view key, T fallback) const {
        Json child = (*this)[key];
        return child.value_ ? child.template get<T>() : std::move(fallback);
    }

    [[nodiscard]] std::string value(std::string_view key, const char* fallback) const {
        Json child = (*this)[key];
        return child.is_string() ? child.get<std::string>() : std::string(fallback);
    }

    class Iterator {
    public:
        using value_type = Json;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;

        Iterator() = default;

        Iterator(const Json* parent, size_t index)
            : parent_(parent),
              index_(index) {}

        const Json& operator*() const {
            Refresh();
            return *cache_;
        }

        const Json* operator->() const {
            Refresh();
            return cache_.get();
        }

        Iterator& operator++() {
            ++index_;
            return *this;
        }

        Iterator operator++(int) {
            Iterator copy = *this;
            ++*this;
            return copy;
        }

        bool operator==(const Iterator& other) const {
            return parent_ == other.parent_ && index_ == other.index_;
        }

    private:
        void Refresh() const {
            if (!parent_)
                return;
            if (parent_->is_array())
                cache_ = std::make_shared<Json>((*parent_)[index_]);
            else if (parent_->is_object() && index_ < parent_->size())
                cache_ = std::shared_ptr<Json>(new Json(parent_->owner_, (*parent_->value_->Members())[index_].value.get()));
        }

        const Json* parent_{};
        size_t index_{};
        mutable std::shared_ptr<Json> cache_;
    };

    [[nodiscard]] Iterator begin() const {
        return Iterator(this, 0);
    }

    [[nodiscard]] Iterator end() const {
        return Iterator(this, size());
    }

    [[nodiscard]] Iterator find(std::string_view key) const {
        if (!is_object())
            return end();
        const auto& members = *value_->Members();
        const auto found = std::ranges::find(members, key, &Member::key);
        return Iterator(this, static_cast<size_t>(found - members.begin()));
    }

    [[nodiscard]] std::vector<std::pair<std::string, Json>> items() const;

    template <typename T>
    friend bool operator==(const Json& json, const T& other) {
        if constexpr (std::same_as<T, const char*> || std::same_as<T, std::string> || std::same_as<T, std::string_view>)
            return json.is_string() && *json.value_->String() == other;
        else if constexpr (std::integral<T>)
            return json.is_number_integer() && json.template get<T>() == other;
        else
            return false;
    }

    template <typename T>
    friend bool operator!=(const Json& json, const T& other) {
        return !(json == other);
    }

private:
    Json(std::shared_ptr<const Document> owner, const Value* value)
        : owner_(std::move(owner)),
          value_(value) {}

    std::shared_ptr<const Document> owner_;
    const Value* value_{};
};

template <typename E>
struct EnumEntry {
    std::string_view name;
    E value;
};

template <typename E, size_t N>
[[nodiscard]] std::optional<E> Enum(std::string_view value, const std::array<EnumEntry<E>, N>& table) noexcept {
    for (const auto& entry : table)
        if (entry.name == value)
            return entry.value;
    return std::nullopt;
}

[[nodiscard]] bool RejectUnknown(const ObjectView& object, std::span<const std::string_view> allowed, std::vector<Diagnostic>& diagnostics, std::string_view code = "CFG2001");
[[nodiscard]] std::optional<std::string_view> RequiredString(const ObjectView& object, std::string_view key, std::vector<Diagnostic>& diagnostics, std::string_view code = "CFG2002");
[[nodiscard]] std::optional<u64> RequiredUnsigned(const ObjectView& object, std::string_view key, std::vector<Diagnostic>& diagnostics, std::string_view code = "CFG2002");
[[nodiscard]] std::optional<std::vector<std::string>> OptionalStrings(const ObjectView& object, std::string_view key, std::vector<Diagnostic>& diagnostics, std::string_view code = "CFG2003");

enum class SchemaType : u8 { Any, Null, Boolean, Integer, Number, String, Array, Object };
struct Schema;
using SchemaPtr = std::shared_ptr<Schema>;
using SemanticPredicate = std::function<void(const Value&, std::vector<Diagnostic>&)>;

struct Schema {
    SchemaType type{SchemaType::Any};
    std::map<std::string, SchemaPtr, std::less<>> properties;
    std::vector<std::string> required;
    bool additional_properties{true};
    SchemaPtr items;
    std::vector<SchemaPtr> tuple;
    std::optional<u64> min_items, max_items;
    bool unique_items{};
    std::optional<f64> minimum, maximum;
    std::optional<u64> min_length, max_length;
    std::vector<std::string> enum_values;
    std::optional<std::string> const_value;
    std::vector<SchemaPtr> one_of;
    std::optional<std::string> discriminator;
    SemanticPredicate predicate;
};

[[nodiscard]] std::vector<Diagnostic> Validate(const Document& document, const Schema& schema);

struct SchemaId {
    std::string family;
    u32 version{};
    friend bool operator==(const SchemaId&, const SchemaId&) = default;
};

using Migration = std::function<std::expected<Document, std::vector<Diagnostic>>(const Document&)>;

class Registry {
public:
    static Registry& Global();
    void Register(SchemaId id, Schema schema, std::vector<std::string> extensions = {}, bool current = true);
    void RegisterMigration(std::string family, u32 from, u32 to, Migration migration);
    [[nodiscard]] const Schema* Find(std::string_view family, u32 version) const noexcept;
    [[nodiscard]] std::optional<u32> Current(std::string_view family) const noexcept;
    // True only when the version is current or has a registered migration path
    // to current. CI and tools use this as the maintained-version window.
    [[nodiscard]] bool Maintained(std::string_view family, u32 version) const noexcept;
    [[nodiscard]] std::expected<Document, std::vector<Diagnostic>> Migrate(const Document& document, std::string_view family, u32 from, u32 to) const;
    [[nodiscard]] std::string JsonSchema(std::string_view family, u32 version) const;
    [[nodiscard]] std::string Catalog() const;

private:
    struct Entry {
        SchemaId id;
        Schema schema;
        std::vector<std::string> extensions;
        bool current{};
    };

    struct Edge {
        std::string family;
        u32 from{}, to{};
        Migration migration;
    };

    std::vector<Entry> entries_;
    std::vector<Edge> edges_;
};

void RegisterBuiltinSchemas(Registry& registry);

class Writer {
public:
    explicit Writer(bool pretty = true)
        : pretty_(pretty) {}

    void Null();
    void Boolean(bool value);
    void Integer(i64 value);
    void Unsigned(u64 value);
    void Real(f64 value);
    void String(std::string_view value);
    void BeginObject();
    void Key(std::string_view key);
    void EndObject();
    void BeginArray();
    void EndArray();

    [[nodiscard]] const std::string& Str() const noexcept {
        return output_;
    }

private:
    void Prefix(bool key = false);
    void Indent();

    struct Frame {
        char type;
        bool first{true};
        bool after_key{};
    };

    bool pretty_{};
    std::string output_;
    std::vector<Frame> stack_;
};

[[nodiscard]] std::string CanonicalJson(const Value& value);
[[nodiscard]] ContentHash CanonicalHash(const Document& document, std::string_view family, u32 version);
[[nodiscard]] std::string FormatDiagnostics(std::span<const Diagnostic> diagnostics);

} // namespace woki::config
