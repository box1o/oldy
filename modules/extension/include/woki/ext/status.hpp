#pragma once

#include <string>

#include <woki/core.hpp>

namespace woki::ext {

enum class ExtensionState : u8 { Active, Failed };

struct ExtensionStatus {
    std::string extension_id;
    ExtensionState state{ExtensionState::Active};
    ErrorCode error_code{ErrorCode::Success};
    std::string error;
};

} // namespace woki::ext
