#pragma once

// Host implementation detail. This header is not installed.

#include <deque>
#include <memory>

#include "../host/event_bus.hpp"

namespace woki::ext::host {

inline constexpr std::size_t kMaxQueuedEvents = 1024;
inline constexpr std::size_t kMaxEventsPerDrain = 1024;

class EventService final {
public:
    void SetBus(EventBus* bus) noexcept;
    [[nodiscard]] Result<void> Enqueue(Event event);
    void Drain();

    [[nodiscard]] std::size_t Checkpoint() const noexcept {
        return queue_.size();
    }

    void DiscardAfter(std::size_t checkpoint) noexcept {
        while (queue_.size() > checkpoint)
            queue_.pop_back();
    }

private:
    EventBus* bus_{};
    std::deque<Event> queue_;
    bool draining_{};
};

} // namespace woki::ext::host
