#pragma once

#include <woki/rhi/null.hpp>

namespace woki::rhi::null {

[[nodiscard]] Result<scope<Adapter>> CreateAdapter(NullRhiDescriptor descriptor = {});

} // namespace woki::rhi::null
