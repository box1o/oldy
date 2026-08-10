#include <woki/ext/plugin.hpp>

namespace {

using namespace woki::ext;

[[nodiscard]] bool Equals(StringView value, StringView expected) noexcept {
    if (value.Size() != expected.Size())
        return false;
    for (u32 index = 0; index < value.Size(); ++index)
        if (value.Data()[index] != expected.Data()[index])
            return false;
    return true;
}

u32 AppendNumber(char* output, u32 used, u32 value) noexcept {
    char digits[10];
    u32 count = 0;
    do {
        digits[count++] = static_cast<char>('0' + value % 10u);
        value /= 10u;
    } while (value != 0u);
    while (count != 0u)
        output[used++] = digits[--count];
    return used;
}

class Kitty final : public Plugin {
public:
    Status OnLoad(Context& context) noexcept {
        // nf-md-cat U+F011B
        const Status logged = context.GetLog().Info("\xF3\xB0\x84\x9B kitty says hi!");
        return logged ? context.GetEvents().Subscribe<WindowResizedEvent>() : logged;
    }

    void OnEvent(Context& context, Event& event) noexcept {
        event.Dispatch<WindowResizedEvent>([&](WindowResizedEvent resized) noexcept {
            static constexpr char prefix[] = "kitty saw window resize: ";
            char message[64];
            u32 used = 0;
            while (used < sizeof(prefix) - 1u) {
                message[used] = prefix[used];
                ++used;
            }
            used = AppendNumber(message, used, resized.width);
            message[used++] = 'x';
            used = AppendNumber(message, used, resized.height);
            (void)context.GetLog().Info({message, used});
        });
    }

    Status OnCommand(Context& context, StringView command, Bytes) noexcept {
        if (Equals(command, "woki.kitty.pet"))
            return context.GetLog().Info("\xF3\xB0\x84\x9B *purrr*");
        if (Equals(command, "woki.kitty.complex"))
            return context.GetLog().Info("\xF3\xB0\x84\x9B complex command!");
        return Status::NotFound();
    }
};

} // namespace

WOKI_PLUGIN(Kitty)
