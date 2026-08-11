#pragma once

#include <woki/extension/types.hpp>

namespace slog {
using woki::Status;

namespace detail {

using namespace woki;

template <u32 Capacity>
class Message final {
public:
    [[nodiscard]] constexpr bool Append(StringView value) noexcept {
        if ((value.Size() != 0u && value.Data() == nullptr) || value.Size() > Capacity - size_)
            return false;
        for (u32 index = 0; index < value.Size(); ++index)
            data_[size_++] = value.Data()[index];
        return true;
    }

    template <u32 N>
    [[nodiscard]] constexpr bool Append(const char (&value)[N]) noexcept {
        return Append(StringView{value});
    }

    [[nodiscard]] constexpr bool Append(char value) noexcept {
        if (size_ == Capacity)
            return false;
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
        u32 count{};
        do {
            digits[count++] = static_cast<char>('0' + value % 10u);
            value /= 10u;
        } while (value != 0u);
        if (count > Capacity - size_)
            return false;
        while (count != 0u)
            data_[size_++] = digits[--count];
        return true;
    }

    [[nodiscard]] constexpr bool Append(i32 value) noexcept {
        if (value >= 0)
            return Append(static_cast<u32>(value));
        const u32 previous = size_;
        if (!Append('-') || !Append(0u - static_cast<u32>(value))) {
            size_ = previous;
            return false;
        }
        return true;
    }

    [[nodiscard]] constexpr StringView View() const noexcept {
        return {data_, size_};
    }

private:
    char data_[Capacity]{};
    u32 size_{};
};

template <typename... Parts>
[[nodiscard]] Status Write(u32 level, const Parts&... parts) noexcept {
    Message<256> message;
    if (!(message.Append(parts) && ...))
        return Status::NoSpace();
    const StringView view = message.View();
    return Status{host_log(level, view.Data(), view.Size())};
}

} // namespace detail

template <typename... Parts>
[[nodiscard]] Status Debug(const Parts&... parts) noexcept {
    return detail::Write(WOKI_EXT_LOG_DEBUG, parts...);
}

template <typename... Parts>
[[nodiscard]] Status Info(const Parts&... parts) noexcept {
    return detail::Write(WOKI_EXT_LOG_INFO, parts...);
}

template <typename... Parts>
[[nodiscard]] Status Warn(const Parts&... parts) noexcept {
    return detail::Write(WOKI_EXT_LOG_WARN, parts...);
}

template <typename... Parts>
[[nodiscard]] Status Error(const Parts&... parts) noexcept {
    return detail::Write(WOKI_EXT_LOG_ERROR, parts...);
}

} // namespace slog
