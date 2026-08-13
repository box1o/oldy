#include <algorithm>
#include <exception>

#include <woki/ui/render/adapter.hpp>

namespace woki::ui::render {
namespace {

woki::gfx::CanvasRect ToRect(Rect value) {
    return {value.x, value.y, value.width, value.height};
}

woki::gfx::CanvasRect Intersect(woki::gfx::CanvasRect a, woki::gfx::CanvasRect b) {
    const f32 left = std::max(a.x, b.x), top = std::max(a.y, b.y);
    const f32 right = std::min(a.x + a.width, b.x + b.width), bottom = std::min(a.y + a.height, b.y + b.height);
    return {left, top, std::max(0.0f, right - left), std::max(0.0f, bottom - top)};
}

void Quad(
    woki::gfx::CanvasFrame& frame,
    Rect rect,
    Rect uv,
    Color color,
    woki::gfx::CanvasPrimitive primitive,
    const woki::gfx::CanvasImageSource& image,
    woki::gfx::CanvasRect clip,
    f32 p0 = 0,
    f32 p1 = 0,
    f32 p2 = 0,
    f32 p3 = 0,
    f32 q0 = 0,
    f32 q1 = 0
) {
    const u32 base = static_cast<u32>(frame.vertices.size());
    frame.vertices.insert(
        frame.vertices.end(),
        {
            {rect.x, rect.y, uv.x, uv.y, color.r, color.g, color.b, color.a, p0, p1, p2, p3, q0, q1},
            {rect.x + rect.width,
                rect.y,
                uv.x + uv.width,
                uv.y,
                color.r,
                color.g,
                color.b,
                color.a,
                p0,
                p1,
                p2,
                p3,
                q0,
                q1},
            {rect.x + rect.width,
                rect.y + rect.height,
                uv.x + uv.width,
                uv.y + uv.height,
                color.r,
                color.g,
                color.b,
                color.a,
                p0,
                p1,
                p2,
                p3,
                q0,
                q1},
            {rect.x,
                rect.y + rect.height,
                uv.x,
                uv.y + uv.height,
                color.r,
                color.g,
                color.b,
                color.a,
                p0,
                p1,
                p2,
                p3,
                q0,
                q1},
        }
    );
    const u32 first = static_cast<u32>(frame.indices.size());
    frame.indices.insert(frame.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    if (!frame.batches.empty()) {
        auto& previous = frame.batches.back();
        if (previous.primitive == primitive && previous.image == image && previous.scissor == clip
            && previous.first_index + previous.index_count == first) {
            previous.index_count += 6;
            return;
        }
    }
    frame.batches.push_back({first, 6, primitive, image, clip});
}

} // namespace

Adapter::Adapter(const BorrowedServices services)
    : images_(&services.images),
      glyphs_(&services.fonts),
      owner_(std::this_thread::get_id()) {}

woki::gfx::CanvasFrame Adapter::Convert(
    const DisplayList& display,
    woki::gfx::SurfaceHandle surface,
    u32 width,
    u32 height,
    f32 content_scale
) const {
    if (owner_ != std::this_thread::get_id())
        std::terminate();
    woki::gfx::CanvasFrame frame{
        .surface = surface,
        .width = width,
        .height = height,
        .content_scale = content_scale,
        .vertices = {},
        .indices = {},
        .batches = {},
    };
    std::vector<woki::gfx::CanvasRect> clips{
        {0, 0, static_cast<f32>(width) / content_scale, static_cast<f32>(height) / content_scale}};
    for (const auto& operation : display.Operations()) {
        std::visit(
            [&](const auto& op) {
                using T = std::decay_t<decltype(op)>;
                if constexpr (std::same_as<T, ClipOp>) {
                    clips.push_back(Intersect(clips.back(), ToRect(op.rect)));
                } else if constexpr (std::same_as<T, PopClip>) {
                    if (clips.size() > 1)
                        clips.pop_back();
                } else if constexpr (std::same_as<T, RectOp>) {
                    Quad(frame, op.rect, {0, 0, 1, 1}, op.color, woki::gfx::CanvasPrimitive::Solid, {}, clips.back());
                } else if constexpr (std::same_as<T, RoundOp>) {
                    Quad(
                        frame,
                        op.rect,
                        {0, 0, 1, 1},
                        op.color,
                        woki::gfx::CanvasPrimitive::Rounded,
                        {},
                        clips.back(),
                        op.radius.top_left,
                        op.radius.top_right,
                        op.radius.bottom_right,
                        op.radius.bottom_left
                    );
                } else if constexpr (std::same_as<T, BorderOp>) {
                    Quad(
                        frame,
                        op.rect,
                        {0, 0, 1, 1},
                        op.stroke.color,
                        woki::gfx::CanvasPrimitive::Border,
                        {},
                        clips.back(),
                        op.radius.top_left,
                        op.radius.top_right,
                        op.radius.bottom_right,
                        op.radius.bottom_left,
                        op.stroke.width
                    );
                } else if constexpr (std::same_as<T, ShadowOp>) {
                    Rect rect = op.rect;
                    const f32 pad = std::max(0.0f, op.shadow.spread + op.shadow.blur);
                    rect.x += op.shadow.offset.x - pad;
                    rect.y += op.shadow.offset.y - pad;
                    rect.width += pad * 2;
                    rect.height += pad * 2;
                    Quad(
                        frame,
                        rect,
                        {0, 0, 1, 1},
                        op.shadow.color,
                        woki::gfx::CanvasPrimitive::Shadow,
                        {},
                        clips.back(),
                        op.radius.top_left + pad,
                        op.radius.top_right + pad,
                        op.radius.bottom_right + pad,
                        op.radius.bottom_left + pad,
                        op.shadow.blur,
                        op.shadow.spread
                    );
                } else if constexpr (std::same_as<T, ImageOp>) {
                    if (auto image = images_->Resolve(op.image))
                        Quad(
                            frame,
                            op.rect,
                            {0, 0, 1, 1},
                            op.tint,
                            woki::gfx::CanvasPrimitive::Image,
                            *image,
                            clips.back(),
                            static_cast<f32>(op.fit)
                        );
                } else if constexpr (std::same_as<T, TextOp>) {
                    std::vector<GlyphQuad> glyphs;
                    glyphs_->Shape(op.text, op.size, glyphs);
                    f32 pen = op.origin.x;
                    for (const auto& glyph : glyphs) {
                        Rect bounds = glyph.bounds;
                        bounds.x += pen;
                        bounds.y += op.origin.y;
                        Quad(
                            frame,
                            bounds,
                            glyph.uv,
                            op.color,
                            woki::gfx::CanvasPrimitive::Glyph,
                            glyph.image,
                            clips.back(),
                            glyph.range
                        );
                        pen += glyph.advance;
                    }
                }
            },
            operation
        );
    }
    return frame;
}

} // namespace woki::ui::render
