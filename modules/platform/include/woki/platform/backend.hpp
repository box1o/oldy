#pragma once

#include <string_view>

#include <woki/core.hpp>
#include "woki/input/capabilities.hpp"

namespace woki {

enum class CursorMode : u8;
enum class CursorType : u8;

class PlatformBackend {
public:
    virtual ~PlatformBackend() = default;
    [[nodiscard]] virtual bool IsOpen() const noexcept = 0;
    virtual void PollEvents() noexcept = 0;
    virtual void WaitEvents() noexcept = 0;
    virtual void Close() noexcept = 0;
    virtual void SetCursorMode(CursorMode mode) noexcept = 0;
    virtual void SetCursorType(CursorType type) noexcept = 0;
    virtual void StartTextInput() noexcept = 0;
    virtual void StopTextInput() noexcept = 0;
    [[nodiscard]] virtual void* NativeHandle() const noexcept = 0;
    [[nodiscard]] virtual const InputCapabilities& Capabilities() const noexcept = 0;
};

} // namespace woki
