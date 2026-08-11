#pragma once

#include <woki/ext/sdk/host.h>

namespace woki {

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using i32 = int32_t;
using f32 = float;
using f64 = double;

class StringView final {
public:
    constexpr StringView() noexcept = default;

    constexpr StringView(const char* data, u32 size) noexcept
        : data_(data),
          size_(size) {}

    template <u32 N>
    constexpr StringView(const char (&text)[N]) noexcept
        : data_(text),
          size_(N - 1u) {}

    [[nodiscard]] constexpr const char* Data() const noexcept {
        return data_;
    }

    [[nodiscard]] constexpr u32 Size() const noexcept {
        return size_;
    }

    [[nodiscard]] constexpr bool Empty() const noexcept {
        return size_ == 0u;
    }

    friend constexpr bool operator==(StringView left, StringView right) noexcept {
        if (left.size_ != right.size_)
            return false;
        for (u32 index = 0; index < left.size_; ++index) {
            if (left.data_[index] != right.data_[index])
                return false;
        }
        return true;
    }

private:
    const char* data_{};
    u32 size_{};
};

class Bytes final {
public:
    constexpr Bytes() noexcept = default;

    constexpr Bytes(const u8* data, u32 size) noexcept
        : data_(data),
          size_(size) {}

    [[nodiscard]] constexpr const u8* Data() const noexcept {
        return data_;
    }

    [[nodiscard]] constexpr u32 Size() const noexcept {
        return size_;
    }

    [[nodiscard]] constexpr bool Empty() const noexcept {
        return size_ == 0u;
    }

private:
    const u8* data_{};
    u32 size_{};
};

class MutableBytes final {
public:
    constexpr MutableBytes(u8* data, u32 capacity) noexcept
        : data_(data),
          capacity_(capacity) {}

    [[nodiscard]] constexpr u8* Data() const noexcept {
        return data_;
    }

    [[nodiscard]] constexpr u32 Capacity() const noexcept {
        return capacity_;
    }

    [[nodiscard]] constexpr u32 Size() const noexcept {
        return size_;
    }

    [[nodiscard]] constexpr Bytes View() const noexcept {
        return {data_, size_ <= capacity_ ? size_ : capacity_};
    }

    [[nodiscard]] constexpr bool Complete() const noexcept {
        return size_ <= capacity_;
    }

    constexpr void SetSize(u32 size) noexcept {
        size_ = size;
    }

private:
    u8* data_{};
    u32 capacity_{};
    u32 size_{};
};

template <u32 Capacity>
class StringBuffer final {
public:
    [[nodiscard]] constexpr char* Data() noexcept {
        return data_;
    }

    [[nodiscard]] static constexpr u32 Size() noexcept {
        return Capacity;
    }

    [[nodiscard]] constexpr StringView View() const noexcept {
        u32 size{};
        while (size < Capacity && data_[size] != '\0')
            ++size;
        return {data_, size};
    }

private:
    static_assert(Capacity > 0u, "StringBuffer capacity must be positive");
    char data_[Capacity]{};
};

class Status final {
public:
    constexpr Status() noexcept = default;

    constexpr explicit Status(i32 code) noexcept
        : code_(code) {}

    [[nodiscard]] constexpr i32 Code() const noexcept {
        return code_;
    }

    [[nodiscard]] constexpr bool Ok() const noexcept {
        return code_ == WOKI_EXT_OK;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return Ok();
    }

    static constexpr Status Success() noexcept {
        return Status{WOKI_EXT_OK};
    }

    static constexpr Status Error() noexcept {
        return Status{WOKI_EXT_ERR};
    }

    static constexpr Status Denied() noexcept {
        return Status{WOKI_EXT_DENIED};
    }

    static constexpr Status NoSpace() noexcept {
        return Status{WOKI_EXT_NO_SPACE};
    }

    static constexpr Status NotFound() noexcept {
        return Status{WOKI_EXT_NOT_FOUND};
    }

    static constexpr Status Invalid() noexcept {
        return Status{WOKI_EXT_INVALID};
    }

private:
    i32 code_{WOKI_EXT_OK};
};

} // namespace woki
