#include <algorithm>

#include <woki/ui/motion/curve.hpp>

namespace woki::ui {

f32 Ease(Curve curve, f32 value) {
    const f32 t = std::clamp(value, 0.0f, 1.0f);
    switch (curve) {
        case Curve::In:
            return t * t;
        case Curve::Out:
            return 1.0f - (1.0f - t) * (1.0f - t);
        case Curve::InOut:
            return t < 0.5f ? 2.0f * t * t : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
        case Curve::Linear:
            return t;
    }
    return t;
}

} // namespace woki::ui
