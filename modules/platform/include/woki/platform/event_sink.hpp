#pragma once

#include <woki/core.hpp>
#include "woki/events/base.hpp"

namespace woki {

class PlatformEventSink {
public:
    virtual ~PlatformEventSink() = default;
    virtual void SubmitPlatformEvent(scope<events::Event> event) noexcept = 0;
};

} // namespace woki
