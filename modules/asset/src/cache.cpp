#include <fstream>
#include <random>

#include <woki/asset/cache.hpp>

namespace woki::asset {
namespace {
void Quarantine(const std::filesystem::path& root, const std::filesystem::path& path, const ContentHash hash) {
    std::error_code error;
    const auto quarantine = root / "quarantine";
    std::filesystem::create_directories(quarantine, error);
    if (error)
        return;
    std::filesystem::rename(path, quarantine / (hash.Hex() + ".corrupt-" + std::to_string(std::random_device{}())), error);
}
} // namespace

Result<ProductCache> ProductCache::Create(std::filesystem::path root, const std::size_t limit) {
    ProductCacheDescriptor descriptor{
        .persistent_root = std::move(root),
        .target = "default",
        .platform = "default",
        .capability_fingerprint = {},
    };
    return Create(std::move(descriptor), limit);
}

Result<ProductCache> ProductCache::Create(ProductCacheDescriptor descriptor, const std::size_t limit) {
    std::error_code error;
    std::filesystem::create_directories(descriptor.persistent_root, error);
    if (error || !std::filesystem::is_directory(descriptor.persistent_root, error) || descriptor.target.empty() || descriptor.platform.empty())
        return Err(ErrorCode::FileAccessDenied, "product cache root is unavailable");
    descriptor.persistent_root = std::filesystem::weakly_canonical(descriptor.persistent_root, error);
    return Ok(ProductCache(std::move(descriptor), limit));
}

ProductCache::ProductCache(ProductCache&& other) noexcept
    : descriptor_(std::move(other.descriptor_)),
      root_(std::move(other.root_)),
      max_product_bytes_(other.max_product_bytes_) {}

std::filesystem::path ProductCache::Path(const ContentHash key) const {
    return Path({descriptor_.target, descriptor_.platform, descriptor_.capability_fingerprint, 0, key});
}

std::filesystem::path ProductCache::Path(const ProductCacheKey& key) const {
    const auto target = Sha256(key.target).Hex();
    const auto platform = Sha256(key.platform).Hex();
    const auto capability = key.capability_fingerprint.Hex();
    const auto product = key.product_hash.Hex();
    return root_ / target / platform / capability / std::to_string(key.asset_type) / product.substr(0, 2) / (product + ".wkas");
}

Result<Product> ProductCache::Load(const ContentHash key) const {
    return Load({descriptor_.target, descriptor_.platform, descriptor_.capability_fingerprint, 0, key});
}

Result<u64> ProductCache::Size(const ContentHash key) const {
    return Size({descriptor_.target, descriptor_.platform, descriptor_.capability_fingerprint, 0, key});
}

Result<u64> ProductCache::Size(const ProductCacheKey& key) const {
    std::error_code error;
    const auto size = std::filesystem::file_size(Path(key), error);
    if (error)
        return Err(ErrorCode::FileNotFound, "cached product was not found");
    if (size > max_product_bytes_)
        return Err(ErrorCode::OutOfRange, "cached product exceeds read limit");
    return Ok(static_cast<u64>(size));
}

Result<std::vector<std::byte>> ProductCache::ReadRange(const ContentHash key, const u64 offset, const std::size_t size) const {
    return ReadRange({descriptor_.target, descriptor_.platform, descriptor_.capability_fingerprint, 0, key}, offset, size);
}

Result<std::vector<std::byte>> ProductCache::ReadRange(const ProductCacheKey& key, const u64 offset, const std::size_t size) const {
    auto file_size = Size(key);
    if (!file_size)
        return Err(std::move(file_size).error());
    if (offset > *file_size || size > *file_size - offset)
        return Err(ErrorCode::OutOfRange, "cached product range exceeds file");
    std::ifstream stream(Path(key), std::ios::binary);
    stream.seekg(static_cast<std::streamoff>(offset));
    std::vector<std::byte> bytes(size);
    if (!stream || (size && !stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size))))
        return Err(ErrorCode::FileReadError, "failed to read cached product range");
    return Ok(std::move(bytes));
}

Result<Product> ProductCache::Load(const ProductCacheKey& key) const {
    const auto path = Path(key);
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error)
        return Err(ErrorCode::FileNotFound, "cached product was not found");
    if (size > max_product_bytes_)
        return Err(ErrorCode::OutOfRange, "cached product exceeds read limit");
    std::ifstream stream(path, std::ios::binary);
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    if (!stream || (!bytes.empty() && !stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))))
        return Err(ErrorCode::FileReadError, "failed to read cached product");
    auto product = ParseProduct(bytes, {.max_payload_bytes = max_product_bytes_});
    if (!product) {
        Quarantine(root_, path, key.product_hash);
        return Err(std::move(product).error());
    }
    if (product->product_hash != key.product_hash || (key.asset_type != 0 && (product->type != key.asset_type || product->target_fingerprint != key.capability_fingerprint))) {
        Quarantine(root_, path, key.product_hash);
        return Err(ErrorCode::ParseInvalidFormat, "cache key does not match product hash");
    }
    return product;
}

Result<void> ProductCache::Store(const ContentHash key, const Product& product) {
    return Store({descriptor_.target, descriptor_.platform, descriptor_.capability_fingerprint, 0, key}, product);
}

Result<void> ProductCache::Store(const ProductCacheKey& key, const Product& product) {
    if (key.target.empty() || key.platform.empty() || product.product_hash != key.product_hash || (key.asset_type != 0 && (product.type != key.asset_type || product.target_fingerprint != key.capability_fingerprint)))
        return Err(ErrorCode::InvalidArgument, "cache key does not match product hash");
    auto bytes = SerializeProduct(product, {.max_payload_bytes = max_product_bytes_});
    if (!bytes)
        return Err(std::move(bytes).error());
    std::lock_guard lock(writer_);
    const auto path = Path(key);
    std::error_code error;
    if (std::filesystem::exists(path, error) && !error) {
        auto existing = Load(key);
        if (existing)
            return Ok();
        if (std::filesystem::exists(path, error))
            Quarantine(root_, path, key.product_hash);
    }
    error.clear();
    std::filesystem::create_directories(path.parent_path(), error);
    if (error)
        return Err(ErrorCode::FileAccessDenied, "failed to create cache shard");
    const auto lock_path = path.string() + ".lock";
    if (!std::filesystem::create_directory(lock_path, error))
        return Err(ErrorCode::InvalidState, "another process owns the product cache writer lock");

    struct Lock {
        std::filesystem::path path;

        ~Lock() {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
        }
    } process_lock{lock_path};

    std::random_device random;
    auto temporary = path;
    temporary += ".tmp-" + std::to_string(random());
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream || (!bytes->empty() && !stream.write(reinterpret_cast<const char*>(bytes->data()), static_cast<std::streamsize>(bytes->size()))) || !stream.flush()) {
            std::filesystem::remove(temporary, error);
            return Err(ErrorCode::FileWriteError, "failed to write cached product");
        }
    }
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return Err(ErrorCode::FileWriteError, "failed to publish cached product");
    }
    return Ok();
}

Result<void> ProductCache::Remove(const ContentHash key) {
    std::lock_guard lock(writer_);
    std::error_code error;
    std::filesystem::remove(Path(key), error);
    if (error)
        return Err(ErrorCode::FileWriteError, "failed to remove cached product");
    return Ok();
}
} // namespace woki::asset
