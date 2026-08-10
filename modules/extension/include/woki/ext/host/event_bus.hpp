#pragma once

#include <string>
#include <vector>
#include <optional>
#include <string_view>

#include <woki/core.hpp>
#include <woki/ext/sdk/events.h>

namespace woki::ext::host {

inline constexpr u32 kWildcardEventType = WOKI_EXT_EVENT_WILDCARD;
inline constexpr u32 kExtensionEventNamespace = WOKI_EXT_EVENT_NAMESPACE;

enum class EventOriginKind : u8 { Host, Extension };

struct EventOrigin {
    EventOriginKind kind{EventOriginKind::Host};
    std::string extension_id;
};

struct Event {
    u32 type{};
    std::vector<u8> payload;
    EventOrigin origin;
    std::optional<std::string> topic;
};

[[nodiscard]] bool IsValidEventTopic(std::string_view topic) noexcept;

class EventBus {
public:
    virtual ~EventBus() = default;
    virtual void Publish(const Event& event) = 0;
};

} // namespace woki::ext::host
