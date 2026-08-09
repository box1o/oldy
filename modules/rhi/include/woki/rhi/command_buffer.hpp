#pragma once

#include <string_view>

#include "types.hpp"

namespace woki::rhi {

class CommandBuffer {
public:
    virtual ~CommandBuffer() = default;

    virtual void SetLabel(std::string_view label) = 0;
    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    CommandBuffer() = default;
};

} // namespace woki::rhi
