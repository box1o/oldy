#include <woki/ext/plugin.hpp>

namespace {

using namespace woki::ext;

class Kitty final : public Plugin {
public:
    Status OnLoad(Context& context) noexcept {
        // nf-md-cat U+F011B
        const Status logged = context.GetLog().Info("\xF3\xB0\x84\x9B kitty says hi!");
        if (!logged)
            return logged;
        const Status resized = context.GetEvents().Subscribe<WindowResizedEvent>();
        return resized ? context.GetEvents().Subscribe<KeyPressedEvent>() : resized;
    }

    void OnEvent(Context& context, Event& event) noexcept {
        event.Dispatch<KeyPressedEvent>([&](KeyPressedEvent key) noexcept { (void)context.GetLog().Info("kitty saw key press: ", key.key, " repeat ", key.repeat_count); });
        event.Dispatch<WindowResizedEvent>([&](WindowResizedEvent resized) noexcept { (void)context.GetLog().Info("kitty saw window resize: ", resized.width, 'x', resized.height); });
    }

    Status OnCommand(Context& context, StringView command, Bytes) noexcept {
        if (command == "woki.kitty.pet")
            return context.GetLog().Info("\xF3\xB0\x84\x9B *purrr*");
        if (command == "woki.kitty.complex")
            return context.GetLog().Info("\xF3\xB0\x84\x9B complex command!");
        return Status::NotFound();
    }
};

} // namespace

WOKI_PLUGIN(Kitty)
