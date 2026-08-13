#include <algorithm>
#include <random>

#include <woki/asset/id.hpp>

namespace woki::asset {
namespace {

constexpr int HexDigit(const char value) noexcept {
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    return -1;
}

AssetId UuidFromHash(const ContentHash& hash) noexcept {
    std::array<u8, AssetId::kSize> bytes{};
    std::ranges::copy_n(hash.Bytes().begin(), bytes.size(), bytes.begin());
    bytes[6] = static_cast<u8>((bytes[6] & 0x0fU) | 0x50U);
    bytes[8] = static_cast<u8>((bytes[8] & 0x3fU) | 0x80U);
    return AssetId(bytes);
}

} // namespace

Result<AssetId> AssetId::Parse(const std::string_view uuid) {
    if (uuid.size() != 36 || uuid[8] != '-' || uuid[13] != '-' || uuid[18] != '-' || uuid[23] != '-')
        return Err(ErrorCode::ParseInvalidFormat, "asset ID must be a canonical lowercase UUID");
    std::array<u8, kSize> bytes{};
    std::size_t input = 0;
    for (u8& byte : bytes) {
        if (input == 8 || input == 13 || input == 18 || input == 23)
            ++input;
        const int high = HexDigit(uuid[input++]);
        const int low = HexDigit(uuid[input++]);
        if (high < 0 || low < 0)
            return Err(ErrorCode::ParseInvalidFormat, "asset ID must be a canonical lowercase UUID");
        byte = static_cast<u8>((high << 4) | low);
    }
    AssetId result(bytes);
    if (!result.IsValid())
        return Err(ErrorCode::ParseInvalidFormat, "nil UUID is not an asset ID");
    return Ok(result);
}

Result<AssetId> AssetId::New() {
#ifdef __EMSCRIPTEN__
    return Err(ErrorCode::InvalidState, "random asset IDs must be supplied by the web host");
#else
    std::random_device random;
    std::array<u8, kSize> bytes{};
    for (u8& byte : bytes)
        byte = static_cast<u8>(random());
    bytes[6] = static_cast<u8>((bytes[6] & 0x0fU) | 0x40U);
    bytes[8] = static_cast<u8>((bytes[8] & 0x3fU) | 0x80U);
    return Ok(AssetId(bytes));
#endif
}

AssetId AssetId::FromName(const std::string_view name) noexcept {
    return UuidFromHash(Sha256(name));
}

AssetId AssetId::FromBytes(const std::span<const std::byte> bytes) noexcept {
    return UuidFromHash(Sha256(bytes));
}

std::string AssetId::String() const {
    constexpr char digits[] = "0123456789abcdef";
    std::string output;
    output.reserve(36);
    for (std::size_t i = 0; i < bytes_.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            output.push_back('-');
        output.push_back(digits[bytes_[i] >> 4U]);
        output.push_back(digits[bytes_[i] & 0x0fU]);
    }
    return output;
}

bool AssetId::IsValid() const noexcept {
    return std::ranges::any_of(bytes_, [](u8 byte) { return byte != 0; });
}

} // namespace woki::asset

std::size_t std::hash<woki::asset::AssetId>::operator()(const woki::asset::AssetId& value) const noexcept {
    const std::size_t prime = sizeof(std::size_t) == 8 ? static_cast<std::size_t>(1099511628211ULL) : static_cast<std::size_t>(16777619U);
    std::size_t result = sizeof(std::size_t) == 8 ? static_cast<std::size_t>(1469598103934665603ULL) : static_cast<std::size_t>(2166136261U);
    for (const woki::u8 byte : value.Bytes())
        result = (result ^ byte) * prime;
    return result;
}

std::size_t std::hash<woki::asset::AssetKey>::operator()(const woki::asset::AssetKey& value) const noexcept {
    return std::hash<woki::asset::AssetId>{}(value.asset) ^ (static_cast<std::size_t>(value.subresource.value) << 1U);
}
