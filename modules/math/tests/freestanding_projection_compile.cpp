// Projection must provide all of its own prerequisites without the umbrella.
#include <woki/math/interop/projection.hpp>

namespace {
[[maybe_unused]] auto FreestandingProjectionProbe() {
    using namespace woki::math;
    const auto identity = mat<4, 4, float>::identity();
    return project_ndc(vec<3, float>(0.0F, 0.0F, -1.0F), identity, identity);
}
} // namespace
