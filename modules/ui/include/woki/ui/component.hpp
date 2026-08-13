#pragma once

#include <atomic>

#include <woki/types/types.hpp>

namespace woki::ui {

class View;

class Component {
public:
    virtual ~Component() = default;

    [[nodiscard]] virtual View Build() = 0;

    [[nodiscard]] u64 Revision() const {
        return revision_.load(std::memory_order_relaxed);
    }

protected:
    void Invalidate() {
        revision_.fetch_add(1, std::memory_order_relaxed);
    }

private:
    std::atomic<u64> revision_{1};
};

} // namespace woki::ui
