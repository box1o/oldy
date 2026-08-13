#pragma once

#include <woki/types/types.hpp>

namespace woki::ui {

enum class Curve : u8 { Linear, In, Out, InOut };

[[nodiscard]] f32 Ease(Curve curve, f32 value);

} // namespace woki::ui
