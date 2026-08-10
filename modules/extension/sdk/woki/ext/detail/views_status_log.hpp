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
};

} // namespace woki::ext
