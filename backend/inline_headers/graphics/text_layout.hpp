// =========================================================================
// text_layout.hpp : turns a string plus a Ttf::Font into a finished RGBA bitmap
// =========================================================================
// Handles UTF-8, kerning, '\n' and '\t', optional word wrapping to a maximum
// width, left/center/right alignment, and an optional outline (stroke). Output
// is AARRGGBB, top row first, straight (non-premultiplied) alpha, which is what
// the engine's image registry stores.
//
// The text comes from a script, so it is untrusted: invalid UTF-8 never reads
// past the end, a missing glyph draws the font's "missing glyph" box instead of
// nothing, and the output size is capped.
// =========================================================================
#ifndef TEXT_LAYOUT_HPP
#define TEXT_LAYOUT_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include "ttf_font.hpp"

namespace TextLayout {

static constexpr int kMaxOutputEdge = 8192;
static constexpr size_t kMaxTextBytes = 1u << 20;

// Decodes one code point starting at s[i], advancing i. Invalid sequences yield U+FFFD and consume one byte.
inline uint32_t NextCodepoint(const std::string& s, size_t& i) {
    const uint8_t c = (uint8_t)s[i++];
    if (c < 0x80) return c;
    int extra; uint32_t cp;
    if ((c & 0xE0) == 0xC0) { extra = 1; cp = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0) { extra = 2; cp = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { extra = 3; cp = c & 0x07; }
    else return 0xFFFD;
    if (i + (size_t)extra > s.size()) return 0xFFFD;
    for (int k = 0; k < extra; ++k) {
        const uint8_t n = (uint8_t)s[i + (size_t)k];
        if ((n & 0xC0) != 0x80) return 0xFFFD;
        cp = (cp << 6) | (n & 0x3F);
    }
    i += (size_t)extra;
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return 0xFFFD;
    if ((extra == 1 && cp < 0x80) || (extra == 2 && cp < 0x800) || (extra == 3 && cp < 0x10000)) return 0xFFFD;    // overlong encodings
    return cp;
}

enum class Align { Left, Center, Right };

struct Options {
    float size = 16.0f;            // em height in pixels
    uint32_t color = 0xFFFFFFFFu;  // AARRGGBB fill
    bool stroke = false;
    uint32_t strokeColor = 0xFF000000u;
    float strokeWidth = 1.0f;      // pixels
    Align align = Align::Left;
    float maxWidth = 0.0f;         // > 0 wraps lines that would be wider; 0 = no wrapping
    float lineSpacing = 1.0f;      // multiplier on the font's natural line height
    int tabSpaces = 4;
};

struct Placed { int glyph; float x; };                 // pen position of a glyph on its line
struct Line { std::vector<Placed> glyphs; float width = 0; };

struct Layout {
    std::vector<Line> lines;
    float width = 0, height = 0;                       // pixels, before the stroke margin
    float lineHeight = 0, ascent = 0;
};

inline Layout Measure(Ttf::Font& font, const std::string& textIn, const Options& opt) {
    Layout out;
    const std::string text = textIn.size() > kMaxTextBytes ? textIn.substr(0, kMaxTextBytes) : textIn;
    const Ttf::Metrics m = font.GetMetrics(opt.size);
    out.lineHeight = std::max(1.0f, m.lineHeight * opt.lineSpacing); out.ascent = m.ascent;
    const int spaceGlyph = font.GlyphIndex(' ');
    const float spaceAdv = font.Advance(spaceGlyph, opt.size);

    Line line; float pen = 0; int prev = -1;
    // word-wrap bookkeeping: where the current word began on the line
    size_t wordStart = 0; float wordPen = 0; bool inWord = false;
    auto endLine = [&]() { line.width = pen; out.width = std::max(out.width, pen); out.lines.push_back(std::move(line)); line = Line(); pen = 0; prev = -1; wordStart = 0; wordPen = 0; inWord = false; };

    for (size_t i = 0; i < text.size();) {
        const uint32_t cp = NextCodepoint(text, i);
        if (cp == '\r') continue;
        if (cp == '\n') { endLine(); continue; }
        if (cp == '\t') { const float tab = spaceAdv * (float)std::max(1, opt.tabSpaces); pen = (std::floor(pen / tab) + 1.0f) * tab; prev = -1; inWord = false; continue; }
        int g = font.GlyphIndex(cp);
        if (cp != ' ' && cp < 0x20) continue;                          // other control characters draw nothing
        const bool isSpace = (cp == ' ');
        if (!isSpace && !inWord) { inWord = true; wordStart = line.glyphs.size(); wordPen = pen; }
        if (isSpace) inWord = false;
        const float kern = prev >= 0 ? font.Kerning(prev, g, opt.size) : 0.0f;
        const float adv = font.Advance(g, opt.size);
        // wrap: this glyph would pass the limit and there is something on the line already
        if (opt.maxWidth > 0 && pen + kern + adv > opt.maxWidth && !line.glyphs.empty() && !isSpace) {
            if (inWord && wordStart > 0) {
                // move the unfinished word to the next line
                Line next; const float shift = line.glyphs[wordStart].x;
                for (size_t k = wordStart; k < line.glyphs.size(); ++k) next.glyphs.push_back({line.glyphs[k].glyph, line.glyphs[k].x - shift});
                line.glyphs.resize(wordStart);
                // trim trailing spaces from the finished line
                pen = wordPen; while (!line.glyphs.empty() && line.glyphs.back().glyph == spaceGlyph) { pen = line.glyphs.back().x; line.glyphs.pop_back(); }
                line.width = pen; out.width = std::max(out.width, pen); out.lines.push_back(std::move(line));
                line = std::move(next);
                pen = line.glyphs.empty() ? 0.0f : line.glyphs.back().x + font.Advance(line.glyphs.back().glyph, opt.size);       // pen after the carried word
                wordStart = 0; wordPen = 0; prev = line.glyphs.empty() ? -1 : line.glyphs.back().glyph;
            } else endLine();
        }
        if (isSpace && line.glyphs.empty() && opt.maxWidth > 0 && !out.lines.empty() && pen == 0) { prev = -1; continue; }      // no leading space on a wrapped line
        pen += (line.glyphs.empty() ? 0.0f : kern);
        line.glyphs.push_back({g, pen}); pen += adv; prev = g;
        if (line.glyphs.size() > 200000) break;
    }
    line.width = pen; out.width = std::max(out.width, pen);
    if (!line.glyphs.empty() || out.lines.empty() || !text.empty()) out.lines.push_back(std::move(line));
    out.height = out.lineHeight * (float)out.lines.size();
    return out;
}

// Renders text to an AARRGGBB bitmap. Returns false (and leaves outputs untouched) if the text is empty or too large.
inline bool Render(Ttf::Font& font, const std::string& text, const Options& opt, std::vector<uint32_t>& pixels, int& outW, int& outH) {
    const Layout lay = Measure(font, text, opt);
    if (lay.lines.empty() || lay.width <= 0.0f) return false;
    const int margin = opt.stroke ? (int)std::ceil(opt.strokeWidth) + 1 : 1;
    const float boxW = opt.maxWidth > 0 ? std::max(lay.width, 0.0f) : lay.width;
    const int W = (int)std::ceil(boxW) + 2 * margin + 1, H = (int)std::ceil(lay.height) + 2 * margin + 1;
    if (W <= 0 || H <= 0 || W > kMaxOutputEdge || H > kMaxOutputEdge || (long long)W * H > (1LL << 26)) return false;
    pixels.assign((size_t)W * (size_t)H, 0u);
    auto blend = [&](int x, int y, uint32_t src, uint8_t cov) {
        if (x < 0 || y < 0 || x >= W || y >= H || cov == 0) return;
        const uint32_t sa = (((src >> 24) & 255) * cov + 127) / 255; if (sa == 0) return;
        uint32_t& d = pixels[(size_t)y * W + x]; const uint32_t da = d >> 24;
        const uint32_t oa = sa + (da * (255 - sa) + 127) / 255; if (oa == 0) return;
        auto ch = [&](int sh) { const uint32_t s = (src >> sh) & 255, dd = (d >> sh) & 255; return (s * sa * 255 + dd * da * (255 - sa) + (oa * 255) / 2) / (oa * 255); };
        d = (oa << 24) | (std::min(255u, ch(16)) << 16) | (std::min(255u, ch(8)) << 8) | std::min(255u, ch(0));
    };
    for (int pass = opt.stroke ? 0 : 1; pass < 2; ++pass) {
        const uint32_t col = pass == 0 ? opt.strokeColor : opt.color;
        const int reps = pass == 0 ? 16 : 1;                           // the stroke is the glyph stamped around a ring
        for (int rep = 0; rep < reps; ++rep) {
            float ox = 0, oy = 0;
            if (pass == 0) { const float a = 6.2831853f * (float)rep / (float)reps; ox = std::cos(a) * opt.strokeWidth; oy = std::sin(a) * opt.strokeWidth; }
            for (size_t li = 0; li < lay.lines.size(); ++li) {
                const Line& ln = lay.lines[li];
                float shift = 0;
                if (opt.align == Align::Center) shift = (boxW - ln.width) * 0.5f; else if (opt.align == Align::Right) shift = boxW - ln.width;
                const float baseline = (float)margin + lay.ascent + lay.lineHeight * (float)li;
                for (const Placed& p : ln.glyphs) {
                    const Ttf::Glyph& gl = font.GetGlyph(p.glyph, opt.size);
                    if (gl.width <= 0) continue;
                    const float gx = (float)margin + shift + p.x + ox, gy = baseline + oy;
                    // glyph bitmaps sit on whole pixels; the fractional pen position is rounded (kept simple and crisp)
                    const int px = (int)std::lround(gx) + gl.left, py = (int)std::lround(gy) - gl.top;
                    for (int y = 0; y < gl.height; ++y) for (int x = 0; x < gl.width; ++x) blend(px + x, py + y, col, gl.coverage[(size_t)y * gl.width + x]);
                }
            }
        }
    }
    outW = W; outH = H;
    return true;
}

} // namespace TextLayout
#endif
