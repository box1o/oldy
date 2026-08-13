#include <woki/config.hpp>
#include <catch2/catch_test_macros.hpp>

#include <woki/ui.hpp>

using namespace woki::ui;

TEST_CASE("debug export describes retained work counters") {
    Runtime runtime;
    runtime.SetContent(Column(Text("inspect").Id(Key{2})));
    runtime.Prepare({.viewport = {100, 50}});
    const auto tree = woki::config::Json::Parse(
        ExportTree(*runtime.Root()),
        "ui.debug-tree",
        woki::config::ParsePolicy::Strict()
    )
                          .value();

    CHECK(tree["children"][0]["key"] == 2);
    CHECK(tree["children"][0]["token"].get<woki::u64>() != 0);
    CHECK(tree["children"][0]["stats"]["layouts"] == 1);
    CHECK(tree["children"][0]["content"] == "inspect");
}
