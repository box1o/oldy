#include <woki/asset/product.hpp>

#include <algorithm>
#include <array>
#include <limits>

namespace woki::asset {
namespace {

constexpr std::array<std::byte, 4> kMagic{std::byte{'W'}, std::byte{'K'}, std::byte{'A'}, std::byte{'S'}};
constexpr std::size_t kFixedSize = 4 + 2 + 2 + 4 + 4 + ContentHash::kSize + ContentHash::kSize + 4 + 8;

template <typename T>
void WriteLittle(std::vector<std::byte>& output, T value) {
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        output.push_back(static_cast<std::byte>(value & 0xffU));
        value >>= 8U;
    }
}

void WriteHash(std::vector<std::byte>& output, const ContentHash& hash) {
    for (const u8 byte : hash.Bytes()) {
        output.push_back(static_cast<std::byte>(byte));
    }
}

class Reader {
public:
    explicit Reader(const std::span<const std::byte> bytes)
        : bytes_(bytes) {}

    template <typename T>
    [[nodiscard]] bool ReadLittle(T& value) {
        if (Remaining() < sizeof(T)) {
            return false;
        }
        u64 bits = 0;
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            bits |= static_cast<u64>(std::to_integer<u8>(bytes_[position_ + i])) << (i * 8U);
        }
        value = static_cast<T>(bits);
        position_ += sizeof(T);
        return true;
    }

    [[nodiscard]] bool ReadHash(ContentHash& hash) {
        if (Remaining() < ContentHash::kSize) {
            return false;
        }
        std::array<u8, ContentHash::kSize> value{};
        for (std::size_t i = 0; i < value.size(); ++i) {
            value[i] = std::to_integer<u8>(bytes_[position_ + i]);
        }
        position_ += value.size();
        hash = ContentHash(value);
        return true;
    }

    [[nodiscard]] std::span<const std::byte> Take(const std::size_t size) {
        if (Remaining() < size) {
            return {};
        }
        const auto result = bytes_.subspan(position_, size);
        position_ += size;
        return result;
    }

    [[nodiscard]] std::size_t Remaining() const noexcept {
        return bytes_.size() - position_;
    }

private:
    std::span<const std::byte> bytes_;
    std::size_t position_{0};
};

} // namespace

ContentHash HashProduct(const Product& product) {
    std::vector<std::byte> bytes;
    WriteLittle(bytes, product.type);
    WriteLittle(bytes, product.schema_version);
    WriteHash(bytes, product.source_hash);
    WriteLittle(bytes, static_cast<u64>(product.dependency_hashes.size()));
    for (const ContentHash& dependency : product.dependency_hashes)
        WriteHash(bytes, dependency);
    WriteLittle(bytes, static_cast<u64>(product.payload.size()));
    bytes.insert(bytes.end(), product.payload.begin(), product.payload.end());
    return Sha256(bytes);
}

Product MakeProduct(const u32 type, const u32 schema_version, ContentHash source_hash, std::vector<ContentHash> dependency_hashes, std::vector<std::byte> payload) {
    Product product{type, schema_version, std::move(source_hash), std::move(dependency_hashes), {}, std::move(payload)};
    product.product_hash = HashProduct(product);
    return product;
}

Result<void> ValidateProduct(const Product& product, const ProductLimits limits) {
    if (product.payload.size() > limits.max_payload_bytes)
        return Err(ErrorCode::OutOfRange, "product payload exceeds the configured limit");
    if (product.dependency_hashes.size() > limits.max_dependencies || product.dependency_hashes.size() > std::numeric_limits<u32>::max())
        return Err(ErrorCode::OutOfRange, "product dependency count exceeds the configured limit");
    if (product.product_hash != HashProduct(product))
        return Err(ErrorCode::ValidationInvalidState, "product hash does not match its metadata and payload");
    return Ok();
}

Result<std::vector<std::byte>> SerializeProduct(const Product& product, const ProductLimits limits) {
    TRY_VOID(ValidateProduct(product, limits));

    const std::size_t maximum_size = std::vector<std::byte>{}.max_size();
    if (product.payload.size() > maximum_size - kFixedSize || product.dependency_hashes.size() > (maximum_size - kFixedSize - product.payload.size()) / ContentHash::kSize) {
        return Err(ErrorCode::OutOfRange, "serialized product size exceeds addressable memory");
    }

    std::vector<std::byte> output;
    output.reserve(kFixedSize + product.dependency_hashes.size() * ContentHash::kSize + product.payload.size());
    output.insert(output.end(), kMagic.begin(), kMagic.end());
    WriteLittle(output, kProductFormatVersion);
    WriteLittle(output, u16{0});
    WriteLittle(output, product.type);
    WriteLittle(output, product.schema_version);
    WriteHash(output, product.source_hash);
    WriteHash(output, product.product_hash);
    WriteLittle(output, static_cast<u32>(product.dependency_hashes.size()));
    WriteLittle(output, static_cast<u64>(product.payload.size()));
    for (const ContentHash& dependency : product.dependency_hashes) {
        WriteHash(output, dependency);
    }
    output.insert(output.end(), product.payload.begin(), product.payload.end());
    return Ok(std::move(output));
}

Result<Product> ParseProduct(const std::span<const std::byte> bytes, const ProductLimits limits) {
    if (bytes.size() < kFixedSize || !std::equal(kMagic.begin(), kMagic.end(), bytes.begin())) {
        return Err(ErrorCode::ParseInvalidFormat, "product has invalid WKAS magic or is truncated");
    }

    Reader reader(bytes.subspan(kMagic.size()));
    u16 format_version = 0;
    u16 reserved = 0;
    Product product;
    u32 dependency_count = 0;
    u64 payload_size = 0;
    if (!reader.ReadLittle(format_version) || !reader.ReadLittle(reserved) || !reader.ReadLittle(product.type) || !reader.ReadLittle(product.schema_version) || !reader.ReadHash(product.source_hash)
        || !reader.ReadHash(product.product_hash) || !reader.ReadLittle(dependency_count) || !reader.ReadLittle(payload_size)) {
        return Err(ErrorCode::ParseInvalidFormat, "product header is truncated");
    }
    if (format_version != kProductFormatVersion || reserved != 0) {
        return Err(ErrorCode::ParseInvalidFormat, "product format version or reserved fields are invalid");
    }
    if (dependency_count > limits.max_dependencies || payload_size > limits.max_payload_bytes || payload_size > std::numeric_limits<std::size_t>::max()) {
        return Err(ErrorCode::OutOfRange, "product exceeds configured parsing limits");
    }
    if (dependency_count > reader.Remaining() / ContentHash::kSize) {
        return Err(ErrorCode::ParseInvalidFormat, "product dependency count exceeds its contents");
    }
    const std::size_t dependencies_size = static_cast<std::size_t>(dependency_count) * ContentHash::kSize;
    if (payload_size != reader.Remaining() - dependencies_size) {
        return Err(ErrorCode::ParseInvalidFormat, "product size fields do not match its contents");
    }

    product.dependency_hashes.resize(dependency_count);
    for (ContentHash& dependency : product.dependency_hashes) {
        if (!reader.ReadHash(dependency)) {
            return Err(ErrorCode::ParseInvalidFormat, "product dependencies are truncated");
        }
    }
    const std::span<const std::byte> payload = reader.Take(static_cast<std::size_t>(payload_size));
    product.payload.assign(payload.begin(), payload.end());
    if (product.product_hash != HashProduct(product)) {
        return Err(ErrorCode::ParseInvalidFormat, "product metadata or payload hash is invalid");
    }
    return Ok(std::move(product));
}

} // namespace woki::asset
