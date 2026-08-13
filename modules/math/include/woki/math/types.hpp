#pragma once

#include "mat/fwd.hpp"
#include "vec/fwd.hpp"
#include "quat/fwd.hpp"
#include "detail/arithmetic.hpp"

namespace woki::math {

template <arithmetic T = float>
using vec2 = vec<2, T>;
template <arithmetic T = float>
using vec3 = vec<3, T>;
template <arithmetic T = float>
using vec4 = vec<4, T>;

using vec2f = vec2<float>;
using vec3f = vec3<float>;
using vec4f = vec4<float>;

using vec2d = vec2<double>;
using vec3d = vec3<double>;
using vec4d = vec4<double>;

using vec2i = vec2<int>;
using vec3i = vec3<int>;
using vec4i = vec4<int>;

using vec2u = vec2<unsigned>;
using vec3u = vec3<unsigned>;
using vec4u = vec4<unsigned>;

template <arithmetic T = float>
using mat2 = mat<2, 2, T>;
template <arithmetic T = float>
using mat3_t = mat<3, 3, T>;
template <arithmetic T = float>
using mat4_t = mat<4, 4, T>;

using mat2f = mat2<float>;
using mat3f = mat3_t<float>;
using mat4f = mat4_t<float>;

using mat2d = mat2<double>;
using mat3d = mat3_t<double>;
using mat4d = mat4_t<double>;

template <floating_point T = float>
using quat_t = quat<T>;

using quatf = quat_t<float>;
using quatd = quat_t<double>;

} // namespace woki::math
