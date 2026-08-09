#include "woki/error/errors.hpp"
#include "woki/logger/logger.hpp"

namespace woki {

namespace {

[[nodiscard]] std::string_view DefaultMessage(ErrorCode code) noexcept {
    return ToString(code);
}

} // namespace

const char* ToString(ErrorType type) noexcept {
    switch (type) {
        case ErrorType::Core:
            return "Core";
        case ErrorType::FileSystem:
            return "FileSystem";
        case ErrorType::Network:
            return "Network";
        case ErrorType::Parse:
            return "Parse";
        case ErrorType::Validation:
            return "Validation";
        case ErrorType::Graphics:
            return "Graphics";
    }

    return "UnknownType";
}

const char* ToString(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::Success:
            return "Success";
        case ErrorCode::InvalidState:
            return "InvalidState";
        case ErrorCode::OutOfRange:
            return "OutOfRange";
        case ErrorCode::FileNotFound:
            return "FileNotFound";
        case ErrorCode::FileAccessDenied:
            return "FileAccessDenied";
        case ErrorCode::FileReadError:
            return "FileReadError";
        case ErrorCode::FileWriteError:
            return "FileWriteError";
        case ErrorCode::FileEndOfFile:
            return "FileEndOfFile";
        case ErrorCode::NetworkConnectionFailed:
            return "NetworkConnectionFailed";
        case ErrorCode::NetworkConnectionClosed:
            return "NetworkConnectionClosed";
        case ErrorCode::NetworkTimeout:
            return "NetworkTimeout";
        case ErrorCode::NetworkHostUnreachable:
            return "NetworkHostUnreachable";
        case ErrorCode::NetworkSendFailed:
            return "NetworkSendFailed";
        case ErrorCode::NetworkReceiveFailed:
            return "NetworkReceiveFailed";
        case ErrorCode::ParseInvalidFormat:
            return "ParseInvalidFormat";
        case ErrorCode::ParseUnexpectedToken:
            return "ParseUnexpectedToken";
        case ErrorCode::ParseMissingField:
            return "ParseMissingField";
        case ErrorCode::ParseTypeMismatch:
            return "ParseTypeMismatch";
        case ErrorCode::ValidationNullValue:
            return "ValidationNullValue";
        case ErrorCode::ValidationOutOfRange:
            return "ValidationOutOfRange";
        case ErrorCode::ValidationInvalidState:
            return "ValidationInvalidState";
        case ErrorCode::GraphicsInitFailed:
            return "GraphicsInitFailed";
        case ErrorCode::GraphicsResourceCreationFailed:
            return "GraphicsResourceCreationFailed";
        case ErrorCode::GraphicsDeviceLost:
            return "GraphicsDeviceLost";
        case ErrorCode::GraphicsInvalidFormat:
            return "GraphicsInvalidFormat";
        case ErrorCode::GraphicsFramebufferIncomplete:
            return "GraphicsFramebufferIncomplete";
        case ErrorCode::GraphicsShaderCompilationFailed:
            return "GraphicsShaderCompilationFailed";
        case ErrorCode::GraphicsTextureCreationFailed:
            return "GraphicsTextureCreationFailed";
        case ErrorCode::GraphicsBufferCreationFailed:
            return "GraphicsBufferCreationFailed";
        case ErrorCode::GraphicsUnsupportedApi:
            return "GraphicsUnsupportedApi";
        case ErrorCode::InvalidArgument:
            return "InvalidArgument";
        case ErrorCode::FailedToAcquireResource:
            return "FailedToAcquireResource";
        case ErrorCode::UnknownError:
            return "UnknownError";
    }

    return "UnknownError";
}

Error::Error(ErrorCode code, std::string_view message, std::source_location location)
    : code_(code),
      message_(message),
      location_(location) {}

ErrorCode Error::Code() const noexcept {
    return code_;
}

ErrorType Error::Type() const noexcept {
    const u16 code_value = static_cast<u16>(code_);

    if (code_value >= 100 && code_value < 200) {
        return ErrorType::FileSystem;
    }
    if (code_value >= 200 && code_value < 300) {
        return ErrorType::Network;
    }
    if (code_value >= 300 && code_value < 400) {
        return ErrorType::Parse;
    }
    if (code_value >= 400 && code_value < 500) {
        return ErrorType::Validation;
    }
    if (code_value >= 500 && code_value < 600) {
        return ErrorType::Graphics;
    }

    return ErrorType::Core;
}

std::string_view Error::Message() const noexcept {
    return message_;
}

const std::source_location& Error::Location() const noexcept {
    return location_;
}

void Error::Log() const {
    const std::string_view message = message_.empty() ? DefaultMessage(code_) : message_;
    slog::Error("{}:{} [{}:{}] {}", location_.file_name(), location_.line(), ToString(Type()), ToString(code_), message);
}

} // namespace woki
