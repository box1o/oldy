#include <algorithm>
#include <array>
#include <limits>

#include <woki/asset/manifest.hpp>

namespace woki::asset {
namespace {
constexpr std::array kMagic{std::byte{'W'}, std::byte{'K'}, std::byte{'A'}, std::byte{'M'}};

template <typename T>
void Put(std::vector<std::byte>& out, T value) {
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        out.push_back(static_cast<std::byte>(value & 0xffU));
        value >>= 8U;
    }
}

void PutId(std::vector<std::byte>& out, const AssetId& id) {
    for (u8 value : id.Bytes())
        out.push_back(static_cast<std::byte>(value));
}

void PutHash(std::vector<std::byte>& out, const ContentHash& hash) {
    for (u8 value : hash.Bytes())
        out.push_back(static_cast<std::byte>(value));
}

void PutString(std::vector<std::byte>& out, const std::string_view value) {
    Put(out, static_cast<u32>(value.size()));
    out.insert(out.end(), std::as_bytes(std::span(value)).begin(), std::as_bytes(std::span(value)).end());
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
        if (bytes_.size() - pos_ < AssetId::kSize)
            return false;
        std::array<u8, AssetId::kSize> bytes{};
        for (u8& byte : bytes)
            byte = std::to_integer<u8>(bytes_[pos_++]);
        id = AssetId(bytes);
        return true;
    }

    bool Hash(ContentHash& hash) {
        if (bytes_.size() - pos_ < ContentHash::kSize)
            return false;
        std::array<u8, ContentHash::kSize> data{};
        for (u8& value : data)
            value = std::to_integer<u8>(bytes_[pos_++]);
        hash = ContentHash(data);
        return true;
    }

    bool String(std::string& value) {
        u32 size{};
        if (!Int(size) || size > 4096 || bytes_.size() - pos_ < size)
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

Result<void> AssetManifest::Add(ManifestEntry entry) {
    if (!entry.asset_id || entry.target.empty() || !entry.locator.uri.String().size() || !entries_.emplace(entry.asset_id, std::move(entry)).second)
        return Err(ErrorCode::InvalidArgument, "manifest entry is incomplete or duplicated");
    return Ok();
}

const ManifestEntry* AssetManifest::Resolve(const AssetId id) const noexcept {
    const auto found = entries_.find(id);
    return found == entries_.end() ? nullptr : &found->second;
}

std::vector<ManifestEntry> AssetManifest::Entries() const {
    std::vector<ManifestEntry> result;
    result.reserve(entries_.size());
    for (const auto& [id, entry] : entries_)
        result.push_back(entry);
    return result;
}

Result<std::vector<std::byte>> AssetManifest::Serialize() const {
    std::vector<std::byte> out(kMagic.begin(), kMagic.end());
    Put(out, kAssetManifestVersion);
    Put(out, u16{});
    Put(out, static_cast<u32>(entries_.size()));
    for (const auto& [id, entry] : entries_) {
        if (entry.target.size() > 4096 || entry.locator.uri.String().size() > 4096 || entry.available_chunks.size() > 65'536)
            return Err(ErrorCode::OutOfRange, "manifest entry exceeds format limits");
        PutId(out, id);
        Put(out, entry.type);
        PutHash(out, entry.product_hash);
        Put(out, entry.version);
        PutString(out, entry.target);
        PutHash(out, entry.capability_fingerprint);
        PutString(out, entry.locator.uri.String());
        Put(out, entry.locator.offset);
        Put(out, entry.locator.size);
        Put(out, static_cast<u32>(entry.available_chunks.size()));
        for (const auto semantic : entry.available_chunks)
            Put(out, static_cast<u32>(semantic));
    }
    return Ok(std::move(out));
}

Result<AssetManifest> AssetManifest::Parse(const std::span<const std::byte> bytes, const std::size_t max_bytes) {
    if (bytes.size() < 12 || bytes.size() > max_bytes || !std::ranges::equal(kMagic, bytes.first(4)))
        return Err(ErrorCode::ParseInvalidFormat, "asset manifest header is invalid");
    Reader in(bytes.subspan(4));
    u16 version{}, flags{};
    u32 count{};
    if (!in.Int(version) || !in.Int(flags) || !in.Int(count) || version != kAssetManifestVersion || flags || count > 1'000'000)
        return Err(ErrorCode::ParseInvalidFormat, "asset manifest version or count is invalid");
    AssetManifest manifest;
    for (u32 i = 0; i < count; ++i) {
        ManifestEntry entry;
        std::string uri;
        u32 chunks{};
        if (!in.Id(entry.asset_id) || !in.Int(entry.type) || !in.Hash(entry.product_hash) || !in.Int(entry.version) || !in.String(entry.target) || !in.Hash(entry.capability_fingerprint) || !in.String(uri)
            || !in.Int(entry.locator.offset) || !in.Int(entry.locator.size) || !in.Int(chunks) || chunks > 65'536)
            return Err(ErrorCode::ParseInvalidFormat, "asset manifest entry is truncated");
        auto parsed_uri = AssetUri::Parse(uri);
        if (!parsed_uri)
            return Err(ErrorCode::ParseInvalidFormat, "asset manifest locator URI is invalid");
        entry.locator.uri = std::move(*parsed_uri);
        entry.available_chunks.resize(chunks);
        for (auto& semantic : entry.available_chunks) {
            u32 value{};
            if (!in.Int(value))
                return Err(ErrorCode::ParseInvalidFormat, "asset manifest chunk list is truncated");
            semantic = static_cast<ProductChunkSemantic>(value);
        }
        if (auto added = manifest.Add(std::move(entry)); !added)
            return Err(ErrorCode::ParseInvalidFormat, added.error().Message());
    }
    if (!in.End())
        return Err(ErrorCode::ParseInvalidFormat, "asset manifest contains trailing bytes");
    return Ok(std::move(manifest));
}

Result<AssetManifest> AssetManifest::Load(const Vfs& vfs, const AssetUri& uri, const std::size_t max_bytes) {
    auto bytes = vfs.ReadBinary(uri, max_bytes);
    return bytes ? Parse(*bytes, max_bytes) : Result<AssetManifest>(Err(std::move(bytes).error()));
}
} // namespace woki::asset
