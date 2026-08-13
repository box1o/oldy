#include <catch2/catch_test_macros.hpp>

#include <woki/ui.hpp>

using namespace woki::ui;

TEST_CASE("theme parses JSONC semantic tokens") {
    auto theme = Theme::Parse(R"({
        "$schema":"https://schemas.woki.dev/ui.theme/v1.schema.json",
        "name":"test",
        // flat application theme
        "color": { "primary": "#3366CCFF" },
        "space": { "md": 8 },
        "radius": { "md": 6 },
        "type": { "body": { "family": "Inter", "size": 14, "weight": 500 } },
        "motion": { "fast": { "duration": 90, "curve": "out" } }
    })");

    REQUIRE(theme);
    CHECK(theme->ColorOf("primary").b > 0.7f);
    CHECK(theme->Space("md") == 8);
    CHECK(theme->Type("body").weight == 500);
    CHECK(theme->MotionOf("fast").duration.count() == 90);
}

TEST_CASE("failed transactional reload preserves active theme") {
    ThemeStore store;
    REQUIRE(store.Reload(
        R"({"$schema":"https://schemas.woki.dev/ui.theme/v1.schema.json","name":"test","color":{"primary":"#112233"}})"
    ));
    const auto revision = store.Revision();
    const Color before = store.Current().ColorOf("primary");

    CHECK_FALSE(store.Reload("{"));
    CHECK(store.Revision() == revision);
    CHECK(store.Current().ColorOf("primary") == before);
}

TEST_CASE("default theme provides complete flat widget tokens") {
    const Theme& theme = Theme::Default();

    CHECK(theme.ColorOf("primary").a == 1.0f);
    CHECK(theme.ColorOf("card").a == 1.0f);
    CHECK(theme.ColorOf("border").a == 1.0f);
    CHECK(theme.Space("panel") > 0.0f);
    CHECK(theme.RadiusOf("control") > 0.0f);
    CHECK(theme.MotionOf("fast").duration.count() > 0);
}

TEST_CASE("partial themes inherit default tokens") {
    const auto theme = Theme::Parse(
        R"({"$schema":"https://schemas.woki.dev/ui.theme/v1.schema.json","name":"test","color":{"primary":"#FF0000"}})"
    );
    REQUIRE(theme);

    CHECK(theme->ColorOf("primary") == Color::rgba(1, 0, 0));
    CHECK(theme->ColorOf("card") == Theme::Default().ColorOf("card"));
    CHECK(theme->Space("panel") == Theme::Default().Space("panel"));
}
