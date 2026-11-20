// =========================================================================
// ttf_font.hpp
// =========================================================================
// A small, dependency-free TrueType reader and rasteriser. It turns a .ttf
// file into smooth anti-aliased glyph bitmaps at any pixel size, with no
// FreeType, no stb, and no system font API, so it behaves the same on every
// OS and works on targets that have none of those.
//
// WHAT IT READS      TrueType outlines (glyf/loca), cmap formats 4 and 12,
//                    hmtx advances, kern (format 0) pair kerning, and
//                    composite glyphs. That covers ordinary .ttf files.
// WHAT IT DOES NOT   OpenType/CFF (.otf with PostScript outlines), font
//                    collections (.ttc), hinting instructions, GPOS/GSUB
//                    shaping, colour/emoji fonts, or right-to-left and
//                    complex scripts. Load() says so instead of drawing junk.
//
// A FONT FILE IS UNTRUSTED INPUT. Fonts are a classic attack surface, so
// every offset, length, count and index read from the file is checked
// against the real file size BEFORE it is used, recursion is bounded,
// outline and bitmap sizes are capped, and a bad file makes Load() return
// false with a message. Nothing here can read or write outside its buffers.
//
// OWNERSHIP. A Font owns a copy of the file bytes and every cache; it frees
// them in its destructor. Glyph bitmaps are returned by const reference into
// the cache and stay valid until the Font is destroyed or ClearCache() runs.
// =========================================================================
#ifndef TTF_FONT_HPP
#define TTF_FONT_HPP

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

namespace Ttf {

// Hard limits. A real font is far inside all of these; a hostile one is not.
static constexpr size_t   kMaxFileBytes      = 64u * 1024u * 1024u;
static constexpr int      kMaxGlyphs         = 65535;
static constexpr int      kMaxPoints         = 8192;      // points in one glyph outline
static constexpr int      kMaxContours       = 512;
static constexpr int      kMaxComposite      = 8;         // nesting depth of composite glyphs
static constexpr int      kMaxComponents     = 64;
static constexpr float    kMaxPixelSize      = 1024.0f;
static constexpr int      kMaxBitmapEdge     = 2048;

// A rendered glyph: an 8-bit coverage mask (0 = nothing, 255 = fully covered).
struct Glyph {
    int width = 0, height = 0;     // bitmap size in pixels (0 for blank glyphs like space)
    int left = 0;                  // x offset of the bitmap's left edge from the pen position
    int top = 0;                   // y offset of the bitmap's TOP edge above the baseline (positive = up)
    float advance = 0.0f;          // how far the pen moves, in pixels
    std::vector<uint8_t> coverage; // width * height, top row first
};

struct Metrics {
    float ascent = 0, descent = 0, lineGap = 0;   // pixels; descent is negative
    float lineHeight = 0;                          // ascent - descent + lineGap
};

class Font {
public:
    Font() = default;
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    // Parses `data` (copied). Returns false and sets `err` if it is not a usable TrueType font.
    bool Load(const uint8_t* data, size_t size, std::string& err) {
        Reset();
        if (!data || size < 12) { err = "file is too small to be a font"; return false; }
        if (size > kMaxFileBytes) { err = "font file is too large"; return false; }
        bytes_.assign(data, data + size);
        if (!Parse(err)) { Reset(); return false; }
        return true;
    }
    bool Loaded() const { return !bytes_.empty() && unitsPerEm_ > 0; }
    const std::string& Name() const { return name_; }
    int GlyphCount() const { return numGlyphs_; }

    // Glyph index for a Unicode code point, or 0 (the "missing glyph") if the font has none.
    int GlyphIndex(uint32_t codepoint) const { return Loaded() ? Lookup(codepoint) : 0; }
    bool HasGlyph(uint32_t codepoint) const { return Loaded() && Lookup(codepoint) != 0; }

    Metrics GetMetrics(float pixelSize) const {
        Metrics m; if (!Loaded()) return m;
        const float s = Scale(pixelSize);
        m.ascent = ascender_ * s; m.descent = descender_ * s; m.lineGap = lineGap_ * s;
        m.lineHeight = m.ascent - m.descent + m.lineGap;
        return m;
    }

    // Distance the pen moves after `glyph`, in pixels.
    float Advance(int glyph, float pixelSize) const { return Loaded() ? AdvanceUnits(glyph) * Scale(pixelSize) : 0.0f; }
    // Extra distance between two glyphs from the font's kerning table (usually negative or zero), in pixels.
    float Kerning(int left, int right, float pixelSize) const {
        if (!Loaded() || kernPairs_.empty()) return 0.0f;
        auto it = kernPairs_.find(((uint32_t)left << 16) | (uint32_t)right);
        return it == kernPairs_.end() ? 0.0f : (float)it->second * Scale(pixelSize);
    }

    // Rasterises a glyph at `pixelSize` (the em height in pixels). The result is cached; the reference stays
    // valid until the Font dies or ClearCache(). `subpixelX` (0..1) shifts the outline right for sharper spacing.
    const Glyph& GetGlyph(int glyph, float pixelSize) {
        static const Glyph kEmpty;
        if (!Loaded() || glyph < 0 || glyph >= numGlyphs_) return kEmpty;
        pixelSize = std::max(1.0f, std::min(kMaxPixelSize, pixelSize));
        const uint64_t key = ((uint64_t)(uint32_t)glyph << 32) | (uint64_t)(uint32_t)std::lround(pixelSize * 16.0f);
        auto it = cache_.find(key);
        if (it != cache_.end()) return it->second;
        Glyph g; Rasterize(glyph, pixelSize, g);
        if (cache_.size() > 8192) cache_.clear();            // bounded memory: start over rather than grow without limit
        return cache_.emplace(key, std::move(g)).first->second;
    }
    void ClearCache() { cache_.clear(); }
    size_t CachedGlyphs() const { return cache_.size(); }

private:
    // ---------------------------------------------------------------- reading
    struct Table { uint32_t offset = 0, length = 0; bool present = false; };
    std::vector<uint8_t> bytes_;
    Table head_, maxp_, hhea_, hmtx_, cmap_, loca_, glyf_, kern_, name_t_;
    int unitsPerEm_ = 0, numGlyphs_ = 0, locaFormat_ = 0, numHMetrics_ = 0;
    float ascender_ = 0, descender_ = 0, lineGap_ = 0;
    uint32_t cmapOffset_ = 0; int cmapFormat_ = 0;
    std::unordered_map<uint32_t, int16_t> kernPairs_;
    std::unordered_map<uint64_t, Glyph> cache_;
    std::string name_;

    void Reset() {
        bytes_.clear(); head_ = maxp_ = hhea_ = hmtx_ = cmap_ = loca_ = glyf_ = kern_ = name_t_ = Table();
        unitsPerEm_ = numGlyphs_ = locaFormat_ = numHMetrics_ = 0; ascender_ = descender_ = lineGap_ = 0;
        cmapOffset_ = 0; cmapFormat_ = 0; kernPairs_.clear(); cache_.clear(); name_.clear();
    }
    float Scale(float pixelSize) const { return pixelSize / (float)unitsPerEm_; }

    // Bounds-checked big-endian reads. Out of range reads return 0 and set bad_; callers check bad_ where it matters.
    mutable bool bad_ = false;
    uint8_t  U8 (size_t o) const { if (o + 1 > bytes_.size()) { bad_ = true; return 0; } return bytes_[o]; }
    uint16_t U16(size_t o) const { if (o + 2 > bytes_.size()) { bad_ = true; return 0; } return (uint16_t)((bytes_[o] << 8) | bytes_[o + 1]); }
    int16_t  S16(size_t o) const { return (int16_t)U16(o); }
    uint32_t U32(size_t o) const { if (o + 4 > bytes_.size()) { bad_ = true; return 0; } return ((uint32_t)bytes_[o] << 24) | ((uint32_t)bytes_[o + 1] << 16) | ((uint32_t)bytes_[o + 2] << 8) | bytes_[o + 3]; }
    bool InTable(const Table& t, size_t rel, size_t len) const { return t.present && rel <= t.length && len <= t.length - rel; }

    bool Parse(std::string& err) {
        bad_ = false;
        const uint32_t tag = U32(0);
        if (tag == 0x4F54544F) { err = "this is an OpenType/CFF font (.otf); only TrueType outlines (.ttf) are supported"; return false; }
        if (tag == 0x74746366) { err = "font collections (.ttc) are not supported; extract one font"; return false; }
        if (tag != 0x00010000 && tag != 0x74727565 /*'true'*/) { err = "not a TrueType font file"; return false; }
        const int numTables = U16(4);
        if (numTables <= 0 || numTables > 64 || 12 + (size_t)numTables * 16 > bytes_.size()) { err = "damaged table directory"; return false; }
        for (int i = 0; i < numTables; ++i) {
            const size_t rec = 12 + (size_t)i * 16;
            const uint32_t t = U32(rec), off = U32(rec + 8), len = U32(rec + 12);
            if ((size_t)off > bytes_.size() || (size_t)len > bytes_.size() - off) continue;      // skip a table that does not fit in the file
            Table tb; tb.offset = off; tb.length = len; tb.present = true;
            switch (t) {
                case 0x68656164: head_ = tb; break;  case 0x6D617870: maxp_ = tb; break; case 0x68686561: hhea_ = tb; break;
                case 0x686D7478: hmtx_ = tb; break;  case 0x636D6170: cmap_ = tb; break; case 0x6C6F6361: loca_ = tb; break;
                case 0x676C7966: glyf_ = tb; break;  case 0x6B65726E: kern_ = tb; break; case 0x6E616D65: name_t_ = tb; break;
                default: break;
            }
        }
        if (!head_.present || !maxp_.present || !hhea_.present || !hmtx_.present || !cmap_.present) { err = "font is missing required tables"; return false; }
        if (!loca_.present || !glyf_.present) { err = "font has no TrueType outlines (glyf/loca)"; return false; }
        if (head_.length < 54 || maxp_.length < 6 || hhea_.length < 36) { err = "font tables are truncated"; return false; }

        unitsPerEm_ = U16(head_.offset + 18);
        if (unitsPerEm_ < 16 || unitsPerEm_ > 16384) { err = "unreasonable units-per-em"; return false; }
        locaFormat_ = S16(head_.offset + 50);
        if (locaFormat_ != 0 && locaFormat_ != 1) { err = "unknown loca format"; return false; }
        numGlyphs_ = U16(maxp_.offset + 4);
        if (numGlyphs_ <= 0 || numGlyphs_ > kMaxGlyphs) { err = "unreasonable glyph count"; return false; }
        const size_t locaNeeded = (size_t)(numGlyphs_ + 1) * (locaFormat_ ? 4 : 2);
        if (loca_.length < locaNeeded) { err = "glyph location table is truncated"; return false; }

        ascender_ = (float)S16(hhea_.offset + 4); descender_ = (float)S16(hhea_.offset + 6); lineGap_ = (float)S16(hhea_.offset + 8);
        numHMetrics_ = U16(hhea_.offset + 34);
        if (numHMetrics_ <= 0 || numHMetrics_ > numGlyphs_ || hmtx_.length < (size_t)numHMetrics_ * 4) { err = "horizontal metrics are damaged"; return false; }

        if (!ChooseCmap()) { err = "font has no usable Unicode character map"; return false; }
        ParseKern();
        ParseName();
        if (bad_) { err = "font data runs past the end of the file"; return false; }
        return true;
    }

    bool ChooseCmap() {
        if (cmap_.length < 4) return false;
        const int n = U16(cmap_.offset + 2);
        if (n <= 0 || n > 64 || (size_t)4 + (size_t)n * 8 > cmap_.length) return false;
        int bestScore = -1;
        for (int i = 0; i < n; ++i) {
            const size_t rec = cmap_.offset + 4 + (size_t)i * 8;
            const int plat = U16(rec), enc = U16(rec + 2); const uint32_t off = U32(rec + 4);
            if (off + 6 > cmap_.length) continue;
            const int fmt = U16(cmap_.offset + off);
            if (fmt != 4 && fmt != 12) continue;
            int score = -1;
            if (plat == 3 && enc == 10) score = 4;            // Windows, full Unicode
            else if (plat == 0) score = 3;                    // Unicode platform
            else if (plat == 3 && enc == 1) score = 2;        // Windows, BMP
            if (score > bestScore) { bestScore = score; cmapOffset_ = cmap_.offset + off; cmapFormat_ = fmt; }
        }
        return bestScore >= 0;
    }

    int Lookup(uint32_t cp) const {
        if (cmapFormat_ == 4) {
            if (cp > 0xFFFF) return 0;
            const size_t base = cmapOffset_;
            const int segX2 = U16(base + 6);
            if (segX2 <= 0 || segX2 % 2 || base + 16 + (size_t)segX2 * 4 > bytes_.size()) { bad_ = false; return 0; }
            const int segCount = segX2 / 2;
            const size_t endCodes = base + 14, startCodes = endCodes + segX2 + 2, deltas = startCodes + segX2, rangeOffs = deltas + segX2;
            int lo = 0, hi = segCount - 1;
            while (lo <= hi) {                                                // segments are sorted by end code
                const int mid = (lo + hi) / 2;
                const uint32_t end = U16(endCodes + (size_t)mid * 2);
                if (cp > end) { lo = mid + 1; continue; }
                const uint32_t start = U16(startCodes + (size_t)mid * 2);
                if (cp < start) { hi = mid - 1; continue; }
                const int delta = S16(deltas + (size_t)mid * 2); const int ro = U16(rangeOffs + (size_t)mid * 2);
                int g;
                if (ro == 0) g = (int)((cp + delta) & 0xFFFF);
                else {
                    const size_t gp = rangeOffs + (size_t)mid * 2 + ro + (size_t)(cp - start) * 2;
                    if (gp + 2 > bytes_.size()) { bad_ = false; return 0; }
                    g = U16(gp); if (g) g = (g + delta) & 0xFFFF;
                }
                return g < numGlyphs_ ? g : 0;
            }
            return 0;
        }
        if (cmapFormat_ == 12) {
            const size_t base = cmapOffset_; const uint32_t n = U32(base + 12);
            if (n > 1000000 || base + 16 + (size_t)n * 12 > bytes_.size()) { bad_ = false; return 0; }
            int64_t lo = 0, hi = (int64_t)n - 1;
            while (lo <= hi) {
                const int64_t mid = (lo + hi) / 2; const size_t g = base + 16 + (size_t)mid * 12;
                const uint32_t start = U32(g), end = U32(g + 4);
                if (cp > end) lo = mid + 1; else if (cp < start) hi = mid - 1;
                else { const uint32_t id = U32(g + 8) + (cp - start); return id < (uint32_t)numGlyphs_ ? (int)id : 0; }
            }
        }
        return 0;
    }

    void ParseKern() {
        if (!kern_.present || kern_.length < 4) return;
        const int nTables = U16(kern_.offset + 2);
        size_t pos = kern_.offset + 4;
        for (int t = 0; t < nTables && t < 8; ++t) {
            if (pos + 6 > kern_.offset + kern_.length) return;
            const int length = U16(pos + 2), coverage = U16(pos + 4);
            const size_t end = std::min<size_t>(pos + (length ? length : 0), kern_.offset + kern_.length);
            if ((coverage >> 8) == 0 && (coverage & 1) && pos + 14 <= end) {         // format 0, horizontal
                const int nPairs = U16(pos + 6);
                size_t p = pos + 14;
                for (int i = 0; i < nPairs && p + 6 <= end && kernPairs_.size() < 200000; ++i, p += 6) {
                    const uint32_t l = U16(p), r = U16(p + 2); const int16_t v = S16(p + 4);
                    if (l < (uint32_t)numGlyphs_ && r < (uint32_t)numGlyphs_ && v != 0) kernPairs_[(l << 16) | r] = v;
                }
            }
            if (length == 0) return;
            pos += (size_t)length;
        }
    }

    void ParseName() {
        if (!name_t_.present || name_t_.length < 6) return;
        const int count = U16(name_t_.offset + 2); const size_t strBase = name_t_.offset + U16(name_t_.offset + 4);
        for (int i = 0; i < count && i < 256; ++i) {
            const size_t rec = name_t_.offset + 6 + (size_t)i * 12;
            if (rec + 12 > name_t_.offset + name_t_.length) return;
            const int plat = U16(rec), nameId = U16(rec + 6), len = U16(rec + 8); const size_t off = strBase + U16(rec + 10);
            if (nameId != 4 || len <= 0 || len > 256 || off + len > bytes_.size()) continue;      // 4 = full font name
            std::string s;
            if (plat == 3 || plat == 0) { for (int k = 0; k + 1 < len; k += 2) { const uint16_t c = (uint16_t)((bytes_[off + k] << 8) | bytes_[off + k + 1]); if (c >= 32 && c < 127) s += (char)c; } }
            else for (int k = 0; k < len; ++k) { const uint8_t c = bytes_[off + k]; if (c >= 32 && c < 127) s += (char)c; }
            if (!s.empty()) { name_ = s; return; }
        }
    }

    float AdvanceUnits(int glyph) const {
        if (glyph < 0 || glyph >= numGlyphs_) return 0;
        const int idx = glyph < numHMetrics_ ? glyph : numHMetrics_ - 1;       // glyphs past the table reuse the last advance
        return (float)U16(hmtx_.offset + (size_t)idx * 4);
    }

    // ---------------------------------------------------------------- outlines
    struct Pt { float x, y; bool on; };
    // Collects the contours of `glyph` (font units, y up) into `pts` / `ends`. Returns false if the glyph is damaged.
    bool Outline(int glyph, std::vector<Pt>& pts, std::vector<int>& ends, int depth, float a = 1, float b = 0, float c = 0, float d = 1, float dx = 0, float dy = 0) const {
        if (depth > kMaxComposite) return false;
        if (glyph < 0 || glyph >= numGlyphs_) return false;
        size_t o0, o1;
        if (locaFormat_ == 0) { o0 = (size_t)U16(loca_.offset + (size_t)glyph * 2) * 2; o1 = (size_t)U16(loca_.offset + (size_t)glyph * 2 + 2) * 2; }
        else                  { o0 = U32(loca_.offset + (size_t)glyph * 4);              o1 = U32(loca_.offset + (size_t)glyph * 4 + 4); }
        if (o1 < o0 || o1 > glyf_.length) return false;
        if (o1 == o0) return true;                                           // an empty glyph (space) is valid
        if (o1 - o0 < 10) return false;
        const size_t g = glyf_.offset + o0, gEnd = glyf_.offset + o1;
        const int nContours = S16(g);
        auto xf = [&](float x, float y, Pt& p, bool on) { p.x = a * x + c * y + dx; p.y = b * x + d * y + dy; p.on = on; };

        if (nContours >= 0) {
            if (nContours > kMaxContours) return false;
            size_t p = g + 10;
            if (p + (size_t)nContours * 2 + 2 > gEnd) return false;
            std::vector<int> localEnds((size_t)nContours);
            int nPts = 0;
            for (int i = 0; i < nContours; ++i) { localEnds[(size_t)i] = U16(p + (size_t)i * 2); if (i && localEnds[(size_t)i] < localEnds[(size_t)i - 1]) return false; }
            if (nContours) nPts = localEnds.back() + 1;
            if (nPts < 0 || nPts > kMaxPoints) return false;
            p += (size_t)nContours * 2; const int insLen = U16(p); p += 2 + (size_t)insLen;
            if (p > gEnd) return false;
            std::vector<uint8_t> flags((size_t)nPts);
            for (int i = 0; i < nPts;) {
                if (p >= gEnd) return false;
                const uint8_t f = bytes_[p++]; flags[(size_t)i++] = f;
                if (f & 8) { if (p >= gEnd) return false; int rep = bytes_[p++]; if (i + rep > nPts) return false; while (rep--) flags[(size_t)i++] = f; }
            }
            std::vector<int> xs((size_t)nPts), ys((size_t)nPts);
            int v = 0;
            for (int i = 0; i < nPts; ++i) {
                const uint8_t f = flags[(size_t)i];
                if (f & 2) { if (p >= gEnd) return false; const int dv = bytes_[p++]; v += (f & 16) ? dv : -dv; }
                else if (!(f & 16)) { if (p + 2 > gEnd) return false; v += S16(p); p += 2; }
                xs[(size_t)i] = v;
            }
            v = 0;
            for (int i = 0; i < nPts; ++i) {
                const uint8_t f = flags[(size_t)i];
                if (f & 4) { if (p >= gEnd) return false; const int dv = bytes_[p++]; v += (f & 32) ? dv : -dv; }
                else if (!(f & 32)) { if (p + 2 > gEnd) return false; v += S16(p); p += 2; }
                ys[(size_t)i] = v;
            }
            if (pts.size() + (size_t)nPts > (size_t)kMaxPoints * 4) return false;
            const int base = (int)pts.size();
            for (int i = 0; i < nPts; ++i) { Pt q; xf((float)xs[(size_t)i], (float)ys[(size_t)i], q, (flags[(size_t)i] & 1) != 0); pts.push_back(q); }
            for (int e : localEnds) ends.push_back(base + e);
            return !bad_;
        }

        // composite glyph: a list of other glyphs, each placed with an offset and an optional scale/rotation
        size_t p = g + 10; int comps = 0; uint16_t flags;
        do {
            if (++comps > kMaxComponents || p + 4 > gEnd) return false;
            flags = U16(p); const int sub = U16(p + 2); p += 4;
            float ox, oy;
            if (flags & 1) { if (p + 4 > gEnd) return false; ox = (float)((flags & 2) ? S16(p) : (int)U16(p)); oy = (float)((flags & 2) ? S16(p + 2) : (int)U16(p + 2)); p += 4; }
            else           { if (p + 2 > gEnd) return false; ox = (float)((flags & 2) ? (int8_t)U8(p) : (int)U8(p)); oy = (float)((flags & 2) ? (int8_t)U8(p + 1) : (int)U8(p + 1)); p += 2; }
            float ma = 1, mb = 0, mc = 0, md = 1;
            auto F2 = [&](size_t q) { return (float)S16(q) / 16384.0f; };
            if (flags & 8)       { if (p + 2 > gEnd) return false; ma = md = F2(p); p += 2; }
            else if (flags & 0x40) { if (p + 4 > gEnd) return false; ma = F2(p); md = F2(p + 2); p += 4; }
            else if (flags & 0x80) { if (p + 8 > gEnd) return false; ma = F2(p); mb = F2(p + 2); mc = F2(p + 4); md = F2(p + 6); p += 8; }
            if (!(flags & 2)) { ox = 0; oy = 0; }                              // point-matching placement is not supported; use the origin
            // combine this component's matrix with the parent transform
            const float na = a * ma + c * mb, nb = b * ma + d * mb, nc = a * mc + c * md, nd = b * mc + d * md;
            const float ndx = a * ox + c * oy + dx, ndy = b * ox + d * oy + dy;
            if (!Outline(sub, pts, ends, depth + 1, na, nb, nc, nd, ndx, ndy)) return false;
        } while (flags & 0x20);
        return !bad_;
    }

    // ---------------------------------------------------------------- flattening
    struct Seg { float x0, y0, x1, y1; };
    static void Quad(std::vector<Seg>& out, float x0, float y0, float cx, float cy, float x1, float y1, float tol) {
        // number of line pieces from the curve's deviation from its chord
        const float dev = std::fabs(x0 - 2 * cx + x1) + std::fabs(y0 - 2 * cy + y1);
        int n = (int)std::ceil(std::sqrt(dev * 0.25f / std::max(tol, 1e-4f)));
        n = std::max(1, std::min(n, 32));
        float px = x0, py = y0;
        for (int i = 1; i <= n; ++i) {
            const float t = (float)i / (float)n, u = 1 - t;
            const float x = u * u * x0 + 2 * u * t * cx + t * t * x1, y = u * u * y0 + 2 * u * t * cy + t * t * y1;
            out.push_back({px, py, x, y}); px = x; py = y;
        }
    }

    void Flatten(const std::vector<Pt>& pts, const std::vector<int>& ends, float scale, std::vector<Seg>& segs) const {
        int start = 0;
        for (int e : ends) {
            const int n = e - start + 1;
            if (n >= 2) {
                auto P = [&](int i) { const Pt& q = pts[(size_t)(start + ((i % n) + n) % n)]; return Pt{q.x * scale, q.y * scale, q.on}; };
                // find a starting on-curve point (or the midpoint of two off-curve points)
                int first = -1; for (int i = 0; i < n; ++i) if (P(i).on) { first = i; break; }
                Pt sp; int begin;
                if (first >= 0) { sp = P(first); begin = first + 1; }
                else { const Pt a = P(0), b = P(n - 1); sp = Pt{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, true}; begin = 0; }
                Pt cur = sp; bool havePending = false; Pt pending{0, 0, false};
                const int steps = (first >= 0) ? n : n + 1;
                for (int k = 0; k < steps; ++k) {
                    Pt q = (k == steps - 1 && first >= 0) ? sp : (first >= 0 ? P(begin + k) : (k == steps - 1 ? sp : P(begin + k)));
                    if (first >= 0 && k == n - 1) q = sp;
                    if (q.on) {
                        if (havePending) { Quad(segs, cur.x, cur.y, pending.x, pending.y, q.x, q.y, 0.1f); havePending = false; }
                        else segs.push_back({cur.x, cur.y, q.x, q.y});
                        cur = q;
                    } else {
                        if (havePending) {
                            const Pt mid{(pending.x + q.x) * 0.5f, (pending.y + q.y) * 0.5f, true};
                            Quad(segs, cur.x, cur.y, pending.x, pending.y, mid.x, mid.y, 0.1f); cur = mid;
                        }
                        pending = q; havePending = true;
                    }
                }
                if (havePending) Quad(segs, cur.x, cur.y, pending.x, pending.y, sp.x, sp.y, 0.1f);
                else if (std::fabs(cur.x - sp.x) > 1e-6f || std::fabs(cur.y - sp.y) > 1e-6f) segs.push_back({cur.x, cur.y, sp.x, sp.y});
            }
            start = e + 1;
        }
    }

    // ---------------------------------------------------------------- rasteriser
    // Exact-area coverage: for each segment, add its signed area contribution to a per-pixel accumulator, then a
    // running sum along each row turns that into coverage. Smooth edges with no supersampling, non-zero winding.
    void Rasterize(int glyph, float pixelSize, Glyph& out) {
        out = Glyph();
        const float scale = Scale(pixelSize);
        out.advance = AdvanceUnits(glyph) * scale;
        std::vector<Pt> pts; std::vector<int> ends;
        bad_ = false;
        if (!Outline(glyph, pts, ends, 0) || pts.empty()) { bad_ = false; return; }          // blank or damaged: draws nothing
        std::vector<Seg> segs; segs.reserve(pts.size() * 3);
        Flatten(pts, ends, scale, segs);
        if (segs.empty()) return;
        float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
        for (const Seg& s : segs) { minX = std::min({minX, s.x0, s.x1}); maxX = std::max({maxX, s.x0, s.x1}); minY = std::min({minY, s.y0, s.y1}); maxY = std::max({maxY, s.y0, s.y1}); }
        if (!(maxX > minX) || !(maxY > minY) || !std::isfinite(minX + maxX + minY + maxY)) return;
        const int x0 = (int)std::floor(minX) - 1, y0 = (int)std::floor(minY) - 1;
        const int w = (int)std::ceil(maxX) + 1 - x0 + 1, h = (int)std::ceil(maxY) + 1 - y0 + 1;
        if (w <= 0 || h <= 0 || w > kMaxBitmapEdge || h > kMaxBitmapEdge) return;
        std::vector<float> area((size_t)w * (size_t)h, 0.0f), cover((size_t)w * (size_t)h, 0.0f);
        for (const Seg& s : segs) {
            // y is flipped: font space is y-up, bitmaps are top-down
            float ax = s.x0 - (float)x0, ay = (float)(y0 + h) - s.y0 - (float)y0, bx = s.x1 - (float)x0, by = (float)(y0 + h) - s.y1 - (float)y0;
            ay = (float)h - (s.y0 - (float)y0); by = (float)h - (s.y1 - (float)y0);
            AddEdge(area, cover, w, h, ax, ay, bx, by);
        }
        out.width = w; out.height = h; out.left = x0; out.top = y0 + h;       // top edge above the baseline = (y0 + h) in y-up pixels
        out.coverage.assign((size_t)w * (size_t)h, 0);
        for (int y = 0; y < h; ++y) {
            float acc = 0;
            for (int x = 0; x < w; ++x) {
                acc += cover[(size_t)y * w + x];
                const float v = std::fabs(acc + area[(size_t)y * w + x]);
                out.coverage[(size_t)y * w + x] = (uint8_t)std::min(255, (int)(v * 255.0f + 0.5f));
            }
        }
    }

    // Adds one edge's contribution to the accumulation buffers.
    //
    // CONVENTION. The caller turns the buffers into coverage with a running sum along each row:
    //     acc += cover[x];   coverage(x) = |acc + area[x]|
    // For an edge slice of signed height d inside one row, crossing pixel i with mean x position xm (0..1) inside it:
    //   pixels left of the edge get 0,   pixels right of the edge get d,   pixel i itself gets d * (1 - xm).
    // That is produced by   cover[i] += d   (so every pixel from i rightwards sums to d)
    //                  and  area[i]  -= d * xm   (so pixel i itself lands on d - d*xm = d*(1 - xm)).
    // A slice that spans several pixels is the same thing split per pixel, each piece using its own height share.
    static void AddEdge(std::vector<float>& area, std::vector<float>& cover, int w, int h, float x0, float y0, float x1, float y1) {
        if (y0 == y1) return;
        float dir = 1.0f;
        if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); dir = -1.0f; }
        const float dxdy = (x1 - x0) / (y1 - y0);
        const int yStart = std::max(0, (int)std::floor(y0)), yEnd = std::min(h - 1, (int)std::ceil(y1) - 1);
        float x = (y0 < 0) ? x0 - y0 * dxdy : x0;               // x where the edge enters the first visible row
        // A piece of the edge of signed height `d` whose mean x is `xmean` (absolute, in pixels).
        auto piece = [&](int y, float d, float xmean) {
            int xi = (int)std::floor(xmean);
            float frac = xmean - (float)xi;
            if (xi < 0) { xi = 0; frac = 0.0f; }                // left of the bitmap: everything to the right is covered
            if (xi >= w) return;                                // right of the bitmap: contributes nothing inside it
            cover[(size_t)y * w + xi] += d;
            area[(size_t)y * w + xi] -= d * frac;
        };
        for (int y = yStart; y <= yEnd; ++y) {
            const float dy = std::min((float)(y + 1), y1) - std::max((float)y, y0);
            const float xnext = x + dxdy * dy;
            const float d = dy * dir;
            const float xa = std::min(x, xnext), xb = std::max(x, xnext);
            const int ia = (int)std::floor(xa), ib = (int)std::floor(xb);
            if (ia == ib) {
                piece(y, d, 0.5f * (xa + xb));
            } else {
                // The slice crosses pixels ia..ib. Split its height by how much of its horizontal travel falls in each
                // pixel, and treat each piece as a small edge at that piece's mean x.
                const float travel = xb - xa;
                for (int xi = ia; xi <= ib; ++xi) {
                    const float lo = std::max(xa, (float)xi), hi = std::min(xb, (float)(xi + 1));
                    if (hi <= lo) continue;
                    piece(y, d * (hi - lo) / travel, 0.5f * (lo + hi));
                }
            }
            x = xnext;
        }
    }
};

} // namespace Ttf
#endif // TTF_FONT_HPP
