#include <set>

#include <woki/asset/reload.hpp>

namespace woki::asset {
void ReloadCoordinator::Push(const std::span<const ReloadHint> hints) {
    const auto now = std::chrono::steady_clock::now();
    for (const auto& hint : hints)
        pending_.insert_or_assign(hint.uri, std::pair(hint.type, now));
}

Result<std::vector<AssetId>> ReloadCoordinator::Reconcile(const AssetScheme scheme, const std::string_view authority) {
    auto uris = vfs_.Enumerate(scheme, authority);
    if (!uris)
        return Err(std::move(uris).error());
    std::set<AssetUri> present(uris->begin(), uris->end());
    std::vector<ReloadHint> hints;
    for (const auto& record : database_.Records())
        if (record.uri.Scheme() == scheme && record.uri.Authority() == authority && !present.contains(record.uri))
            hints.push_back({ReloadHintType::Removed, record.uri});
    for (const auto& uri : *uris)
        if (database_.Find(uri))
            hints.push_back({ReloadHintType::Changed, uri});
    Push(hints);
    return Pump();
}

Result<std::vector<AssetId>> ReloadCoordinator::Pump() {
    const auto now = std::chrono::steady_clock::now();
    std::set<AssetId> invalidated;
    auto transaction = database_.BeginTransaction();
    bool changed = false;
    for (auto it = pending_.begin(); it != pending_.end();) {
        if (now - it->second.second < debounce_) {
            ++it;
            continue;
        }
        const auto record = database_.Find(it->first);
        if (record) {
            bool source_changed = it->second.first == ReloadHintType::Removed;
            if (!source_changed) {
                auto bytes = vfs_.ReadBinary(record->uri);
                source_changed = !bytes || Sha256(*bytes) != record->source_hash;
            }
            if (source_changed) {
                invalidated.insert(record->id);
                const auto dependent = dependencies_.TransitiveDependents(record->id);
                invalidated.insert(dependent.begin(), dependent.end());
                AssetRecord next = *record;
                ++next.revision;
                if (it->second.first == ReloadHintType::Removed)
                    next.source_hash = {};
                auto updated = transaction.Upsert(std::move(next));
                if (!updated)
                    return Err(std::move(updated).error());
                changed = true;
            }
        }
        it = pending_.erase(it);
    }
    if (changed)
        TRY_VOID(transaction.Commit());
    std::vector<AssetId> result(invalidated.begin(), invalidated.end());
    manager_.Invalidate(result);
    return Ok(std::move(result));
}
} // namespace woki::asset
