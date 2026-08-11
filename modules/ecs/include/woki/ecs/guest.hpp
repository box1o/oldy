#pragma once

#include <stddef.h>
#include <stdint.h>

#if __has_include(<new>)
#include <new>
#else
#ifndef WOKI_PLACEMENT_NEW_DEFINED
#define WOKI_PLACEMENT_NEW_DEFINED

inline void* operator new(__SIZE_TYPE__, void* address) noexcept {
    return address;
}
#endif
#endif

namespace woki::guest {

struct Entity {
    static constexpr uint32_t kInvalidIndex = UINT32_MAX;

    uint32_t index{kInvalidIndex};
    uint32_t generation{UINT32_MAX};

    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return index != kInvalidIndex;
    }

    [[nodiscard]] constexpr bool operator==(const Entity&) const noexcept = default;
};

template <size_t Capacity>
class Registry final {
public:
    static_assert(Capacity > 0, "Guest registry capacity must be positive");

    [[nodiscard]] Entity Create() noexcept {
        for (size_t index = 0; index < Capacity; ++index) {
            if (!alive_[index]) {
                alive_[index] = true;
                ++size_;
                return {static_cast<uint32_t>(index), generations_[index]};
            }
        }
        return {};
    }

    [[nodiscard]] bool Destroy(Entity entity) noexcept {
        if (!Valid(entity))
            return false;
        alive_[entity.index] = false;
        ++generations_[entity.index];
        --size_;
        return true;
    }

    [[nodiscard]] bool Valid(Entity entity) const noexcept {
        return entity.index < Capacity && alive_[entity.index] && generations_[entity.index] == entity.generation;
    }

    [[nodiscard]] size_t Size() const noexcept {
        return size_;
    }

    [[nodiscard]] static constexpr size_t MaxSize() noexcept {
        return Capacity;
    }

private:
    bool alive_[Capacity]{};
    uint32_t generations_[Capacity]{};
    size_t size_{};
};

template <typename T, size_t Capacity>
class ComponentPool final {
public:
    static_assert(Capacity > 0, "Guest component pool capacity must be positive");
    ComponentPool() = default;

    ~ComponentPool() {
        Clear();
    }

    ComponentPool(const ComponentPool&) = delete;
    ComponentPool& operator=(const ComponentPool&) = delete;

    template <typename... Args>
    [[nodiscard]] T* Emplace(Entity entity, Args&&... args) noexcept(noexcept(T(static_cast<Args&&>(args)...))) {
        if (entity.index >= Capacity)
            return nullptr;
        Slot& slot = slots_[entity.index];
        if (slot.occupied) {
            if (slot.generation == entity.generation)
                return nullptr;
            slot.Value()->~T();
        }
        new (slot.storage) T(static_cast<Args&&>(args)...);
        slot.generation = entity.generation;
        slot.occupied = true;
        return slot.Value();
    }

    [[nodiscard]] T* Get(Entity entity) noexcept {
        Slot& slot = slots_[entity.index < Capacity ? entity.index : 0];
        return entity.index < Capacity && slot.occupied && slot.generation == entity.generation ? slot.Value() : nullptr;
    }

    [[nodiscard]] const T* Get(Entity entity) const noexcept {
        const Slot& slot = slots_[entity.index < Capacity ? entity.index : 0];
        return entity.index < Capacity && slot.occupied && slot.generation == entity.generation ? slot.Value() : nullptr;
    }

    [[nodiscard]] bool Remove(Entity entity) noexcept {
        T* value = Get(entity);
        if (value == nullptr)
            return false;
        value->~T();
        slots_[entity.index].occupied = false;
        return true;
    }

    void Clear() noexcept {
        for (Slot& slot : slots_) {
            if (slot.occupied) {
                slot.Value()->~T();
                slot.occupied = false;
            }
        }
    }

private:
    struct Slot {
        [[nodiscard]] T* Value() noexcept {
            return reinterpret_cast<T*>(storage);
        }

        [[nodiscard]] const T* Value() const noexcept {
            return reinterpret_cast<const T*>(storage);
        }

        alignas(T) unsigned char storage[sizeof(T)];
        uint32_t generation{};
        bool occupied{};
    };

    Slot slots_[Capacity]{};
};

} // namespace woki::guest
