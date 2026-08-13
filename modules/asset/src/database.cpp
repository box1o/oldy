#include <array>
#include <fstream>
#include <limits>
#include <mutex>
#include <random>

#include <woki/asset/database.hpp>

namespace woki::asset {
namespace {
constexpr std::array<std::byte, 4> kMagic{std::byte{'W'}, std::byte{'K'}, std::byte{'D'}, std::byte{'B'}};
constexpr u16 kVersion = 1;

template <typename T>
void Put(std::vector<std::byte>& out, T value) {
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        out.push_back(static_cast<std::byte>(value & 0xffU));
        value >>= 8U;
    }
}

void PutId(std::vector<std::byte>& out, const AssetId& id) {
    for (u8 byte : id.Bytes())
        out.push_back(static_cast<std::byte>(byte));
}

void PutHash(std::vector<std::byte>& out, const ContentHash& hash) {
    for (u8 byte : hash.Bytes())
        out.push_back(static_cast<std::byte>(byte));
}

class Reader {
public:
    explicit Reader(std::span<const std::byte> bytes)
        : bytes_(bytes) {}

    template <typename T>
    bool Int(T& value) {
        if (bytes_.size() - pos_ < sizeof(T))
            return false;
        u64 bits{};
        for (std::size_t i = 0; i < sizeof(T); ++i)
            bits |= static_cast<u64>(std::to_integer<u8>(bytes_[pos_++])) << (i * 8U);
        value = static_cast<T>(bits);
        return true;
    }

    bool Id(AssetId& id) {
        std::array<u8, AssetId::kSize> bytes{};
        for (u8& byte : bytes)
            if (!Int(byte))
                return false;
        id = AssetId(bytes);
        return true;
    }

    bool Hash(ContentHash& hash) {
        std::array<u8, ContentHash::kSize> bytes{};
        for (u8& byte : bytes)
            if (!Int(byte))
                return false;
        hash = ContentHash(bytes);
        return true;
    }

    bool String(std::string& value, u32 limit) {
        u32 size{};
        if (!Int(size) || size > limit || bytes_.size() - pos_ < size)
            return false;
        value.assign(reinterpret_cast<const char*>(bytes_.data() + pos_), size);
        pos_ += size;
        return true;
    }

    bool End() const {
        return pos_ == bytes_.size();
    }

private:
    std::span<const std::byte> bytes_;
    std::size_t pos_{};
};
} // namespace

AssetDatabase::Transaction AssetDatabase::BeginTransaction() {
    std::shared_lock lock(mutex_);
    return Transaction(*this, epoch_, by_id_);
}

std::optional<AssetRecord> AssetDatabase::Find(const AssetId id) const {
    std::shared_lock lock(mutex_);
    const auto found = by_id_.find(id);
    return found == by_id_.end() ? std::nullopt : std::optional(found->second);
}

std::optional<AssetRecord> AssetDatabase::Find(const AssetUri& uri) const {
    std::shared_lock lock(mutex_);
    const auto found = by_uri_.find(uri);
    return found == by_uri_.end() ? std::nullopt : std::optional(by_id_.at(found->second));
}

std::vector<AssetRecord> AssetDatabase::Records() const {
    std::shared_lock lock(mutex_);
    std::vector<AssetRecord> result;
    result.reserve(by_id_.size());
    for (const auto& [id, record] : by_id_)
        result.push_back(record);
    return result;
}

Result<void> AssetDatabase::Transaction::Upsert(AssetRecord record) {
    if (!record.id)
        return Err(ErrorCode::InvalidArgument, "database record has no asset ID");
    for (const auto& [id, current] : records_)
        if (id != record.id && current.uri == record.uri)
            return Err(ErrorCode::InvalidArgument, "asset URI is already assigned to another ID");
    records_.insert_or_assign(record.id, std::move(record));
    return Ok();
}

void AssetDatabase::Transaction::Remove(const AssetId id) {
    records_.erase(id);
}

Result<void> AssetDatabase::Transaction::Commit() {
    if (!database_)
        return Err(ErrorCode::InvalidState, "database transaction is already committed");
    std::map<AssetUri, AssetId> uris;
    for (const auto& [id, record] : records_)
        if (!uris.emplace(record.uri, id).second)
            return Err(ErrorCode::InvalidArgument, "database transaction contains duplicate URIs");
    std::unique_lock lock(database_->mutex_);
    if (database_->epoch_ != epoch_)
        return Err(ErrorCode::InvalidState, "database changed while transaction was open");
    database_->by_id_ = std::move(records_);
    database_->by_uri_ = std::move(uris);
    ++database_->epoch_;
    database_ = nullptr;
    return Ok();
}

Result<std::vector<std::byte>> AssetDatabase::Serialize(const DatabaseLimits limits) const {
    const auto records = Records();
    if (records.size() > limits.max_records)
        return Err(ErrorCode::OutOfRange, "database record count exceeds limit");
    std::vector<std::byte> out;
    out.insert(out.end(), kMagic.begin(), kMagic.end());
    Put(out, kVersion);
    Put(out, u16{});
    Put(out, static_cast<u32>(records.size()));
    for (const auto& record : records) {
        if (record.uri.String().size() > limits.max_uri_bytes)
            return Err(ErrorCode::OutOfRange, "database URI exceeds limit");
        PutId(out, record.id);
        Put(out, static_cast<u32>(record.uri.String().size()));
        for (char c : record.uri.String())
            out.push_back(static_cast<std::byte>(c));
        Put(out, record.source_type);
        Put(out, record.product_type);
        PutHash(out, record.source_hash);
        PutHash(out, record.product_hash);
        Put(out, record.revision);
        Put(out, record.version);
        if (out.size() > limits.max_bytes)
            return Err(ErrorCode::OutOfRange, "database exceeds size limit");
    }
    return Ok(std::move(out));
}

Result<void> AssetDatabase::ParseAndReplace(const std::span<const std::byte> bytes, const DatabaseLimits limits) {
    if (bytes.size() > limits.max_bytes || bytes.size() < 12 || !std::equal(kMagic.begin(), kMagic.end(), bytes.begin()))
        return Err(ErrorCode::ParseInvalidFormat, "database header is invalid");
    Reader in(bytes.subspan(4));
    u16 version{}, flags{};
    u32 count{};
    if (!in.Int(version) || !in.Int(flags) || !in.Int(count) || version != kVersion || flags != 0 || count > limits.max_records)
        return Err(ErrorCode::ParseInvalidFormat, "database version or record count is invalid");
    std::map<AssetId, AssetRecord> replacement;
    std::map<AssetUri, AssetId> uris;
    for (u32 i = 0; i < count; ++i) {
        AssetId id;
        std::string uri_text;
        u32 source_type{}, product_type{};
        ContentHash source_hash, product_hash;
        u64 revision{}, record_version{};
        if (!in.Id(id) || !in.String(uri_text, limits.max_uri_bytes) || !in.Int(source_type) || !in.Int(product_type) || !in.Hash(source_hash) || !in.Hash(product_hash) || !in.Int(revision) || !in.Int(record_version))
            return Err(ErrorCode::ParseInvalidFormat, "database record is truncated");
        auto uri = AssetUri::Parse(uri_text);
        if (!uri)
            return Err(ErrorCode::ParseInvalidFormat, "database record contains an invalid URI");
        AssetRecord record{id, std::move(*uri), source_type, product_type, source_hash, product_hash, revision, record_version};
        if (!id || !replacement.emplace(id, record).second || !uris.emplace(record.uri, id).second)
            return Err(ErrorCode::ParseInvalidFormat, "database contains a nil or duplicate identity");
    }
    if (!in.End())
        return Err(ErrorCode::ParseInvalidFormat, "database contains trailing bytes");
    std::shared_lock lock(mutex_);
    const u64 epoch = epoch_;
    lock.unlock();
    return Transaction(*this, epoch, std::move(replacement)).Commit();
}

Result<void> AssetDatabase::Load(const Vfs& vfs, const AssetUri& uri, const DatabaseLimits limits) {
    auto bytes = vfs.ReadBinary(uri, limits.max_bytes);
    return bytes ? ParseAndReplace(*bytes, limits) : Err(std::move(bytes).error());
}

Result<void> AssetDatabase::Load(const std::filesystem::path& path, const DatabaseLimits limits) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error)
        return Err(ErrorCode::FileNotFound, "database file was not found");
    if (size > limits.max_bytes)
        return Err(ErrorCode::OutOfRange, "database exceeds size limit");
    std::ifstream stream(path, std::ios::binary);
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    if (!stream || (!bytes.empty() && !stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))))
        return Err(ErrorCode::FileReadError, "failed to read database");
    return ParseAndReplace(bytes, limits);
}

Result<void> AssetDatabase::SaveAtomic(const std::filesystem::path& path, const DatabaseLimits limits) const {
    auto bytes = Serialize(limits);
    if (!bytes)
        return Err(std::move(bytes).error());
    std::random_device random;
    auto temporary = path;
    temporary += ".tmp-" + std::to_string(random());
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream || (!bytes->empty() && !stream.write(reinterpret_cast<const char*>(bytes->data()), static_cast<std::streamsize>(bytes->size()))) || !stream.flush()) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return Err(ErrorCode::FileWriteError, "failed to write temporary database");
        }
    }
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return Err(ErrorCode::FileWriteError, "failed to atomically replace database");
    }
    return Ok();
}

} // namespace woki::asset
