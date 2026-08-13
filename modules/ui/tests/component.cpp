#include <catch2/catch_test_macros.hpp>

#include <woki/ui.hpp>

using namespace woki::ui;

namespace {

class Counter final : public Component {
public:
    void Increment() {
        ++value_;
        Invalidate();
    }

    View Build() override {
        return Text(std::to_string(value_)).Id(Key{9});
    }

private:
    int value_{};
};

} // namespace

TEST_CASE("invalidated component reconciles only its retained subtree") {
    auto counter = std::make_shared<Counter>();
    Runtime runtime;
    runtime.SetContent(Row(Mount(counter).Id(Key{5}), Text("stable").Id(Key{6})));
    runtime.Prepare({.viewport = {200, 50}});

    const Element* host = runtime.Root()->Children()[0].get();
    const Element* stable = runtime.Root()->Children()[1].get();
    const Element* content = host->Children()[0].get();
    counter->Increment();
    runtime.Prepare({.viewport = {200, 50}});

    CHECK(runtime.Root()->Children()[0].get() == host);
    CHECK(runtime.Root()->Children()[1].get() == stable);
    CHECK(host->Children()[0].get() == content);
    CHECK(host->Children()[0]->Content() == "1");
}
