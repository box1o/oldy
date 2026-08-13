#include <algorithm>
#include <array>
#include <limits>

#include <woki/asset/cache.hpp>
#include <woki/asset/manifest.hpp>
#include <woki/asset/product.hpp>
#include <woki/asset/vfs.hpp>

namespace woki::asset {
namespace {

constexpr std::array<std::byte, 4> kMagic{std::byte{'W'}, std::byte{'K'}, std::byte{'A'}, std::byte{'S'}};
constexpr std::size_t kHeaderSize = 164;
constexpr std::size_t kDependencySize = AssetId::kSize + ContentHash::kSize;
constexpr std::size_t kChunkSize = 36 + ContentHash::kSize;

template <typename T>
void Write(std::vector<std::byte>& out, T value) {
    u64 bits = static_cast<u64>(value);
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        out.push_back(static_cast<std::byte>(bits & 0xffU));
        bits >>= 8U;
    }
}

void WriteId(std::vector<std::byte>& out, const AssetId& id) {
    for (u8 byte : id.Bytes())
        out.push_back(static_cast<std::byte>(byte));
}

void WriteHash(std::vector<std::byte>& out, const ContentHash& hash) {
    for (u8 byte : hash.Bytes())
        out.push_back(static_cast<std::byte>(byte));
}

class Reader {
public:
    explicit Reader(std::span<const std::byte> bytes)
        : bytes_(bytes) {}

    template <typename T>
    bool Int(T& value) {
        if (Remaining() < sizeof(T))
            return false;
        u64 bits{};
        for (std::size_t i = 0; i < sizeof(T); ++i)
            bits |= static_cast<u64>(std::to_integer<u8>(bytes_[position_ + i])) << (i * 8U);
        value = static_cast<T>(bits);
        position_ += sizeof(T);
        return true;
    }

    bool Id(AssetId& id) {
        if (Remaining() < AssetId::kSize)
            return false;
        std::array<u8, AssetId::kSize> bytes{};
        for (u8& byte : bytes)
            byte = std::to_integer<u8>(bytes_[position_++]);
        id = AssetId(bytes);
        return true;
    }

    bool Hash(ContentHash& hash) {
        if (Remaining() < ContentHash::kSize)
            return false;
        std::array<u8, ContentHash::kSize> bytes{};
        for (u8& byte : bytes)
            byte = std::to_integer<u8>(bytes_[position_++]);
        hash = ContentHash(bytes);
        return true;
    }

    std::span<const std::byte> Bytes(std::size_t size) {
        if (Remaining() < size)
            return {};
        auto result = bytes_.subspan(position_, size);
        position_ += size;
        return result;
    }

    std::size_t Remaining() const {
        return bytes_.size() - position_;
    }

private:
    std::span<const std::byte> bytes_;
    std::size_t position_{};
};

void WriteCanonical(std::vector<std::byte>& out, const Product& product) {
    WriteId(out, product.asset_id);
    Write(out, product.subresource.value);
    Write(out, product.type);
    Write(out, product.schema_version);
    Write(out, product.builder_version);
    Write(out, product.container_version);
    WriteHash(out, product.source_hash);
    WriteHash(out, product.target_fingerprint);
    Write(out, static_cast<u32>(product.dependencies.size()));
    for (const auto& dependency : product.dependencies) {
        WriteId(out, dependency.asset_id);
        WriteHash(out, dependency.product_hash);
    }
    Write(out, static_cast<u32>(product.chunks.size()));
    for (const auto& chunk : product.chunks) {
        Write(out, static_cast<u32>(chunk.semantic));
        Write(out, static_cast<u8>(chunk.compression));
        Write(out, u8{});
        Write(out, u16{});
        Write(out, chunk.alignment);
        Write(out, chunk.offset);
        Write(out, chunk.compressed_size);
        Write(out, chunk.uncompressed_size);
        WriteHash(out, chunk.checksum);
    }
    Write(out, static_cast<u64>(product.payload.size()));
    out.insert(out.end(), product.payload.begin(), product.payload.end());
}

} // namespace

ContentHash HashProduct(const Product& product) {
    std::vector<std::byte> bytes;
    WriteCanonical(bytes, product);
    return Sha256(bytes);
}

Product MakeProduct(AssetId asset_id,
    const u32 type,
    const u32 schema_version,
    const u32 builder_version,
    ContentHash source_hash,
    std::vector<ProductDependency> dependencies,
    ContentHash target_fingerprint,
    std::vector<std::byte> payload,
    const SubresourceId subresource) {
    Product product{std::move(asset_id), subresource, type, schema_version, builder_version, kProductFormatVersion, std::move(source_hash), std::move(dependencies), std::move(target_fingerprint), {}, {},
        std::move(payload)};
    if (!product.payload.empty())
        product.chunks.push_back({ProductChunkSemantic::Generic, ProductCompression::None, 1, 0, product.payload.size(), product.payload.size(), Sha256(product.payload)});
    product.product_hash = HashProduct(product);
    return product;
}

Result<void> ValidateProduct(const Product& product, const ProductLimits limits) {
    if (!product.asset_id)
        return Err(ErrorCode::ValidationNullValue, "product has no asset ID");
    if (product.container_version != kProductFormatVersion)
        return Err(ErrorCode::ParseInvalidFormat, "product container version is unsupported");
    if (product.payload.size() > limits.max_payload_bytes || product.dependencies.size() > limits.max_dependencies || product.chunks.size() > limits.max_chunks)
        return Err(ErrorCode::OutOfRange, "product exceeds configured limits");
    if (!std::ranges::is_sorted(product.dependencies) || std::ranges::adjacent_find(product.dependencies, {}, &ProductDependency::asset_id) != product.dependencies.end())
        return Err(ErrorCode::ValidationInvalidState, "product dependencies must be ordered and unique by asset ID");
    u64 previous_end{};
    for (const auto& chunk : product.chunks) {
        if (chunk.compression != ProductCompression::None)
            return Err(ErrorCode::GraphicsUnsupportedApi, "product chunk compression method is not implemented");
        if (chunk.alignment == 0 || (chunk.alignment & (chunk.alignment - 1U)) != 0 || chunk.offset % chunk.alignment != 0)
            return Err(ErrorCode::ValidationInvalidState, "product chunk alignment is invalid");
        if (chunk.compressed_size != chunk.uncompressed_size || chunk.uncompressed_size > limits.max_decompressed_chunk_bytes)
            return Err(ErrorCode::OutOfRange, "product chunk decompression size is invalid");
        if (chunk.offset < previous_end || chunk.offset > product.payload.size() || chunk.compressed_size > product.payload.size() - chunk.offset)
            return Err(ErrorCode::ValidationInvalidState, "product chunk table is invalid");
        const auto bytes = std::span(product.payload).subspan(static_cast<std::size_t>(chunk.offset), static_cast<std::size_t>(chunk.compressed_size));
        if (chunk.checksum != Sha256(bytes))
            return Err(ErrorCode::ValidationInvalidState, "product chunk checksum is invalid");
        previous_end = chunk.offset + chunk.compressed_size;
    }
    if (product.payload.empty() != product.chunks.empty())
        return Err(ErrorCode::ValidationInvalidState, "empty product payload and chunk table disagree");
    if (product.product_hash != HashProduct(product))
        return Err(ErrorCode::ValidationInvalidState, "product hash is invalid");
    return Ok();
}

Result<std::vector<std::byte>> SerializeProduct(const Product& product, const ProductLimits limits) {
    TRY_VOID(ValidateProduct(product, limits));
    const u64 payload_offset = kHeaderSize + product.dependencies.size() * kDependencySize + product.chunks.size() * kChunkSize;
    if (payload_offset > std::numeric_limits<std::size_t>::max() - product.payload.size())
        return Err(ErrorCode::OutOfRange, "serialized product is too large");
    std::vector<std::byte> out;
    out.reserve(static_cast<std::size_t>(payload_offset) + product.payload.size());
    out.insert(out.end(), kMagic.begin(), kMagic.end());
    Write(out, kProductFormatVersion);
    Write(out, u16{});
    WriteId(out, product.asset_id);
    Write(out, product.subresource.value);
    Write(out, product.type);
    Write(out, product.schema_version);
    Write(out, product.builder_version);
    Write(out, product.container_version);
    WriteHash(out, product.source_hash);
    WriteHash(out, product.target_fingerprint);
    WriteHash(out, product.product_hash);
    Write(out, static_cast<u32>(product.dependencies.size()));
    Write(out, static_cast<u32>(product.chunks.size()));
    Write(out, payload_offset);
    Write(out, static_cast<u64>(product.payload.size()));
    for (const auto& dependency : product.dependencies) {
        WriteId(out, dependency.asset_id);
        WriteHash(out, dependency.product_hash);
    }
    for (const auto& chunk : product.chunks) {
        Write(out, static_cast<u32>(chunk.semantic));
        Write(out, static_cast<u8>(chunk.compression));
        Write(out, u8{});
        Write(out, u16{});
        Write(out, chunk.alignment);
        Write(out, chunk.offset);
        Write(out, chunk.compressed_size);
        Write(out, chunk.uncompressed_size);
        WriteHash(out, chunk.checksum);
    }
    out.insert(out.end(), product.payload.begin(), product.payload.end());
    return Ok(std::move(out));
}

Result<Product> ParseProduct(const std::span<const std::byte> bytes, const ProductLimits limits) {
    if (bytes.size() < kHeaderSize || !std::equal(kMagic.begin(), kMagic.end(), bytes.begin()))
        return Err(ErrorCode::ParseInvalidFormat, "product magic is invalid or header is truncated");
    Reader in(bytes.subspan(4));
    Product product;
    u16 format{}, flags{};
    u32 dependency_count{}, chunk_count{};
    u64 payload_offset{}, payload_size{};
    if (!in.Int(format) || !in.Int(flags) || !in.Id(product.asset_id) || !in.Int(product.subresource.value) || !in.Int(product.type) || !in.Int(product.schema_version) || !in.Int(product.builder_version)
        || !in.Int(product.container_version) || !in.Hash(product.source_hash) || !in.Hash(product.target_fingerprint) || !in.Hash(product.product_hash) || !in.Int(dependency_count) || !in.Int(chunk_count)
        || !in.Int(payload_offset) || !in.Int(payload_size))
        return Err(ErrorCode::ParseInvalidFormat, "product header is truncated");
    if (format != kProductFormatVersion || flags != 0 || product.container_version != kProductFormatVersion)
        return Err(ErrorCode::ParseInvalidFormat, "product format version is unsupported");
    if (dependency_count > limits.max_dependencies || chunk_count > limits.max_chunks || payload_size > limits.max_payload_bytes)
        return Err(ErrorCode::OutOfRange, "product exceeds configured parsing limits");
    const u64 expected_offset = kHeaderSize + static_cast<u64>(dependency_count) * kDependencySize + static_cast<u64>(chunk_count) * kChunkSize;
    if (payload_offset != expected_offset || payload_size != bytes.size() - std::min<u64>(payload_offset, bytes.size()) || payload_offset > bytes.size())
        return Err(ErrorCode::ParseInvalidFormat, "product offsets, sizes, or trailing bytes are invalid");
    product.dependencies.resize(dependency_count);
    for (auto& dependency : product.dependencies)
        if (!in.Id(dependency.asset_id) || !in.Hash(dependency.product_hash))
            return Err(ErrorCode::ParseInvalidFormat, "product dependency table is truncated");
    product.chunks.resize(chunk_count);
    for (auto& chunk : product.chunks) {
        u32 semantic{};
        u8 compression{}, reserved8{};
        u16 reserved16{};
        if (!in.Int(semantic) || !in.Int(compression) || !in.Int(reserved8) || !in.Int(reserved16) || !in.Int(chunk.alignment) || !in.Int(chunk.offset) || !in.Int(chunk.compressed_size)
            || !in.Int(chunk.uncompressed_size) || !in.Hash(chunk.checksum) || reserved8 != 0 || reserved16 != 0 || compression > static_cast<u8>(ProductCompression::Zstd))
            return Err(ErrorCode::ParseInvalidFormat, "product chunk table is truncated");
        chunk.semantic = static_cast<ProductChunkSemantic>(semantic);
        chunk.compression = static_cast<ProductCompression>(compression);
    }
    const auto payload = in.Bytes(static_cast<std::size_t>(payload_size));
    product.payload.assign(payload.begin(), payload.end());
    auto valid = ValidateProduct(product, limits);
    if (!valid)
        return Err(ErrorCode::ParseInvalidFormat, valid.error().Message());
    return Ok(std::move(product));
}

bool ProductRequiresRebuild(const u16 container_version) noexcept {
    return container_version < kOldestReadableProductFormatVersion || container_version > kProductFormatVersion;
}

Result<ProductReader> ProductReader::Open(const u64 size, const ProductLimits limits, RangeRead read) {
    if (size < kHeaderSize)
        return Err(ErrorCode::ParseInvalidFormat, "product header is truncated");
    auto fixed = read(0, kHeaderSize);
    if (!fixed || fixed->size() != kHeaderSize)
        return fixed ? Result<ProductReader>(Err(ErrorCode::ParseInvalidFormat, "product header range is truncated")) : Result<ProductReader>(Err(std::move(fixed).error()));
    if (!std::equal(kMagic.begin(), kMagic.end(), fixed->begin()))
        return Err(ErrorCode::ParseInvalidFormat, "product magic is invalid");
    Reader prefix(std::span(*fixed).subspan(4));
    ProductHeader header;
    u16 format{}, flags{};
    u32 dependency_count{}, chunk_count{};
    if (!prefix.Int(format) || !prefix.Int(flags) || !prefix.Id(header.asset_id) || !prefix.Int(header.subresource.value) || !prefix.Int(header.type) || !prefix.Int(header.schema_version)
        || !prefix.Int(header.builder_version) || !prefix.Int(header.container_version) || !prefix.Hash(header.source_hash) || !prefix.Hash(header.target_fingerprint) || !prefix.Hash(header.product_hash)
        || !prefix.Int(dependency_count) || !prefix.Int(chunk_count) || !prefix.Int(header.payload_offset) || !prefix.Int(header.payload_size))
        return Err(ErrorCode::ParseInvalidFormat, "product header is truncated");
    if (format != kProductFormatVersion || flags != 0 || header.container_version != kProductFormatVersion || dependency_count > limits.max_dependencies || chunk_count > limits.max_chunks
        || header.payload_size > limits.max_payload_bytes)
        return Err(ErrorCode::ParseInvalidFormat, "product version, flags, or limits are invalid");
    const u64 table_size = static_cast<u64>(dependency_count) * kDependencySize + static_cast<u64>(chunk_count) * kChunkSize;
    if (table_size > std::numeric_limits<u64>::max() - kHeaderSize || header.payload_offset != kHeaderSize + table_size || header.payload_offset > size || header.payload_size != size - header.payload_offset)
        return Err(ErrorCode::ParseInvalidFormat, "product table or payload range is invalid");
    auto tables = read(kHeaderSize, static_cast<std::size_t>(table_size));
    if (!tables || tables->size() != table_size)
        return tables ? Result<ProductReader>(Err(ErrorCode::ParseInvalidFormat, "product tables are truncated")) : Result<ProductReader>(Err(std::move(tables).error()));
    Reader in(*tables);
    header.dependencies.resize(dependency_count);
    for (auto& dependency : header.dependencies)
        if (!in.Id(dependency.asset_id) || !in.Hash(dependency.product_hash))
            return Err(ErrorCode::ParseInvalidFormat, "product dependency table is truncated");
    header.chunks.resize(chunk_count);
    u64 previous_end{};
    for (auto& chunk : header.chunks) {
        u32 semantic{};
        u8 compression{}, reserved8{};
        u16 reserved16{};
        if (!in.Int(semantic) || !in.Int(compression) || !in.Int(reserved8) || !in.Int(reserved16) || !in.Int(chunk.alignment) || !in.Int(chunk.offset) || !in.Int(chunk.compressed_size)
            || !in.Int(chunk.uncompressed_size) || !in.Hash(chunk.checksum) || reserved8 || reserved16 || compression != static_cast<u8>(ProductCompression::None))
            return Err(ErrorCode::ParseInvalidFormat, "product chunk metadata is invalid or unsupported");
        chunk.semantic = static_cast<ProductChunkSemantic>(semantic);
        chunk.compression = static_cast<ProductCompression>(compression);
        if (chunk.alignment == 0 || (chunk.alignment & (chunk.alignment - 1U)) != 0 || chunk.offset % chunk.alignment != 0 || chunk.offset < previous_end || chunk.offset > header.payload_size
            || chunk.compressed_size > header.payload_size - chunk.offset || chunk.compressed_size != chunk.uncompressed_size || chunk.uncompressed_size > limits.max_decompressed_chunk_bytes)
            return Err(ErrorCode::ParseInvalidFormat, "product chunk range, overlap, alignment, or decompression limit is invalid");
        previous_end = chunk.offset + chunk.compressed_size;
    }
    if (header.payload_size == 0 ? !header.chunks.empty() : header.chunks.empty())
        return Err(ErrorCode::ParseInvalidFormat, "product payload and chunk table disagree");
    header.file_size = size;
    return Ok(ProductReader(std::move(header), limits, std::move(read)));
}

Result<ProductReader> ProductReader::Open(const Vfs& vfs, const AssetUri& uri, const ProductLimits limits) {
    auto stat = vfs.Stat(uri);
    if (!stat || stat->type != VfsEntryType::File)
        return stat ? Result<ProductReader>(Err(ErrorCode::FileReadError, "product URI is not a file")) : Result<ProductReader>(Err(std::move(stat).error()));
    return Open(stat->size, limits, [&vfs, uri](const u64 offset, const std::size_t size) { return vfs.ReadBinary(uri, offset, size); });
}

Result<ProductReader> ProductReader::Open(const Vfs& vfs, const ProductLocator& locator, const ProductLimits limits) {
    auto stat = vfs.Stat(locator.uri);
    if (!stat || stat->type != VfsEntryType::File)
        return stat ? Result<ProductReader>(Err(ErrorCode::FileReadError, "product locator is not a file")) : Result<ProductReader>(Err(std::move(stat).error()));
    if (locator.offset > stat->size || locator.size > stat->size - locator.offset)
        return Err(ErrorCode::ParseInvalidFormat, "product locator range exceeds its bundle");
    return Open(locator.size, limits, [&vfs, locator](const u64 offset, const std::size_t size) { return vfs.ReadBinary(locator.uri, locator.offset + offset, size); });
}

Result<ProductReader> ProductReader::Open(const ProductCache& cache, const ContentHash key, const ProductLimits limits) {
    auto size = cache.Size(key);
    if (!size)
        return Err(std::move(size).error());
    return Open(*size, limits, [&cache, key](const u64 offset, const std::size_t count) { return cache.ReadRange(key, offset, count); });
}

Result<std::vector<std::byte>> ProductReader::ReadRange(const u64 file_offset, const std::size_t size) const {
    if (file_offset > header_.file_size || size > header_.file_size - file_offset)
        return Err(ErrorCode::OutOfRange, "product read range exceeds file");
    auto bytes = read_(file_offset, size);
    if (bytes && bytes->size() != size)
        return Err(ErrorCode::FileEndOfFile, "product range read was truncated");
    return bytes;
}

Result<std::vector<std::byte>> ProductReader::ReadChunk(const std::size_t index) const {
    if (index >= header_.chunks.size())
        return Err(ErrorCode::OutOfRange, "product chunk index is out of range");
    const auto& chunk = header_.chunks[index];
    auto bytes = ReadRange(header_.payload_offset + chunk.offset, static_cast<std::size_t>(chunk.compressed_size));
    if (!bytes)
        return bytes;
    if (Sha256(*bytes) != chunk.checksum)
        return Err(ErrorCode::ParseInvalidFormat, "product chunk checksum is invalid");
    return bytes;
}

Result<Product> ProductReader::ReadProduct() const {
    auto bytes = ReadRange(0, static_cast<std::size_t>(header_.file_size));
    return bytes ? ParseProduct(*bytes, limits_) : Result<Product>(Err(std::move(bytes).error()));
}

} // namespace woki::asset
