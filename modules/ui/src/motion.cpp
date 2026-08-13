#include <algorithm>

#include <woki/ui/motion/motion.hpp>

namespace woki::ui {

namespace {

Color Mix(Color from, Color to, f32 t) {
    return {
        from.r + (to.r - from.r) * t,
        from.g + (to.g - from.g) * t,
        from.b + (to.b - from.b) * t,
        from.a + (to.a - from.a) * t,
    };
}

} // namespace

void Motion::Set(Key key, Property property, MotionValue target, Transition transition, Clock::time_point now) {
    const Id id{key.Value(), property};
    const auto found = tracks_.find(id);
    MotionValue current = found == tracks_.end() ? target : Sample(found->second, now);
    tracks_.insert_or_assign(id, Track{std::move(current), std::move(target), transition, now});
}

MotionValue Motion::Get(Key key, Property property, Clock::time_point now) const {
    const auto found = tracks_.find({key.Value(), property});
    return found == tracks_.end() ? MotionValue{0.0f} : Sample(found->second, now);
}

bool Motion::Active(Clock::time_point now) const {
    return std::any_of(tracks_.begin(), tracks_.end(), [&](const auto& entry) {
        const auto end = entry.second.start + entry.second.transition.delay + entry.second.transition.duration;
        return now < end;
    });
}

void Motion::Clear(Key key) {
    std::erase_if(tracks_, [&](const auto& entry) { return entry.first.key == key.Value(); });
}

MotionValue Motion::Sample(const Track& track, Clock::time_point now) {
    const auto elapsed = now - track.start - track.transition.delay;
    const f32 duration = static_cast<f32>(track.transition.duration.count());
    const f32 milliseconds = std::chrono::duration<f32, std::milli>(elapsed).count();
    const f32 t = Ease(track.transition.curve, duration <= 0.0f ? 1.0f : milliseconds / duration);
    if (track.from.index() != track.to.index()) {
        return t < 1.0f ? track.from : track.to;
    }
    if (const auto* from = std::get_if<f32>(&track.from)) {
        return *from + (std::get<f32>(track.to) - *from) * t;
    }
    return Mix(std::get<Color>(track.from), std::get<Color>(track.to), t);
}

} // namespace woki::ui
