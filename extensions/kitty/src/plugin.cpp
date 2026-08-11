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
