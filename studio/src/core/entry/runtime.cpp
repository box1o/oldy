#include <woki/core.hpp>

#include "entry.hpp"
#include "settings.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace studio {

static int RunApplication(woki::scope<woki::Application> application) {
#ifdef __EMSCRIPTEN__
    struct MainLoopState {
        woki::scope<woki::Application> application;
    };

    auto* state = new MainLoopState{std::move(application)};
    emscripten_set_main_loop_arg(
        [](void* user_data) {
            auto* loop = static_cast<MainLoopState*>(user_data);
            if (loop->application->Tick())
                return;
            loop->application.reset();
            emscripten_cancel_main_loop();
            delete loop;
        },
        state, 0, true
    );
    return 0;
#else
    while (application->Tick()) {
    }
    return 0;
#endif
}

int Run(int argc, char* argv[]) {
    return RunApplication(woki::createScope<woki::Application>(LoadSettings(argc, argv)));
}

} // namespace studio
