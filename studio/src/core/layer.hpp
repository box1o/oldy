#pragma once

#include "context.hpp"

namespace woki {

class Layer {
public:
    virtual ~Layer() = default;

    Layer(const Layer&) = delete;
    Layer& operator=(const Layer&) = delete;
    Layer(Layer&&) = delete;
    Layer& operator=(Layer&&) = delete;

    virtual void OnAttach(Context& ctx) {
        (void)ctx;
    }

    virtual void OnDetach(Context& ctx) {
        (void)ctx;
    }

    virtual void OnUpdate(Context& ctx, f64 delta_ms) {
        (void)ctx;
        (void)delta_ms;
    }

    virtual void OnEvent(Context& ctx, events::Event& event) {
        (void)ctx;
        (void)event;
    }

    virtual void ObserveEvent(Context& ctx, const events::Event& event) {
        (void)ctx;
        (void)event;
    }

    virtual void OnUi(Context& ctx) {
        (void)ctx;
    }

protected:
    Layer() = default;
};

} // namespace woki
