#include "woki/ext/limits.hpp"
#include "woki/ext/internal/event_service.hpp"

namespace woki::ext::host {

bool IsValidEventTopic(std::string_view topic) noexcept {
    if (topic.empty() || topic.size() > limits::kMaxEventTopicBytes || topic.front() == '.' || topic.back() == '.' || !topic.contains('.'))
        return false;
    std::size_t segment_start = 0;
    for (std::size_t index = 0; index <= topic.size(); ++index) {
        if (index != topic.size() && topic[index] != '.') {
            const unsigned char ch = static_cast<unsigned char>(topic[index]);
            if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-'))
                return false;
            continue;
        }
        const std::string_view segment = topic.substr(segment_start, index - segment_start);
        if (segment.empty() || segment.front() == '-' || segment.back() == '-')
            return false;
        segment_start = index + 1;
    }
    return true;
}

void EventSession::Subscribe(u32 event_type) {
    subscriptions_.insert(event_type);
}

void EventSession::Subscribe(std::string topic) {
    named_subscriptions_.insert(std::move(topic));
}

bool EventSession::IsSubscribed(u32 event_type) const noexcept {
    return subscriptions_.contains(kWildcardEventType) || subscriptions_.contains(event_type);
}

bool EventSession::IsSubscribed(std::string_view topic) const noexcept {
    return named_subscriptions_.contains(std::string(topic));
}

void EventService::SetBus(EventBus* bus) noexcept {
    bus_ = bus;
}

Result<void> EventService::Enqueue(Event event) {
    if (bus_ == nullptr)
        return Err(ErrorCode::InvalidState, "Extension event bus is not configured.");
    if (queue_.size() >= kMaxQueuedEvents)
        return Err(ErrorCode::ValidationOutOfRange, "Extension event queue is full.");
    queue_.push_back(std::move(event));
    return Ok();
}

void EventService::Drain() {
    if (draining_ || bus_ == nullptr)
        return;
    draining_ = true;

    struct DrainGuard final {
        bool& draining;

        ~DrainGuard() {
            draining = false;
        }
    } guard{draining_};

    while (!queue_.empty() && bus_ != nullptr) {
        EventBus* bus = bus_;
        Event event = std::move(queue_.front());
        queue_.pop_front();
        bus->Publish(event);
    }
}

} // namespace woki::ext::host
