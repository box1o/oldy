#pragma once

#include <utility>

#include <woki/gfx/advanced/mesh.hpp>
#include <woki/rhi/types.hpp>

namespace woki::gfx::detail {

template <typename T>
[[nodiscard]] constexpr T AlignUp(T value, T alignment) noexcept {
    return alignment == 0 ? value : value + (alignment - value % alignment) % alignment;
}

template <typename To, typename From>
[[nodiscard]] Result<To> CheckedCast(From value) {
    if (!std::in_range<To>(value))
        return Err(ErrorCode::OutOfRange, "numeric conversion is out of range");
    return Ok(static_cast<To>(value));
}

inline rhi::VertexFormat ToRhiVertexFormat(const VertexFormat value) noexcept {
    switch (value) {
        case VertexFormat::Float32x2:
            return rhi::VertexFormat::Float32x2;
        case VertexFormat::Float32x3:
            return rhi::VertexFormat::Float32x3;
        case VertexFormat::Float32x4:
            return rhi::VertexFormat::Float32x4;
        case VertexFormat::Uint16x4:
            return rhi::VertexFormat::Uint16x4;
        case VertexFormat::Unorm16x4:
            return rhi::VertexFormat::Unorm16x4;
    }
    return rhi::VertexFormat::Float32x3;
}

} // namespace woki::gfx::detail
