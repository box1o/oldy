#pragma once

#include <utility>
#include <concepts>
#include <functional>
#include <type_traits>

#include "woki/events/base.hpp"

namespace woki::events {

template <std::derived_from<Event> E>
class EventDispatcher {
public:
    explicit EventDispatcher(E& event)
        : event_(event) {}

    template <typename T, typename Fn>
    requires EventLike<T> && std::derived_from<T, Event> && std::invocable<Fn, const T&>
    bool Dispatch(Fn&& fn) {
        if (event_.GetEventType() != T::GetStaticType() || event_.handled) {
            return false;
        }

        const auto& typed_event = static_cast<const T&>(event_);
        if constexpr (std::same_as<std::invoke_result_t<Fn, const T&>, bool>) {
            event_.handled = std::invoke(std::forward<Fn>(fn), typed_event);
        } else {
            std::invoke(std::forward<Fn>(fn), typed_event);
        }

        return true;
    }

private:
    E& event_;
};

} // namespace woki::events
