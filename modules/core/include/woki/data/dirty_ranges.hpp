#pragma once

// IWYU pragma: private, include "woki/core.hpp"

#include <algorithm>
#include <limits>
#include <span>
#include <vector>

#include "../types/types.hpp"

namespace woki {

struct DirtyRange final {
    u64 offset{};
    u64 size{};
};

class DirtyRangeSet final {
public:
    explicit DirtyRangeSet(const u64 whole_size = 0, const f32 whole_threshold = 0.6F)
        : whole_size_(whole_size),
          threshold_(std::clamp(whole_threshold, 0.0F, 1.0F)) {}

    void Mark(const u64 offset, const u64 size) {
        if (size == 0 || (whole_size_ != 0 && offset >= whole_size_))
            return;
        const u64 available = whole_size_ == 0 ? std::numeric_limits<u64>::max() - offset : whole_size_ - offset;
        const u64 end = offset + std::min(size, available);
        DirtyRange merged{offset, end - offset};
        auto first = std::lower_bound(ranges_.begin(), ranges_.end(), offset, [](const DirtyRange& range, const u64 value) { return range.offset + range.size < value; });
        while (first != ranges_.end() && first->offset <= merged.offset + merged.size) {
            const u64 merged_end = std::max(merged.offset + merged.size, first->offset + first->size);
            merged.offset = std::min(merged.offset, first->offset);
            merged.size = merged_end - merged.offset;
            dirty_bytes_ -= first->size;
            first = ranges_.erase(first);
        }
        dirty_bytes_ += merged.size;
        ranges_.insert(first, merged);
        if (whole_size_ != 0 && static_cast<f64>(dirty_bytes_) >= static_cast<f64>(whole_size_) * threshold_) {
            ranges_ = {{0, whole_size_}};
            dirty_bytes_ = whole_size_;
        }
    }

    void Clear() noexcept {
        ranges_.clear();
        dirty_bytes_ = 0;
    }

    [[nodiscard]] bool Empty() const noexcept {
        return ranges_.empty();
    }

    [[nodiscard]] bool Whole() const noexcept {
        return whole_size_ != 0 && ranges_.size() == 1 && ranges_.front().offset == 0 && ranges_.front().size == whole_size_;
    }

    [[nodiscard]] std::span<const DirtyRange> Ranges() const noexcept {
        return ranges_;
    }

    [[nodiscard]] u64 Covered() const noexcept {
        return dirty_bytes_;
    }

    [[nodiscard]] u64 DirtyBytes() const noexcept {
        return dirty_bytes_;
    }

private:
    u64 whole_size_{};
    u64 dirty_bytes_{};
    f32 threshold_{};
    std::vector<DirtyRange> ranges_;
};

} // namespace woki
