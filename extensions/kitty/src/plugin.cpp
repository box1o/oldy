#include <woki/extension.hpp>
#include <woki/ecs/guest.hpp>
#include <woki/math/guest.hpp>

namespace {

class Kitty final {
public:
    woki::Status OnAttach() noexcept {
        // nf-md-cat U+F011B
        entity_ = registry_.Create();
        if (!entity_ || positions_.Emplace(entity_, Position{}) == nullptr)
            return woki::Status::NoSpace();
        return slog::Info("\xF3\xB0\x84\x9B kitty says hi!");
    }

    void OnUpdate(woki::f64 delta_ms) noexcept {
        if (Position* position = positions_.Get(entity_))
            position->value.x += static_cast<float>(delta_ms * 0.001);
    }

    void OnEvent(woki::events::Event& event) noexcept {
        woki::events::EventDispatcher dispatcher{event};
        dispatcher.Dispatch<woki::events::KeyPressedEvent>([](woki::events::KeyPressedEvent key) noexcept { (void)slog::Info("kitty saw key press: ", key.key, " repeat ", key.repeat_count); });
        dispatcher.Dispatch<woki::events::WindowResizedEvent>([](woki::events::WindowResizedEvent resized) noexcept { (void)slog::Info("kitty saw window resize: ", resized.width, 'x', resized.height); });
    }

    woki::Status OnCommand(const woki::extension::Command& command) noexcept {
        if (command.Id() == "woki.kitty.pet")
            return slog::Info("\xF3\xB0\x84\x9B *purrr*");
        if (command.Id() == "woki.kitty.complex")
            return slog::Info("\xF3\xB0\x84\x9B complex command!");
        return woki::Status::NotFound();
    }

    void OnDetach() noexcept {
        (void)positions_.Remove(entity_);
        (void)registry_.Destroy(entity_);
    }

private:
    struct Position final {
        woki::math::vec2<float> value{};
    };

    woki::guest::Registry<8> registry_;
    woki::guest::ComponentPool<Position, 8> positions_;
    woki::guest::Entity entity_;
};

} // namespace

WOKI_EXTENSION(Kitty)
