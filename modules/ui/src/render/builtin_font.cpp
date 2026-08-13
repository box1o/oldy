#include <algorithm>

#include <woki/ui/render/adapter.hpp>

namespace woki::ui::render {
namespace {

constexpr u32 kReplacement = '?';

u32 Decode(std::string_view text, size_t& offset) {
    const u8 lead = static_cast<u8>(text[offset++]);
    if (lead < 0x80)
        return lead;
    const u32 count = lead >= 0xf0 ? 3 : lead >= 0xe0 ? 2 : lead >= 0xc2 ? 1 : 0;
    if (count == 0 || offset + count > text.size())
        return kReplacement;
    u32 value = lead & (count == 1 ? 0x1fU : count == 2 ? 0x0fU : 0x07U);
    for (u32 index = 0; index < count; ++index) {
        const u8 next = static_cast<u8>(text[offset]);
        if ((next & 0xc0U) != 0x80U)
            return kReplacement;
        ++offset;
        value = (value << 6U) | (next & 0x3fU);
    }
    if (value < 0x20 || value > 0x7e)
        return kReplacement;
    return value;
}

} // namespace

void BuiltinFontProvider::Shape(const std::string_view utf8, const f32 size, std::vector<GlyphQuad>& output) {
    const f32 height = std::max(size, 1.0f);
    const f32 cell = height * 0.625f;
    for (size_t offset = 0; offset < utf8.size();) {
        const u32 codepoint = Decode(utf8, offset);
        const u32 glyph = std::clamp(codepoint, 0x20U, 0x7eU);
        const u32 cell_index = glyph - 0x20U;
        const f32 advance = glyph == ' ' ? height * 0.375f : cell;
        if (glyph != ' ') {
            const u32 column = cell_index % 16U;
            const u32 row = cell_index / 16U;
            output.push_back(
                {glyph,
                    0,
                    {0, -height * 0.8f, cell, height},
                    {(static_cast<f32>(column * 8U) + 0.5f) / 128.0f,
                        (static_cast<f32>(row * 12U) + 0.5f) / 72.0f,
                        7.0f / 128.0f,
                        11.0f / 72.0f},
                    advance,
                    1.0f,
                    {}}
            );
        }
    }
}

} // namespace woki::ui::render
