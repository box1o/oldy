#pragma once

namespace woki {

struct InputCapabilities final {
    bool mouse{false};
    bool touch{false};
    bool pen{false};
    bool pressure{false};
    bool precise_scroll{false};
    bool gestures{false};
    bool gamepads{false};
    bool text_composition{false};
    bool sensors{false};
    bool haptics{false};
};

} // namespace woki
