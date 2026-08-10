#pragma once

// Test-only construction hook for exercising the host facade with fake engines.

#include "../manager.hpp"
#include "../runtime.hpp"

namespace woki::ext::internal {

struct ExtensionManagerAccess {
    [[nodiscard]] static ExtensionManager Create(scope<RuntimeEngine> engine) {
        return ExtensionManager(std::move(engine));
    }
};

} // namespace woki::ext::internal
