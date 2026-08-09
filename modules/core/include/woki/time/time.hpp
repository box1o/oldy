#pragma once

// IWYU pragma: private, include "woki/core.hpp"

#include <chrono>

#include "../types/types.hpp"

namespace woki {

class Clock {
public:
    using SteadyClock = std::chrono::steady_clock;
    using SystemClock = std::chrono::system_clock;
    using TimePoint = SteadyClock::time_point;

    [[nodiscard]] static TimePoint Now() noexcept {
        return SteadyClock::now();
    }

    [[nodiscard]] static f64 Seconds() noexcept {
        return ToSeconds(Now().time_since_epoch());
    }

    [[nodiscard]] static u64 Milliseconds() noexcept {
        return ToMilliseconds(Now().time_since_epoch());
    }

    [[nodiscard]] static u64 Microseconds() noexcept {
        return ToMicroseconds(Now().time_since_epoch());
    }

    [[nodiscard]] static u64 Nanoseconds() noexcept {
        return ToNanoseconds(Now().time_since_epoch());
    }

    template <typename Duration>
    [[nodiscard]] static f64 ToSeconds(Duration duration) noexcept {
        return std::chrono::duration<f64>(duration).count();
    }

    template <typename Duration>
    [[nodiscard]] static u64 ToMilliseconds(Duration duration) noexcept {
        return static_cast<u64>(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
    }

    template <typename Duration>
    [[nodiscard]] static u64 ToMicroseconds(Duration duration) noexcept {
        return static_cast<u64>(std::chrono::duration_cast<std::chrono::microseconds>(duration).count());
    }

    template <typename Duration>
    [[nodiscard]] static u64 ToNanoseconds(Duration duration) noexcept {
        return static_cast<u64>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
    }
};

class Stopwatch {
public:
    Stopwatch() = default;

    void Start() noexcept;
    void Stop() noexcept;
    void Reset() noexcept;
    void Restart() noexcept;

    [[nodiscard]] bool Running() const noexcept;

    [[nodiscard]] f64 ElapsedSeconds() const noexcept;
    [[nodiscard]] u64 ElapsedMilliseconds() const noexcept;
    [[nodiscard]] u64 ElapsedMicroseconds() const noexcept;
    [[nodiscard]] u64 ElapsedNanoseconds() const noexcept;

private:
    [[nodiscard]] Clock::SteadyClock::duration ElapsedDuration() const noexcept;

    Clock::TimePoint start_time_{};
    Clock::SteadyClock::duration accumulated_{};
    bool running_ = false;
};

class FrameTimer {
public:
    FrameTimer() noexcept;

    void Tick() noexcept;
    void Reset() noexcept;

    [[nodiscard]] f64 DeltaSeconds() const noexcept;
    [[nodiscard]] f64 TotalSeconds() const noexcept;
    [[nodiscard]] u64 FrameCount() const noexcept;

private:
    Clock::TimePoint start_time_{};
    Clock::TimePoint last_tick_;
    f64 delta_seconds_ = 0.0;
    f64 total_seconds_ = 0.0;
    u64 frame_count_ = 0;
};

inline void Stopwatch::Start() noexcept {
    if (running_) {
        return;
    }

    start_time_ = Clock::Now();
    running_ = true;
}

inline void Stopwatch::Stop() noexcept {
    if (!running_) {
        return;
    }

    accumulated_ += Clock::Now() - start_time_;
    running_ = false;
}

inline void Stopwatch::Reset() noexcept {
    accumulated_ = Clock::SteadyClock::duration::zero();
    start_time_ = Clock::TimePoint{};
    running_ = false;
}

inline void Stopwatch::Restart() noexcept {
    accumulated_ = Clock::SteadyClock::duration::zero();
    start_time_ = Clock::Now();
    running_ = true;
}

inline bool Stopwatch::Running() const noexcept {
    return running_;
}

inline Clock::SteadyClock::duration Stopwatch::ElapsedDuration() const noexcept {
    if (!running_) {
        return accumulated_;
    }

    return accumulated_ + (Clock::Now() - start_time_);
}

inline f64 Stopwatch::ElapsedSeconds() const noexcept {
    return Clock::ToSeconds(ElapsedDuration());
}

inline u64 Stopwatch::ElapsedMilliseconds() const noexcept {
    return Clock::ToMilliseconds(ElapsedDuration());
}

inline u64 Stopwatch::ElapsedMicroseconds() const noexcept {
    return Clock::ToMicroseconds(ElapsedDuration());
}

inline u64 Stopwatch::ElapsedNanoseconds() const noexcept {
    return Clock::ToNanoseconds(ElapsedDuration());
}

inline FrameTimer::FrameTimer() noexcept
    : start_time_(Clock::Now()),
      last_tick_(start_time_) {}

inline void FrameTimer::Tick() noexcept {
    const auto now = Clock::Now();
    delta_seconds_ = Clock::ToSeconds(now - last_tick_);
    total_seconds_ = Clock::ToSeconds(now - start_time_);
    last_tick_ = now;
    ++frame_count_;
}

inline void FrameTimer::Reset() noexcept {
    start_time_ = Clock::Now();
    last_tick_ = start_time_;
    delta_seconds_ = 0.0;
    total_seconds_ = 0.0;
    frame_count_ = 0;
}

inline f64 FrameTimer::DeltaSeconds() const noexcept {
    return delta_seconds_;
}

inline f64 FrameTimer::TotalSeconds() const noexcept {
    return total_seconds_;
}

inline u64 FrameTimer::FrameCount() const noexcept {
    return frame_count_;
}

} // namespace woki
