#pragma once

// IWYU pragma: private, include "woki/core.hpp"

#include <limits>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "../types/handle.hpp"

namespace woki {

template <typename T, typename HandleType>
class DenseHandleStorage final {
public:
    template <typename... Args>
    [[nodiscard]] T& Emplace(const HandleType handle, Args&&... args) {
        if (!handle.IsValid())
            throw std::invalid_argument("dense storage handle is invalid");
        if (values_.size() >= std::numeric_limits<u32>::max())
            throw std::length_error("dense storage index space exhausted");
        if (handle.Index() >= sparse_.size())
            sparse_.resize(static_cast<size_t>(handle.Index()) + 1);
        auto& entry = sparse_[handle.Index()];
        if (entry.dense != kInvalid)
            throw std::invalid_argument("dense storage handle index is already occupied");
        entry = {handle.Generation(), static_cast<u32>(values_.size())};
        handles_.push_back(handle);
        values_.emplace_back(std::forward<Args>(args)...);
        return values_.back();
    }

    [[nodiscard]] bool Contains(const HandleType handle) const noexcept {
        return handle.IsValid() && handle.Index() < sparse_.size() && sparse_[handle.Index()].generation == handle.Generation() && sparse_[handle.Index()].dense != kInvalid;
    }

    [[nodiscard]] T* TryGet(const HandleType handle) noexcept {
        return Contains(handle) ? &values_[sparse_[handle.Index()].dense] : nullptr;
    }

    [[nodiscard]] const T* TryGet(const HandleType handle) const noexcept {
        return Contains(handle) ? &values_[sparse_[handle.Index()].dense] : nullptr;
    }

    [[nodiscard]] T& Get(const HandleType handle) {
        if (auto* value = TryGet(handle))
            return *value;
        throw std::out_of_range("dense storage handle is stale");
    }

    [[nodiscard]] const T& Get(const HandleType handle) const {
        if (const auto* value = TryGet(handle))
            return *value;
        throw std::out_of_range("dense storage handle is stale");
    }

    [[nodiscard]] bool Remove(const HandleType handle) {
        if (!Contains(handle))
            return false;
        const u32 dense = sparse_[handle.Index()].dense;
        const u32 last = static_cast<u32>(values_.size() - 1);
        sparse_[handle.Index()].dense = kInvalid;
        if (dense != last) {
            values_[dense] = std::move(values_[last]);
            handles_[dense] = handles_[last];
            sparse_[handles_[dense].Index()].dense = dense;
        }
        values_.pop_back();
        handles_.pop_back();
        return true;
    }

    [[nodiscard]] u32 DenseIndex(const HandleType handle) const {
        if (!Contains(handle))
            throw std::out_of_range("dense storage handle is stale");
        return sparse_[handle.Index()].dense;
    }

    [[nodiscard]] std::span<T> Values() noexcept {
        return values_;
    }

    [[nodiscard]] std::span<const T> Values() const noexcept {
        return values_;
    }

    [[nodiscard]] std::span<const HandleType> Handles() const noexcept {
        return handles_;
    }

    [[nodiscard]] size_t Size() const noexcept {
        return values_.size();
    }

private:
    static constexpr u32 kInvalid = std::numeric_limits<u32>::max();

    struct SparseEntry final {
        u32 generation{};
        u32 dense{kInvalid};
    };

    std::vector<SparseEntry> sparse_;
    std::vector<T> values_;
    std::vector<HandleType> handles_;
};

} // namespace woki
