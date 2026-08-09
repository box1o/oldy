#include <new>
#include <limits>
#include <cassert>
#include <cstdint>
#include <algorithm>

#include "../../include/woki/memory/memory.hpp"

namespace woki::detail {

BumpResource::BumpResource(void* memory, std::size_t size)
    : begin_(static_cast<std::byte*>(memory)),
      size_(size) {}

std::size_t BumpResource::used() const {
    return offset_;
}

std::size_t BumpResource::peak() const {
    return peak_;
}

bool BumpResource::can_fit(std::size_t bytes, std::size_t alignment) const {
    std::size_t aligned_offset = 0;
    std::size_t next_offset = 0;
    return next_allocation(bytes, alignment, aligned_offset, next_offset);
}

void BumpResource::reset() {
    offset_ = 0;
}

bool BumpResource::rewind(std::size_t offset, std::size_t expected_offset) noexcept {
    if (offset_ != expected_offset || offset > expected_offset) {
        return false;
    }

    offset_ = offset;
    return true;
}

bool BumpResource::is_power_of_two(std::size_t value) noexcept {
    return value != 0 && (value & (value - 1)) == 0;
}

bool BumpResource::next_allocation(std::size_t bytes, std::size_t alignment, std::size_t& aligned_offset, std::size_t& next_offset) const noexcept {
    if (begin_ == nullptr || offset_ > size_ || !is_power_of_two(alignment)) {
        return false;
    }

    const auto current = reinterpret_cast<std::uintptr_t>(begin_ + offset_);
    const std::size_t misalignment = static_cast<std::size_t>(current % alignment);
    const std::size_t padding = misalignment == 0 ? 0 : alignment - misalignment;

    if (padding > size_ - offset_) {
        return false;
    }

    aligned_offset = offset_ + padding;
    const std::size_t remaining = size_ - aligned_offset;

    if (bytes > remaining) {
        return false;
    }

    next_offset = aligned_offset + bytes;
    return true;
}

void* BumpResource::do_allocate(std::size_t bytes, std::size_t alignment) {
    std::size_t aligned_offset = 0;
    std::size_t next_offset = 0;

    if (!next_allocation(bytes, alignment, aligned_offset, next_offset)) {
        throw std::bad_alloc{};
    }

    offset_ = next_offset;
    peak_ = std::max(peak_, offset_);

    return begin_ + aligned_offset;
}

void BumpResource::do_deallocate(void*, std::size_t, std::size_t) {
    // No-op. Arena memory is released all at once by reset().
}

bool BumpResource::do_is_equal(const std::pmr::memory_resource& other) const noexcept {
    return this == &other;
}

} // namespace woki::detail

namespace woki {

Arena::Arena(u64 size)
    : buffer_(static_cast<std::size_t>(size)),
      resource_(buffer_.data(), buffer_.size()) {}

Arena::~Arena() noexcept {
    clear();
}

void* Arena::allocate_bytes(u64 bytes, u64 alignment) {
    if (bytes > std::numeric_limits<std::size_t>::max() || alignment > std::numeric_limits<std::size_t>::max()) {
        throw std::bad_alloc{};
    }

    return resource_.allocate(static_cast<std::size_t>(bytes), static_cast<std::size_t>(alignment));
}

std::pmr::memory_resource* Arena::resource() {
    return &resource_;
}

const std::pmr::memory_resource* Arena::resource() const {
    return &resource_;
}

u64 Arena::capacity() const {
    return static_cast<u64>(buffer_.size());
}

u64 Arena::used() const {
    return static_cast<u64>(resource_.used());
}

u64 Arena::free() const {
    return capacity() - used();
}

u64 Arena::peak() const {
    return static_cast<u64>(resource_.peak());
}

bool Arena::empty() const {
    return used() == 0;
}

bool Arena::contains(const void* ptr) const {
    if (ptr == nullptr || buffer_.empty()) {
        return false;
    }

    const auto first = reinterpret_cast<std::uintptr_t>(buffer_.data());
    const auto last = first + buffer_.size();
    const auto p = reinterpret_cast<std::uintptr_t>(ptr);

    return p >= first && p < last;
}

bool Arena::valid(const void* ptr) const {
    if (ptr == nullptr || buffer_.empty()) {
        return false;
    }

    const auto first = reinterpret_cast<std::uintptr_t>(buffer_.data());
    const auto current_ptr = first + resource_.used();
    const auto p = reinterpret_cast<std::uintptr_t>(ptr);

    return p >= first && p < current_ptr;
}

const void* Arena::begin() const {
    return buffer_.data();
}

const void* Arena::end() const {
    return buffer_.data() + buffer_.size();
}

const void* Arena::current() const {
    return buffer_.data() + resource_.used();
}

bool Arena::can_fit(u64 bytes, u64 alignment) const {
    if (bytes > std::numeric_limits<std::size_t>::max() || alignment > std::numeric_limits<std::size_t>::max()) {
        return false;
    }

    return resource_.can_fit(static_cast<std::size_t>(bytes), static_cast<std::size_t>(alignment));
}

void Arena::clear() noexcept {
    if (clearing_) {
        return;
    }

    clearing_ = true;
    while (!destructors_.empty()) {
        const Destructor destructor = destructors_.back();
        destructors_.pop_back();
        destructor.destroy(destructor.ptr);
    }

    resource_.reset();
    clearing_ = false;
}

void Arena::verify() const {
    assert(used() <= capacity());
    assert(free() == capacity() - used());
    assert(peak() <= capacity());
}

} // namespace woki
