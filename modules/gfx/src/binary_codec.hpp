#pragma once

#include <bit>
#include <limits>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

#include <woki/asset.hpp>
#include <woki/core.hpp>
#include <woki/math.hpp>

namespace woki::gfx::detail {

template <typename T, bool = std::is_enum_v<T>>
struct BinaryIntegerType {
    using type = T;
};

template <typename T>
struct BinaryIntegerType<T, true> {
    using type = std::underlying_type_t<T>;
};

class BinaryWriter {
public:
    template <typename T>
    requires(std::is_integral_v<T> || std::is_enum_v<T>)
    void Int(T value) {
        using U = std::make_unsigned_t<typename BinaryIntegerType<T>::type>;
        const U bits = static_cast<U>(value);
        for (size_t index = 0; index < sizeof(U); ++index)
            bytes.push_back(static_cast<std::byte>((bits >> (index * 8U)) & U{0xff}));
    }

    void Float(f32 value) {
        Int(std::bit_cast<u32>(value));
    }

    void String(std::string_view value) {
        Int(static_cast<u32>(value.size()));
        const auto encoded = std::as_bytes(std::span(value.data(), value.size()));
        bytes.insert(bytes.end(), encoded.begin(), encoded.end());
    }

    void Hash(const ContentHash& value) {
        for (const u8 byte : value.Bytes())
            bytes.push_back(static_cast<std::byte>(byte));
    }

    void Id(const asset::AssetId& value) {
        for (const u8 byte : value.Bytes())
            bytes.push_back(static_cast<std::byte>(byte));
    }

    void Raw(std::span<const std::byte> value) {
        Int(static_cast<u32>(value.size()));
        bytes.insert(bytes.end(), value.begin(), value.end());
    }

    void Matrix(const math::mat4f& value) {
        for (size_t index = 0; index < 16; ++index)
            Float(value.data()[index]);
    }

    std::vector<std::byte> bytes;
};

class BinaryReader {
public:
    explicit BinaryReader(std::span<const std::byte> bytes)
        : bytes_(bytes) {}

    template <typename T>
    requires(std::is_integral_v<T> || std::is_enum_v<T>)
    bool Int(T& output) {
        if (offset_ > bytes_.size() || bytes_.size() - offset_ < sizeof(T))
            return false;
        u64 bits{};
        for (size_t index = 0; index < sizeof(T); ++index)
            bits |= static_cast<u64>(std::to_integer<u8>(bytes_[offset_ + index])) << (index * 8U);
        output = static_cast<T>(bits);
        offset_ += sizeof(T);
        return true;
    }

    bool Float(f32& output) {
        u32 bits{};
        if (!Int(bits))
            return false;
        output = std::bit_cast<f32>(bits);
        return std::isfinite(output);
    }

    bool String(std::string& output, size_t maximum = std::numeric_limits<u32>::max(), size_t* decoded_size = nullptr) {
        u32 size{};
        if (!Int(size) || size > maximum || offset_ > bytes_.size() || bytes_.size() - offset_ < size)
            return false;
        output.assign(reinterpret_cast<const char*>(bytes_.data() + offset_), size);
        offset_ += size;
        if (decoded_size != nullptr)
            *decoded_size = size;
        return true;
    }

    bool Hash(ContentHash& output) {
        if (offset_ > bytes_.size() || bytes_.size() - offset_ < ContentHash::kSize)
            return false;
        std::array<u8, ContentHash::kSize> value{};
        for (u8& byte : value)
            byte = std::to_integer<u8>(bytes_[offset_++]);
        output = ContentHash(value);
        return true;
    }

    bool Id(asset::AssetId& output) {
        if (offset_ > bytes_.size() || bytes_.size() - offset_ < asset::AssetId::kSize)
            return false;
        std::array<u8, asset::AssetId::kSize> bytes{};
        for (u8& byte : bytes)
            byte = std::to_integer<u8>(bytes_[offset_++]);
        output = asset::AssetId(bytes);
        return true;
    }

    bool Raw(std::vector<std::byte>& output, size_t maximum = std::numeric_limits<u32>::max()) {
        u32 size{};
        if (!Int(size) || size > maximum || offset_ > bytes_.size() || bytes_.size() - offset_ < size)
            return false;
        output.assign(bytes_.begin() + static_cast<ptrdiff_t>(offset_), bytes_.begin() + static_cast<ptrdiff_t>(offset_ + size));
        offset_ += size;
        return true;
    }

    bool Matrix(math::mat4f& output) {
        for (size_t index = 0; index < 16; ++index)
            if (!Float(output.data()[index]))
                return false;
        return true;
    }

    [[nodiscard]] size_t Offset() const noexcept {
        return offset_;
    }

    [[nodiscard]] bool End() const noexcept {
        return offset_ == bytes_.size();
    }

protected:
    std::span<const std::byte> bytes_;
    size_t offset_{};
};

} // namespace woki::gfx::detail
