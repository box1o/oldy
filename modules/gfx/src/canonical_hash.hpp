#pragma once

#include <bit>
#include <string_view>
#include <type_traits>
#include <vector>
#include <optional>

#include <woki/core.hpp>
#include <woki/asset/id.hpp>

namespace woki::gfx::detail {

template <typename T, bool = std::is_enum_v<T>>
struct HashIntegerType {
    using type = T;
};

template <typename T>
struct HashIntegerType<T, true> {
    using type = std::underlying_type_t<T>;
};

class CanonicalHashWriter final {
public:
    template <typename T>
    requires(std::is_integral_v<T> || std::is_enum_v<T>)
    void Value(T value) {
        using Raw = typename HashIntegerType<T>::type;
        using U = std::make_unsigned_t<Raw>;
        bytes_.push_back(std::byte{0x01});
        const U bits = static_cast<U>(value);
        for (size_t index = 0; index < sizeof(U); ++index)
            bytes_.push_back(static_cast<std::byte>((bits >> (index * 8U)) & U{0xff}));
    }

    void Value(bool value) {
        bytes_.push_back(std::byte{0x02});
        bytes_.push_back(value ? std::byte{1} : std::byte{0});
    }

    void Value(std::string_view value) {
        bytes_.push_back(std::byte{0x03});
        Value(static_cast<u64>(value.size()));
        const auto encoded = std::as_bytes(std::span(value.data(), value.size()));
        bytes_.insert(bytes_.end(), encoded.begin(), encoded.end());
    }

    void Value(const ContentHash& value) {
        bytes_.push_back(std::byte{0x04});
        for (const u8 byte : value.Bytes())
            bytes_.push_back(static_cast<std::byte>(byte));
    }

    void Value(const asset::AssetId& value) {
        bytes_.push_back(std::byte{0x05});
        for (const u8 byte : value.Bytes())
            bytes_.push_back(static_cast<std::byte>(byte));
    }

    template <typename T>
    void Int(T value) {
        Value(value);
    }

    void String(std::string_view value) {
        Value(value);
    }

    void Hash(const ContentHash& value) {
        Value(value);
    }

    void Hash(const asset::AssetId& value) {
        Value(value);
    }

    void Optional(const std::optional<std::string>& value) {
        Value(value.has_value());
        if (value)
            Value(*value);
    }

    [[nodiscard]] ContentHash Finish() const noexcept {
        return Sha256(bytes_);
    }

private:
    std::vector<std::byte> bytes_;
};

inline u64 Hash64(const ContentHash& hash) noexcept {
    u64 value{};
    for (u32 index = 0; index < sizeof(value); ++index)
        value |= static_cast<u64>(hash.Bytes()[index]) << (index * 8U);
    return value;
}

} // namespace woki::gfx::detail
