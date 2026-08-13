#include <optional>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <woki/ui/render.hpp>

namespace {

class Images final : public woki::ui::render::ImageResolver {
public:
    std::optional<woki::gfx::CanvasImageSource> Resolve(woki::ui::ImageId image) const override {
        ++calls;
        if (image.value == 7)
            return woki::gfx::TextureHandle::Create(3, 2);
        return std::nullopt;
    }

    mutable int calls{};
};

class Font final : public woki::ui::render::FontProvider {
public:
    void Shape(std::string_view text, woki::f32 size, std::vector<woki::ui::render::GlyphQuad>& output) override {
        shaped.assign(text);
        output.push_back(
            {65,
                2,
                {0, -size, size / 2, size},
                {0.25f, 0.5f, 0.1f, 0.2f},
                size * 0.6f,
                3.0f,
                woki::gfx::TextureHandle::Create(8, 1)}
        );
        output.push_back(
            {66,
                2,
                {0, -size, size / 2, size},
                {0.35f, 0.5f, 0.1f, 0.2f},
                size * 0.6f,
                3.0f,
                woki::gfx::TextureHandle::Create(8, 1)}
        );
    }

    std::string shaped;
};

} // namespace

TEST_CASE("adapter tessellates display operations and preserves shader parameters") {
    using namespace woki;
    ui::DisplayList display;
    display.Add(ui::RectOp{{1, 2, 3, 4}, ui::Color::rgba(1, 0, 0)});
    display.Add(ui::RoundOp{{5, 6, 7, 8}, {1, 2, 3, 4}, ui::Color::rgba(0, 1, 0)});
    display.Add(ui::BorderOp{{9, 10, 11, 12}, ui::Radius::All(5), {ui::Color::rgba(0, 0, 1), 2}});
    display.Add(ui::ShadowOp{{20, 20, 10, 10}, ui::Radius::All(3), {ui::Color::rgba(0, 0, 0, 0.5f), {2, 1}, 4, 2}});

    Images images;
    Font font;
    ui::render::Adapter adapter({images, font});
    const auto frame = adapter.Convert(display, gfx::SurfaceHandle::Create(0, 1), 100, 80, 2);

    REQUIRE(frame.vertices.size() == 16);
    REQUIRE(frame.indices.size() == 24);
    REQUIRE(frame.batches.size() == 4);
    CHECK(frame.batches[0].primitive == gfx::CanvasPrimitive::Solid);
    CHECK(frame.batches[1].primitive == gfx::CanvasPrimitive::Rounded);
    CHECK(frame.vertices[4].p0 == 1);
    CHECK(frame.vertices[4].p3 == 4);
    CHECK(frame.batches[2].primitive == gfx::CanvasPrimitive::Border);
    CHECK(frame.vertices[8].q0 == 2);
    CHECK(frame.batches[3].primitive == gfx::CanvasPrimitive::Shadow);
    CHECK(frame.vertices[12].x == 16);
    CHECK(frame.vertices[12].y == 15);
    CHECK(frame.vertices[12].q0 == 4);
    CHECK(frame.vertices[12].q1 == 2);
    CHECK(frame.batches[0].scissor == gfx::CanvasRect{0, 0, 50, 40});
    CHECK(frame.Valid());
}

TEST_CASE("adapter resolves images skips missing resources and forwards fit") {
    using namespace woki;
    ui::DisplayList display;
    display.Add(ui::ImageOp{{0, 0, 30, 20}, ui::ImageId{7}, ui::ImageFit::Cover, ui::Color::rgba(1, 1, 1, 0.5f)});
    display.Add(ui::ImageOp{{30, 0, 30, 20}, ui::ImageId{99}, ui::ImageFit::Fill, ui::Color::rgba(1, 1, 1)});
    Images images;
    Font font;
    ui::render::Adapter adapter({images, font});

    const auto frame = adapter.Convert(display, gfx::SurfaceHandle::Create(0, 1), 60, 20);
    CHECK(images.calls == 2);
    REQUIRE(frame.batches.size() == 1);
    CHECK(frame.batches.front().primitive == gfx::CanvasPrimitive::Image);
    CHECK(frame.batches.front().image == gfx::CanvasImageSource{gfx::TextureHandle::Create(3, 2)});
    CHECK(frame.vertices.front().p0 == static_cast<f32>(ui::ImageFit::Cover));
    CHECK(frame.vertices.front().a == 0.5f);
}

TEST_CASE("nested clips intersect and restore physical canvas scissor") {
    using namespace woki;
    ui::DisplayList display;
    display.Add(ui::ClipOp{{10, 10, 50, 50}, {}});
    display.Add(ui::ClipOp{{30, 0, 50, 30}, {}});
    display.Add(ui::RectOp{{0, 0, 80, 80}, ui::Color::rgba(1, 1, 1)});
    display.Add(ui::PopClip{});
    display.Add(ui::RectOp{{0, 0, 80, 80}, ui::Color::rgba(1, 1, 1)});
    display.Add(ui::PopClip{});
    display.Add(ui::RectOp{{0, 0, 80, 80}, ui::Color::rgba(1, 1, 1)});
    Images images;
    Font font;
    ui::render::Adapter adapter({images, font});

    const auto frame = adapter.Convert(display, gfx::SurfaceHandle::Create(0, 1), 200, 100, 2);
    REQUIRE(frame.batches.size() == 3);
    CHECK(frame.batches[0].scissor == gfx::CanvasRect{30, 10, 30, 20});
    CHECK(frame.batches[1].scissor == gfx::CanvasRect{10, 10, 50, 40});
    CHECK(frame.batches[2].scissor == gfx::CanvasRect{0, 0, 100, 50});
}

TEST_CASE("adjacent compatible quads batch while clip image and primitive changes split") {
    using namespace woki;
    ui::DisplayList display;
    display.Add(ui::RectOp{{0, 0, 10, 10}, ui::Color::rgba(1, 0, 0)});
    display.Add(ui::RectOp{{10, 0, 10, 10}, ui::Color::rgba(0, 1, 0)});
    display.Add(ui::RoundOp{{20, 0, 10, 10}, {}, ui::Color::rgba(0, 0, 1)});
    display.Add(ui::ClipOp{{0, 0, 15, 10}, {}});
    display.Add(ui::RoundOp{{30, 0, 10, 10}, {}, ui::Color::rgba(1, 1, 1)});
    Images images;
    Font font;
    ui::render::Adapter adapter({images, font});

    const auto frame = adapter.Convert(display, gfx::SurfaceHandle::Create(0, 1), 100, 100);
    REQUIRE(frame.batches.size() == 3);
    CHECK(frame.batches[0].index_count == 12);
    CHECK(frame.batches[1].index_count == 6);
    CHECK(frame.batches[2].index_count == 6);
}

TEST_CASE("font provider glyph atlas output becomes positioned glyph batches") {
    using namespace woki;
    ui::DisplayList display;
    display.Add(ui::TextOp{{10, 20}, "AB", ui::Color::rgba(1, 1, 1), 10});
    Images images;
    Font font;
    ui::render::Adapter adapter({images, font});

    const auto frame = adapter.Convert(display, gfx::SurfaceHandle::Create(0, 1), 100, 100);
    CHECK(font.shaped == "AB");
    REQUIRE(frame.vertices.size() == 8);
    REQUIRE(frame.batches.size() == 1);
    CHECK(frame.batches.front().primitive == gfx::CanvasPrimitive::Glyph);
    CHECK(frame.batches.front().index_count == 12);
    CHECK(frame.vertices[0].x == 10);
    CHECK(frame.vertices[0].y == 10);
    CHECK(frame.vertices[4].x == 16);
    CHECK(frame.vertices[0].p0 == 3);
}

TEST_CASE("builtin font emits one replacement glyph for invalid and non-ASCII UTF-8 sequences") {
    using namespace woki;
    ui::render::BuiltinFontProvider font;
    std::vector<ui::render::GlyphQuad> glyphs;
    const std::string text = std::string{"A"} + "\xE2\x82\xAC" + "\xC2" + "Z";
    font.Shape(text, 16, glyphs);

    REQUIRE(glyphs.size() == 4);
    CHECK(glyphs[0].glyph == 'A');
    CHECK(glyphs[1].glyph == '?');
    CHECK(glyphs[2].glyph == '?');
    CHECK(glyphs[3].glyph == 'Z');
    CHECK(glyphs[1].page == 0);
    CHECK(glyphs[1].uv.width > 0);
}

TEST_CASE("converted CanvasFrame owns display-derived values for its full lifetime") {
    using namespace woki;
    gfx::CanvasFrame frame;
    {
        ui::DisplayList display;
        display.Add(ui::RectOp{{1, 2, 3, 4}, ui::Color::rgba(0.1f, 0.2f, 0.3f)});
        Images images;
        Font font;
        ui::render::Adapter adapter({images, font});
        frame = adapter.Convert(display, gfx::SurfaceHandle::Create(4, 1), 20, 30);
        display.Clear();
    }

    REQUIRE(frame.vertices.size() == 4);
    REQUIRE(frame.indices == std::vector<u32>{0, 1, 2, 0, 2, 3});
    CHECK(frame.vertices.front().x == 1);
    CHECK(frame.vertices.front().r == Catch::Approx(0.1f));
    CHECK(frame.Valid());
}
