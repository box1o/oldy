#pragma once

#include <woki/ext/sdk/host.h>

namespace woki::ext {

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using i32 = int32_t;

/** A non-owning UTF-8 string view passed across the extension API. */
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

/** A non-owning byte view passed across the extension API. */
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

/** A raw ABI status code with named constructors for SDK results. */
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

/** An allocation-free UTF-8 message that tracks its own byte length. */
template <u32 Capacity>
class StringBuilder final {
    static_assert(Capacity > 0u, "StringBuilder capacity must be positive");

public:
    constexpr StringBuilder() noexcept = default;

    constexpr explicit StringBuilder(StringView initial) noexcept {
        (void)Append(initial);
    }

    [[nodiscard]] constexpr bool Append(StringView value) noexcept {
        if ((value.Size() != 0u && value.Data() == nullptr) || value.Size() > Capacity - size_) {
            valid_ = false;
            return false;
        }
        for (u32 index = 0; index < value.Size(); ++index)
            data_[size_++] = value.Data()[index];
        return true;
    }

    template <u32 N>
    [[nodiscard]] constexpr bool Append(const char (&value)[N]) noexcept {
        return Append(StringView{value});
    }

    [[nodiscard]] constexpr bool Append(char value) noexcept {
        if (size_ == Capacity) {
            valid_ = false;
            return false;
        }
        data_[size_++] = value;
        return true;
    }

    [[nodiscard]] constexpr bool Append(u8 value) noexcept {
        return Append(static_cast<u32>(value));
    }

    [[nodiscard]] constexpr bool Append(u16 value) noexcept {
        return Append(static_cast<u32>(value));
    }

    [[nodiscard]] constexpr bool Append(u32 value) noexcept {
        char digits[10]{};
        u32 count = 0;
        do {
            digits[count++] = static_cast<char>('0' + value % 10u);
            value /= 10u;
        } while (value != 0u);
        if (count > Capacity - size_) {
            valid_ = false;
            return false;
        }
        while (count != 0u)
            data_[size_++] = digits[--count];
        return true;
    }

    [[nodiscard]] constexpr bool Append(i32 value) noexcept {
        if (value >= 0)
            return Append(static_cast<u32>(value));
        const u32 original_size = size_;
        if (!Append('-') || !Append(0u - static_cast<u32>(value))) {
            size_ = original_size;
            valid_ = false;
            return false;
        }
        return true;
    }

    template <typename T>
    constexpr StringBuilder& operator<<(const T& value) noexcept {
        (void)Append(value);
        return *this;
    }

    [[nodiscard]] constexpr StringView View() const noexcept {
        return {data_, size_};
    }

    [[nodiscard]] constexpr operator StringView() const noexcept {
        return View();
    }

    [[nodiscard]] constexpr u32 Size() const noexcept {
        return size_;
    }

    [[nodiscard]] constexpr bool Complete() const noexcept {
        return valid_;
    }

private:
    char data_[Capacity]{};
    u32 size_{};
    bool valid_{true};
};

enum class LogLevel : u32 { Debug = WOKI_EXT_LOG_DEBUG, Info = WOKI_EXT_LOG_INFO, Warn = WOKI_EXT_LOG_WARN, Error = WOKI_EXT_LOG_ERROR };

/** Allocation-free access to the host logger. */
class Log final {
public:
    [[nodiscard]] Status Write(LogLevel level, StringView message) const noexcept {
        return Status{host_log(static_cast<u32>(level), message.Data(), message.Size())};
    }

    [[nodiscard]] Status Debug(StringView message) const noexcept {
        return Write(LogLevel::Debug, message);
    }

    [[nodiscard]] Status Info(StringView message) const noexcept {
        return Write(LogLevel::Info, message);
    }

    [[nodiscard]] Status Warn(StringView message) const noexcept {
        return Write(LogLevel::Warn, message);
    }

    [[nodiscard]] Status Error(StringView message) const noexcept {
        return Write(LogLevel::Error, message);
    }

    template <typename First, typename Second, typename... Rest>
    [[nodiscard]] Status Debug(const First& first, const Second& second, const Rest&... rest) const noexcept {
        return WriteParts(LogLevel::Debug, first, second, rest...);
    }

    template <typename First, typename Second, typename... Rest>
    [[nodiscard]] Status Info(const First& first, const Second& second, const Rest&... rest) const noexcept {
        return WriteParts(LogLevel::Info, first, second, rest...);
    }

    template <typename First, typename Second, typename... Rest>
    [[nodiscard]] Status Warn(const First& first, const Second& second, const Rest&... rest) const noexcept {
        return WriteParts(LogLevel::Warn, first, second, rest...);
    }

    template <typename First, typename Second, typename... Rest>
    [[nodiscard]] Status Error(const First& first, const Second& second, const Rest&... rest) const noexcept {
        return WriteParts(LogLevel::Error, first, second, rest...);
    }

private:
    template <typename First, typename Second, typename... Rest>
    [[nodiscard]] Status WriteParts(LogLevel level, const First& first, const Second& second, const Rest&... rest) const noexcept {
        StringBuilder<256> message;
        const bool complete = message.Append(first) && message.Append(second) && (message.Append(rest) && ...);
        return complete ? Write(level, message.View()) : Status::NoSpace();
    }
};

} // namespace woki::ext
