// This translation unit intentionally includes only the public umbrella header.
#include <woki/math.hpp>

static_assert(woki::math::vec<3, int>(1, 2, 3).x == 1);
static_assert(woki::math::mat<2, 2, int>::identity()(1, 1) == 1);

namespace {
[[maybe_unused]] auto HeaderOnlyProbe() {
    return woki::math::quat<float>(0.0F, 0.0F, 0.0F, 1.0F).toMat4();
}
} // namespace
