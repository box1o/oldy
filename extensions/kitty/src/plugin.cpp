#include <woki/extension.hpp>
#include <woki/ecs/guest.hpp>
#include <woki/math/guest.hpp>

namespace {

using namespace woki;
using namespace math;

class Kitty final {
public:
    Status OnAttach() noexcept {
        // nf-md-cat U+F011B
        entity_ = registry_.Create();
        if (!entity_ || positions_.Emplace(entity_, Position{}) == nullptr)
            return Status::NoSpace();
        return slog::Info("\xF3\xB0\x84\x9B kitty says hi!");
    }

    void OnUpdate(f64 delta_ms) noexcept {
        if (Position* position = positions_.Get(entity_))
            position->value.x += static_cast<float>(delta_ms * 0.001);
    }

    void OnEvent(events::Event& event) noexcept {
        events::EventDispatcher dispatcher{event};
        dispatcher.Dispatch<events::KeyPressedEvent>([](events::KeyPressedEvent key) noexcept { slog::Info("kitty saw key press: ", key.key, " repeat ", key.repeat_count); });
        dispatcher.Dispatch<events::WindowResizedEvent>([](events::WindowResizedEvent resized) noexcept { slog::Info("kitty saw window resize: ", resized.width, 'x', resized.height); });
        dispatcher.Dispatch<events::WindowMovedEvent>([](events::WindowMovedEvent moved) noexcept { slog::Info("kitty saw window move: ", moved.x, 'x', moved.y); });
        dispatcher.Dispatch<events::WindowClosedEvent>([](events::WindowClosedEvent) noexcept { slog::Info("kitty saw window close"); });
        dispatcher.Dispatch<events::WindowMinimizedEvent>([](events::WindowMinimizedEvent) noexcept { slog::Info("kitty saw window minimize"); });
        dispatcher.Dispatch<events::WindowMaximizedEvent>([](events::WindowMaximizedEvent) noexcept { slog::Info("kitty saw window maximize"); });
        dispatcher.Dispatch<events::WindowRestoredEvent>([](events::WindowRestoredEvent) noexcept { slog::Info("kitty saw window restore"); });
    }

    Status OnCommand(const extension::Command& command) noexcept {
        if (command.Id() == "woki.kitty.pet")
            return slog::Info("\xF3\xB0\x84\x9B *purrr*");
        if (command.Id() == "woki.kitty.complex")
            return slog::Info("\xF3\xB0\x84\x9B complex command!");
        return Status::NotFound();
    }

    void OnDetach() noexcept {
        (void)positions_.Remove(entity_);
        (void)registry_.Destroy(entity_);
    }

private:
    struct Position final {
        vec2<float> value{};
    };

    guest::Registry<8> registry_;
    guest::ComponentPool<Position, 8> positions_;
    guest::Entity entity_;
};

} // namespace

WOKI_EXTENSION(Kitty)
