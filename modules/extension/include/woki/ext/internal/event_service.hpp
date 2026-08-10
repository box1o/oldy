#pragma once

// Host implementation detail. This header is not installed.

#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>

#include "../host/event_bus.hpp"

namespace woki::ext::host {

inline constexpr std::size_t kMaxQueuedEvents = 1024;

class EventSession final {
public:
    void Subscribe(u32 event_type);
    void Subscribe(std::string topic);
    [[nodiscard]] bool IsSubscribed(u32 event_type) const noexcept;
    [[nodiscard]] bool IsSubscribed(std::string_view topic) const noexcept;

private:
    std::unordered_set<u32> subscriptions_;
    std::unordered_set<std::string> named_subscriptions_;
};

class EventService final {
public:
    void SetBus(EventBus* bus) noexcept;
    [[nodiscard]] Result<void> Enqueue(Event event);
    void Drain();

private:
    EventBus* bus_{};
    std::deque<Event> queue_;
    bool draining_{};
};

} // namespace woki::ext::host
