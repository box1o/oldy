#pragma once

#include "op.hpp"
#include "../mat/base.hpp"
#include "../mat/mat4.hpp"
#include "../vec/vec3.hpp"
#include "../vec/vec4.hpp"
#include "../mat/functions.hpp"
#include "../common/functions.hpp"

namespace woki::math {

// Project a 3D world point to normalized device coordinates (-1..1)
template <floating_point T>
[[nodiscard]] inline vec<3, T> project_ndc(const vec<3, T>& world, const mat<4, 4, T>& view, const mat<4, 4, T>& proj) noexcept {
    vec<4, T> v(world, T{1});
    v = view * v;
    v = proj * v;
    return vec<3, T>(v.x / v.w, v.y / v.w, v.z / v.w);
}

// Unproject NDC point (-1..1) back to world space
template <floating_point T>
[[nodiscard]] inline vec<3, T> unproject_ndc(const vec<3, T>& ndc, const mat<4, 4, T>& view, const mat<4, 4, T>& proj) noexcept {
    mat<4, 4, T> inv_vp = woki::math::inverse(proj * view);
    vec<4, T> v(ndc, T{1});
    v = inv_vp * v;
    return vec<3, T>(v.x / v.w, v.y / v.w, v.z / v.w);
}

} // namespace woki::math
