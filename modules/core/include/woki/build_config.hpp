#pragma once

#include "types/types.hpp"

namespace woki {

enum class BuildMode : u8 { Debug, Release };

[[nodiscard]] constexpr const char* ToString(BuildMode mode) noexcept {
    return mode == BuildMode::Debug ? "Debug" : "Release";
}

class BuildConfig {
public:
    [[nodiscard]] static constexpr BuildMode Mode() noexcept {
#ifdef NDEBUG
        return BuildMode::Release;
#else
        return BuildMode::Debug;
#endif
    }

    [[nodiscard]] static constexpr bool IsDebug() noexcept {
        return Mode() == BuildMode::Debug;
    }

    [[nodiscard]] static constexpr bool IsRelease() noexcept {
        return Mode() == BuildMode::Release;
    }
};

} // namespace woki
