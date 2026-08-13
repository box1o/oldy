#include <cmath>

#include "woki/input/gesture_recognizer.hpp"

namespace woki {
namespace {
constexpr f32 kPanThreshold = 4.0f;
constexpr f32 kTapDistance = 8.0f;
constexpr f64 kTapSeconds = 0.35;
constexpr f64 kDoubleTapSeconds = 0.45;
constexpr f64 kLongPressSeconds = 0.5;

f32 Distance(f32 x, f32 y) {
    return std::sqrt(x * x + y * y);
}

events::GestureData Data(events::GesturePhase phase, f32 x, f32 y, u8 count) {
    return {.phase = phase, .center_x = x, .center_y = y, .pointer_count = count};
}
} // namespace

std::vector<scope<events::Event>> GestureRecognizer::Process(const events::Event& event) {
    std::vector<scope<events::Event>> output;
    const events::PointerData* pointer = nullptr;
    if (event.GetEventType() == events::EventType::kPointerDown)
        pointer = &static_cast<const events::PointerDownEvent&>(event).pointer_data;
    if (event.GetEventType() == events::EventType::kPointerMoved)
        pointer = &static_cast<const events::PointerMoveEvent&>(event).pointer_data;
    if (event.GetEventType() == events::EventType::kPointerUp)
        pointer = &static_cast<const events::PointerUpEvent&>(event).pointer_data;
    if (event.GetEventType() == events::EventType::kPointerCancel)
        pointer = &static_cast<const events::PointerCancelEvent&>(event).pointer_data;
    if (pointer == nullptr || pointer->kind == events::PointerKind::kMouse)
        return output;

    if (event.GetEventType() == events::EventType::kPointerDown) {
        contacts_[pointer->pointer] = {*pointer, *pointer, event.metadata.timestamp, false, false};
        if (contacts_.size() == 2) {
            const auto& a = contacts_.begin()->second.current;
            const auto& b = std::next(contacts_.begin())->second.current;
            initial_distance_ = Distance(b.x - a.x, b.y - a.y);
            initial_angle_ = std::atan2(b.y - a.y, b.x - a.x);
            last_scale_ = 1;
            last_angle_ = 0;
        }
        return output;
    }

    auto found = contacts_.find(pointer->pointer);
    if (found == contacts_.end())
        return output;
    auto& contact = found->second;
    contact.current = *pointer;
    if (event.GetEventType() == events::EventType::kPointerMoved) {
        const f32 dx = pointer->x - contact.start.x, dy = pointer->y - contact.start.y;
        const bool starts = !contact.panning && Distance(dx, dy) >= kPanThreshold;
        if (starts)
            contact.panning = true;
        if (contact.panning) {
            auto data = Data(starts ? events::GesturePhase::kBegin : events::GesturePhase::kUpdate, pointer->x, pointer->y, static_cast<u8>(contacts_.size()));
            data.delta_x = pointer->delta_x;
            data.delta_y = pointer->delta_y;
            data.total_x = dx;
            data.total_y = dy;
            output.push_back(createScope<events::PanEvent>(data));
        }
        if (contacts_.size() == 2 && initial_distance_ > 0) {
            const auto& a = contacts_.begin()->second.current;
            const auto& b = std::next(contacts_.begin())->second.current;
            const f32 center_x = (a.x + b.x) * 0.5f, center_y = (a.y + b.y) * 0.5f;
            const f32 scale = Distance(b.x - a.x, b.y - a.y) / initial_distance_;
            const f32 angle = std::atan2(b.y - a.y, b.x - a.x) - initial_angle_;
            output.push_back(createScope<events::PinchEvent>(Data(last_scale_ == 1 ? events::GesturePhase::kBegin : events::GesturePhase::kUpdate, center_x, center_y, 2), scale / last_scale_, scale));
            output.push_back(createScope<events::RotateEvent>(Data(last_scale_ == 1 ? events::GesturePhase::kBegin : events::GesturePhase::kUpdate, center_x, center_y, 2), angle - last_angle_, angle));
            last_scale_ = scale;
            last_angle_ = angle;
        }
        return output;
    }

    const bool canceled = event.GetEventType() == events::EventType::kPointerCancel;
    if (contact.panning)
        output.push_back(createScope<events::PanEvent>(Data(canceled ? events::GesturePhase::kCancel : events::GesturePhase::kEnd, pointer->x, pointer->y, static_cast<u8>(contacts_.size()))));
    else if (!canceled && !contact.long_pressed && event.metadata.timestamp - contact.started <= kTapSeconds && Distance(pointer->x - contact.start.x, pointer->y - contact.start.y) <= kTapDistance) {
        auto data = Data(events::GesturePhase::kEnd, pointer->x, pointer->y, 1);
        if (last_tap_time_ >= 0 && event.metadata.timestamp - last_tap_time_ <= kDoubleTapSeconds && Distance(pointer->x - last_tap_x_, pointer->y - last_tap_y_) <= kTapDistance)
            output.push_back(createScope<events::DoubleTapEvent>(data));
        else
            output.push_back(createScope<events::TapEvent>(data));
        last_tap_time_ = event.metadata.timestamp;
        last_tap_x_ = pointer->x;
        last_tap_y_ = pointer->y;
    }
    if (contacts_.size() == 2) {
        const auto phase = canceled ? events::GesturePhase::kCancel : events::GesturePhase::kEnd;
        output.push_back(createScope<events::PinchEvent>(Data(phase, pointer->x, pointer->y, 2), 1, last_scale_));
        output.push_back(createScope<events::RotateEvent>(Data(phase, pointer->x, pointer->y, 2), 0, last_angle_));
    }
    contacts_.erase(found);
    return output;
}

std::vector<scope<events::Event>> GestureRecognizer::Update(f64 timestamp) {
    std::vector<scope<events::Event>> output;
    for (auto& [id, contact] : contacts_) {
        (void)id;
        if (contact.long_pressed || contact.panning || timestamp - contact.started < kLongPressSeconds)
            continue;
        if (Distance(contact.current.x - contact.start.x, contact.current.y - contact.start.y) > kTapDistance)
            continue;
        contact.long_pressed = true;
        output.push_back(createScope<events::LongPressEvent>(Data(events::GesturePhase::kBegin, contact.current.x, contact.current.y, 1)));
    }
    return output;
}

void GestureRecognizer::Reset() noexcept {
    contacts_.clear();
    initial_distance_ = 0;
    last_scale_ = 1;
    last_angle_ = 0;
}
} // namespace woki
