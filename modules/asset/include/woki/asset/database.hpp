#pragma once

// IWYU pragma: private, include "woki/asset.hpp"

#include <filesystem>
#include <map>
#include <shared_mutex>
#include <span>
#include <vector>

#include "id.hpp"
#include "uri.hpp"
#include "vfs.hpp"

namespace woki::asset {

struct AssetRecord {
    AssetId id;
    AssetUri uri;
    u32 source_type{};
    u32 product_type{};
    ContentHash source_hash;
    ContentHash product_hash;
    u64 revision{};
    u64 version{};
};

struct DatabaseLimits {
    u32 max_records{1'000'000};
    u32 max_uri_bytes{4096};
    std::size_t max_bytes{256U * 1024U * 1024U};
};

class AssetDatabase {
public:
    class Transaction;

    [[nodiscard]] Transaction BeginTransaction();
    [[nodiscard]] std::optional<AssetRecord> Find(AssetId id) const;
    [[nodiscard]] std::optional<AssetRecord> Find(const AssetUri& uri) const;
    [[nodiscard]] std::vector<AssetRecord> Records() const;
    [[nodiscard]] Result<std::vector<std::byte>> Serialize(DatabaseLimits limits = {}) const;
    [[nodiscard]] Result<void> ParseAndReplace(std::span<const std::byte> bytes, DatabaseLimits limits = {});
    [[nodiscard]] Result<void> Load(const Vfs& vfs, const AssetUri& uri, DatabaseLimits limits = {});
    [[nodiscard]] Result<void> Load(const std::filesystem::path& path, DatabaseLimits limits = {});
    [[nodiscard]] Result<void> SaveAtomic(const std::filesystem::path& path, DatabaseLimits limits = {}) const;

private:
    mutable std::shared_mutex mutex_;
    std::map<AssetId, AssetRecord> by_id_;
    std::map<AssetUri, AssetId> by_uri_;
    u64 epoch_{};
};

class AssetDatabase::Transaction {
public:
    [[nodiscard]] Result<void> Upsert(AssetRecord record);
    void Remove(AssetId id);
    [[nodiscard]] Result<void> Commit();

private:
    friend class AssetDatabase;

    Transaction(AssetDatabase& database, u64 epoch, std::map<AssetId, AssetRecord> records)
        : database_(&database),
          epoch_(epoch),
          records_(std::move(records)) {}

    AssetDatabase* database_{};
    u64 epoch_{};
    std::map<AssetId, AssetRecord> records_;
};

} // namespace woki::asset
