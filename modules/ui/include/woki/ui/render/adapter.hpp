#pragma once

#include <optional>
#include <string_view>
#include <thread>

#include <woki/gfx/canvas.hpp>
#include <woki/ui/display.hpp>

namespace woki::ui::render {

class ImageResolver {
public:
    virtual ~ImageResolver() = default;
    [[nodiscard]] virtual std::optional<woki::gfx::CanvasImageSource> Resolve(ImageId image) const = 0;
};

struct GlyphQuad final {
    u32 glyph{};
    u32 page{};
    Rect bounds;
    Rect uv;
    f32 advance{};
    f32 range{};
    woki::gfx::CanvasImageSource image;
};

// Stable shaping/atlas seam. The built-in provider is intentionally simple;
// FreeType/HarfBuzz/MSDF providers can implement this interface without
// changing UI display lists or the physical canvas renderer.
class FontProvider {
public:
    virtual ~FontProvider() = default;
    virtual void Shape(std::string_view utf8, f32 size, std::vector<GlyphQuad>& output) = 0;
};

class BuiltinFontProvider final : public FontProvider {
public:
    void Shape(std::string_view utf8, f32 size, std::vector<GlyphQuad>& output) override;
};

class Adapter final {
public:
    struct BorrowedServices final {
        const ImageResolver& images;
        FontProvider& fonts;
    };

    explicit Adapter(BorrowedServices services);
    Adapter(const Adapter&) = delete;
    Adapter& operator=(const Adapter&) = delete;
    Adapter(Adapter&&) = delete;
    Adapter& operator=(Adapter&&) = delete;
    [[nodiscard]] woki::gfx::CanvasFrame Convert(
        const DisplayList& display,
        woki::gfx::SurfaceHandle surface,
        u32 width,
        u32 height,
        f32 content_scale = 1.0f
    ) const;

private:
    const ImageResolver* images_{};
    FontProvider* glyphs_{};
    std::thread::id owner_;
};

} // namespace woki::ui::render
