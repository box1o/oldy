#pragma once

// IWYU pragma: private, include "woki/core.hpp"

#include <string>
#include <string_view>
#include <source_location>

#include "../types/types.hpp"

namespace woki {

enum class ErrorType : u8 {
    Core = 0,
    FileSystem,
    Network,
    Parse,
    Validation,
    Graphics,
};

enum class ErrorCode : u16 {
    Success = 0,
    InvalidState,
    OutOfRange,
    Cancelled,
    QueueFull,
    ExecutorStopped,

    FileNotFound = 100,
    FileAccessDenied,
    FileReadError,
    FileWriteError,
    FileEndOfFile,

    NetworkConnectionFailed = 200,
    NetworkConnectionClosed,
    NetworkTimeout,
    NetworkHostUnreachable,
    NetworkSendFailed,
    NetworkReceiveFailed,

    ParseInvalidFormat = 300,
    ParseUnexpectedToken,
    ParseMissingField,
    ParseTypeMismatch,

    ValidationNullValue = 400,
    ValidationOutOfRange,
    ValidationInvalidState,

    GraphicsInitFailed = 500,
    GraphicsResourceCreationFailed,
    GraphicsDeviceLost,
    GraphicsInvalidFormat,
    GraphicsFramebufferIncomplete,
    GraphicsShaderCompilationFailed,
    GraphicsTextureCreationFailed,
    GraphicsBufferCreationFailed,
    GraphicsUnsupportedApi,

    InvalidArgument = 900,
    FailedToAcquireResource,

    UnknownError = 999,
};

[[nodiscard]] const char* ToString(ErrorType type) noexcept;
[[nodiscard]] const char* ToString(ErrorCode code) noexcept;

class Error {
public:
    explicit Error(ErrorCode code, std::string_view message = {}, std::source_location location = std::source_location::current());

    [[nodiscard]] ErrorCode Code() const noexcept;
    [[nodiscard]] ErrorType Type() const noexcept;
    [[nodiscard]] std::string_view Message() const noexcept;
    [[nodiscard]] const std::source_location& Location() const noexcept;

    void Log() const;

private:
    ErrorCode code_;
    std::string message_;
    std::source_location location_;
};

} // namespace woki
