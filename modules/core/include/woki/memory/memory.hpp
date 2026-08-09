#pragma once

// IWYU pragma: private, include "woki/core.hpp"

#include <new>
#include <limits>
#include <memory>
#include <vector>
#include <cstddef>
#include <utility>
#include <type_traits>
#include <memory_resource>

#include "../types/types.hpp"

namespace woki {

namespace detail {

class BumpResource final : public std::pmr::memory_resource {
public:
    BumpResource(void* memory, std::size_t size);

    std::size_t used() const;
    std::size_t peak() const;
    bool can_fit(std::size_t bytes, std::size_t alignment) const;

    void reset();
    bool rewind(std::size_t offset, std::size_t expected_offset) noexcept;

private:
    static bool is_power_of_two(std::size_t value) noexcept;
    bool next_allocation(std::size_t bytes, std::size_t alignment, std::size_t& aligned_offset, std::size_t& next_offset) const noexcept;

    void* do_allocate(std::size_t bytes, std::size_t alignment) override;
    void do_deallocate(void* ptr, std::size_t bytes, std::size_t alignment) override;
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override;

    std::byte* begin_ = nullptr;
    std::size_t size_ = 0;
    std::size_t offset_ = 0;
    std::size_t peak_ = 0;
};

} // namespace detail

class Arena {
public:
    explicit Arena(u64 size = 8ull * 1024ull * 1024ull);
    ~Arena() noexcept;

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    Arena(Arena&&) = delete;
    Arena& operator=(Arena&&) = delete;

    template <typename T, typename... Args>
    requires std::is_nothrow_destructible_v<T>
    [[nodiscard]] T* create(Args&&... args) {
        static_assert(!std::is_void_v<T>, "Arena::create<T> does not support void");
        static_assert(!std::is_array_v<T>, "Arena::create<T> does not support arrays");

        if constexpr (!std::is_trivially_destructible_v<T>) {
            destructors_.reserve(destructors_.size() + 1);
        }

        const std::size_t previous_offset = resource_.used();
        void* memory = resource_.allocate(sizeof(T), alignof(T));
        const std::size_t allocated_offset = resource_.used();

        T* object = nullptr;
        try {
            object = std::construct_at(static_cast<T*>(memory), std::forward<Args>(args)...);
        } catch (...) {
            resource_.rewind(previous_offset, allocated_offset);
            throw;
        }

        if constexpr (!std::is_trivially_destructible_v<T>) {
            destructors_.push_back({object, [](void* ptr) noexcept { std::destroy_at(static_cast<T*>(ptr)); }});
        }

        return object;
    }

    template <typename T>
    [[nodiscard]] T* allocate(u64 count = 1) {
        static_assert(!std::is_void_v<T>, "Arena::allocate<void> is invalid");
        static_assert(std::is_trivially_destructible_v<T>, "Arena::allocate<T> only supports trivially destructible types. Use create<T>() instead.");

        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::bad_alloc{};
        }

        void* memory = resource_.allocate(sizeof(T) * static_cast<std::size_t>(count), alignof(T));

        return static_cast<T*>(memory);
    }

    [[nodiscard]] void* allocate_bytes(u64 bytes, u64 alignment = alignof(std::max_align_t));

    [[nodiscard]] std::pmr::memory_resource* resource();
    [[nodiscard]] const std::pmr::memory_resource* resource() const;

    [[nodiscard]] u64 capacity() const;
    [[nodiscard]] u64 used() const;
    [[nodiscard]] u64 free() const;
    [[nodiscard]] u64 peak() const;

    [[nodiscard]] bool empty() const;

    [[nodiscard]] bool contains(const void* ptr) const;
    [[nodiscard]] bool valid(const void* ptr) const;

    [[nodiscard]] const void* begin() const;
    [[nodiscard]] const void* end() const;
    [[nodiscard]] const void* current() const;

    [[nodiscard]] bool can_fit(u64 bytes, u64 alignment = alignof(std::max_align_t)) const;

    template <typename T>
    [[nodiscard]] bool can_fit(u64 count = 1) const {
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            return false;
        }

        return can_fit(sizeof(T) * count, alignof(T));
    }

    void clear() noexcept;
    void verify() const;

private:
    struct Destructor {
        void* ptr = nullptr;
        void (*destroy)(void*) noexcept = nullptr;
    };

    std::vector<std::byte> buffer_;
    detail::BumpResource resource_;
    std::vector<Destructor> destructors_;
    bool clearing_ = false;
};

} // namespace woki
