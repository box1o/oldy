#pragma once

#include <string_view>

#include "woki/events/base.hpp"

namespace woki::events {

struct AppTickEvent final : TypedEvent<EventType::kAppTick, EventCategory::kApplication> {
    f32 delta_time{0.0f};

    explicit AppTickEvent(f32 delta_time_value)
        : delta_time(delta_time_value) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "AppTick";
    }
};

struct AppUpdateEvent final : TypedEvent<EventType::kAppUpdate, EventCategory::kApplication> {
    f32 delta_time{0.0f};

    explicit AppUpdateEvent(f32 delta_time_value)
        : delta_time(delta_time_value) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "AppUpdate";
    }
};

struct AppRenderEvent final : TypedEvent<EventType::kAppRender, EventCategory::kApplication> {
    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "AppRender";
    }
};

struct AppShutdownEvent final : TypedEvent<EventType::kAppShutdown, EventCategory::kApplication> {
    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "AppShutdown";
    }
};

struct AppSuspendEvent final : TypedEvent<EventType::kAppSuspend, EventCategory::kApplication> {
    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "AppSuspend";
    }
};

struct AppResumeEvent final : TypedEvent<EventType::kAppResume, EventCategory::kApplication> {
    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "AppResume";
    }
};

} // namespace woki::events
