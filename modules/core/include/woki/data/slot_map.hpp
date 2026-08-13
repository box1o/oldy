#pragma once

// IWYU pragma: private, include "woki/core.hpp"

#include <limits>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "../types/handle.hpp"

namespace woki {

template <typename T, typename HandleType>
class SlotMap final {
public:
    template <typename... Args>
    [[nodiscard]] HandleType Emplace(Args&&... args) {
        u32 index;
        if (free_.empty()) {
            if (slots_.size() >= std::numeric_limits<u32>::max())
                throw std::length_error("slot map index space exhausted");
            index = static_cast<u32>(slots_.size());
            slots_.push_back({});
        } else {
            index = free_.back();
            free_.pop_back();
        }
        auto& slot = slots_[index];
        slot.value.emplace(std::forward<Args>(args)...);
        ++size_;
        return HandleType::Create(index, slot.generation);
    }

    [[nodiscard]] bool Contains(const HandleType handle) const noexcept {
        return handle.IsValid() && handle.Index() < slots_.size() && slots_[handle.Index()].generation == handle.Generation() && slots_[handle.Index()].value.has_value();
    }

    [[nodiscard]] T* TryGet(const HandleType handle) noexcept {
        return Contains(handle) ? &*slots_[handle.Index()].value : nullptr;
    }

    [[nodiscard]] const T* TryGet(const HandleType handle) const noexcept {
        return Contains(handle) ? &*slots_[handle.Index()].value : nullptr;
    }

    [[nodiscard]] T& Get(const HandleType handle) {
        if (auto* value = TryGet(handle))
            return *value;
        throw std::out_of_range("slot map handle is stale");
    }

    [[nodiscard]] const T& Get(const HandleType handle) const {
        if (const auto* value = TryGet(handle))
            return *value;
        throw std::out_of_range("slot map handle is stale");
    }

    [[nodiscard]] bool Remove(const HandleType handle) {
        if (!Contains(handle))
            return false;
        auto& slot = slots_[handle.Index()];
        slot.value.reset();
        --size_;
        if (slot.generation != std::numeric_limits<u32>::max()) {
            ++slot.generation;
            free_.push_back(handle.Index());
        }
        return true;
    }

    [[nodiscard]] size_t Size() const noexcept {
        return size_;
    }

    [[nodiscard]] std::vector<HandleType> Handles() const {
        std::vector<HandleType> result;
        result.reserve(size_);
        for (u32 index = 0; index < slots_.size(); ++index)
            if (slots_[index].value)
                result.push_back(HandleType::Create(index, slots_[index].generation));
        return result;
    }

    template <typename Function>
    void ForEach(Function&& function) {
        for (u32 index = 0; index < slots_.size(); ++index)
            if (slots_[index].value)
                function(HandleType::Create(index, slots_[index].generation), *slots_[index].value);
    }

    template <typename Function>
    void ForEach(Function&& function) const {
        for (u32 index = 0; index < slots_.size(); ++index)
            if (slots_[index].value)
                function(HandleType::Create(index, slots_[index].generation), *slots_[index].value);
    }

private:
    struct Slot final {
        u32 generation{1};
        std::optional<T> value;
    };

    std::vector<Slot> slots_;
    std::vector<u32> free_;
    size_t size_{};
};

} // namespace woki
