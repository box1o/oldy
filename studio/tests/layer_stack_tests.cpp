#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>

#include <woki/events/window/events.hpp>

#include "core/layer_stack.hpp"

namespace {

class RecordingLayer final : public woki::Layer {
public:
    RecordingLayer(std::string name, std::vector<std::string>& calls, bool handles = false)
        : name_(std::move(name)),
          calls_(calls),
          handles_(handles) {}

    void OnEvent(woki::Context&, woki::events::Event& event) override {
        calls_.push_back(name_ + ":event");
        event.handled = handles_;
    }

    void ObserveEvent(woki::Context&, const woki::events::Event& event) override {
        calls_.push_back(name_ + (event.handled ? ":observed-handled" : ":observed"));
    }

private:
    std::string name_;
    std::vector<std::string>& calls_;
    bool handles_{};
};

} // namespace

TEST_CASE("LayerStack observers receive handled events after normal dispatch") {
    std::vector<std::string> calls;
    woki::LayerStack stack;
    stack.PushLayer(woki::createScope<RecordingLayer>("layer", calls));
    stack.PushOverlay(woki::createScope<RecordingLayer>("overlay", calls, true));

    woki::Context context;
    woki::events::WindowCloseEvent event;
    stack.DispatchEvent(context, event);

    CHECK(calls == std::vector<std::string>{"overlay:event", "layer:observed-handled", "overlay:observed-handled"});
}
