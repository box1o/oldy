#pragma once

// IWYU pragma: private, include "woki/core.hpp"

#include <algorithm>
#include <bit>
#include <vector>

#include "dirty_ranges.hpp"

namespace woki {

class DirtyBitset final {
public:
    explicit DirtyBitset(const u32 bit_count = 0) {
        Resize(bit_count);
    }

    void Resize(const u32 bit_count) {
        bit_count_ = bit_count;
        words_.assign((static_cast<size_t>(bit_count) + 63) / 64, 0);
    }

    void Mark(const u32 index) {
        if (index < bit_count_)
            words_[index / 64] |= u64{1} << (index % 64);
    }

    void Mark(const u32 first, const u32 count) {
        if (first >= bit_count_ || count == 0)
            return;
        const u32 end = first + std::min(count, bit_count_ - first);
        for (u32 index = first; index < end; ++index)
            Mark(index);
    }

    void Clear() noexcept {
        std::ranges::fill(words_, 0);
    }

    [[nodiscard]] bool Test(const u32 index) const noexcept {
        return index < bit_count_ && (words_[index / 64] & (u64{1} << (index % 64))) != 0;
    }

    [[nodiscard]] u32 Count() const noexcept {
        u32 count{};
        for (const u64 word : words_)
            count += static_cast<u32>(std::popcount(word));
        return count;
    }

    [[nodiscard]] std::vector<DirtyRange> Ranges(const u64 element_size) const {
        std::vector<DirtyRange> result;
        for (u32 index = 0; index < bit_count_;) {
            if (!Test(index)) {
                ++index;
                continue;
            }
            const u32 first = index;
            while (index < bit_count_ && Test(index))
                ++index;
            result.push_back({static_cast<u64>(first) * element_size, static_cast<u64>(index - first) * element_size});
        }
        return result;
    }

private:
    u32 bit_count_{};
    std::vector<u64> words_;
};

} // namespace woki
