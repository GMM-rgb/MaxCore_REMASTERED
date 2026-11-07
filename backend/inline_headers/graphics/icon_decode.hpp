// =========================================================================
// icon_decode.hpp
// =========================================================================
// Small, dependency-free image decoders used to turn an icon file into raw
// pixels: PNG (all colour types and bit depths, interlaced or not, with
// tRNS transparency) and ICO (BMP and PNG payloads). No zlib, no libpng, no
// system calls; it only needs the C++ standard library, so it works on any
// target regardless of what is installed.
//
// Every decoder treats its input as UNTRUSTED: each length is checked
// against what is actually there before it is used, sizes are capped, and a
// bad file returns false with a message instead of crashing. An icon path
// comes from a script, so a corrupt or hostile file must not take the
// process down.
//
// Output is always straight (non-premultiplied) RGBA, 8 bits per channel,
// row-major, top row first.
// =========================================================================
#ifndef ICON_DECODE_HPP
#define ICON_DECODE_HPP

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

namespace IconDecode {

struct Rgba {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels; // width * height * 4, R G B A
};

// Largest icon edge accepted. Real icons are at most 256 (a few 512 / 1024
// for macOS); anything bigger is a mistake or an attack on memory.
static constexpr int kMaxEdge = 2048;

// -------------------------------------------------------------------------
// INFLATE (RFC 1951) and the zlib wrapper (RFC 1950)
// -------------------------------------------------------------------------
class BitReader {
public:
    BitReader(const uint8_t* d, size_t n) : data_(d), size_(n) {}
    // Reads `count` bits (0..24), least significant bit first. Sets failed_
    // and returns 0 when the stream runs out.
    uint32_t bits(int count) {
        while (have_ < count) {
            if (pos_ >= size_) { failed_ = true; return 0; }
            acc_ |= (uint32_t)data_[pos_++] << have_;
            have_ += 8;
        }
        uint32_t v = acc_ & ((count == 32) ? 0xFFFFFFFFu : ((1u << count) - 1u));
        acc_ >>= count;
        have_ -= count;
        return v;
    }
    void alignToByte() { acc_ = 0; have_ = 0; }
    bool failed() const { return failed_; }
    // Raw byte access for stored blocks (call alignToByte first).
    bool bytes(uint8_t* out, size_t n) {
        if (size_ - pos_ < n) { failed_ = true; return false; }
        if (n) std::memcpy(out, data_ + pos_, n);
        pos_ += n;
        return true;
    }
    size_t position() const { return pos_; }
private:
    const uint8_t* data_;
    size_t size_;
    size_t pos_ = 0;
    uint32_t acc_ = 0;
    int have_ = 0;
    bool failed_ = false;
};

// Canonical Huffman table decoded one bit at a time (small and simple; icon
// data is tiny so speed is not a concern).
struct Huffman {
    uint16_t count[16] = {0};
    uint16_t symbol[320] = {0};
    bool build(const uint8_t* lengths, int n) {
        std::memset(count, 0, sizeof(count));
        for (int i = 0; i < n; ++i) ++count[lengths[i]];
        count[0] = 0;
        int left = 1;
        for (int len = 1; len < 16; ++len) {
            left <<= 1;
            left -= count[len];
            if (left < 0) return false; // over-subscribed
        }
        uint16_t offs[16];
        offs[1] = 0;
        for (int len = 1; len < 15; ++len) offs[len + 1] = (uint16_t)(offs[len] + count[len]);
        for (int i = 0; i < n; ++i)
            if (lengths[i] != 0) symbol[offs[lengths[i]]++] = (uint16_t)i;
        return true;
    }
    int decode(BitReader& br) const {
        int code = 0, first = 0, index = 0;
        for (int len = 1; len < 16; ++len) {
            code |= (int)br.bits(1);
            if (br.failed()) return -1;
            int c = count[len];
            if (code - c < first) return symbol[index + (code - first)];
            index += c;
            first += c;
            first <<= 1;
            code <<= 1;
        }
        return -1;
    }
};

// Inflates a raw DEFLATE stream into `out`. `limit` caps the output size so
// a decompression bomb cannot exhaust memory.
inline bool Inflate(const uint8_t* src, size_t srcLen, std::vector<uint8_t>& out, size_t limit) {
    static const uint16_t lenBase[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    static const uint8_t  lenExtra[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    static const uint16_t distBase[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    static const uint8_t  distExtra[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    static const uint8_t  clOrder[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};

    BitReader br(src, srcLen);
    bool last = false;
    while (!last) {
        last = br.bits(1) != 0;
        uint32_t type = br.bits(2);
        if (br.failed()) return false;
        if (type == 0) { // stored
            br.alignToByte();
            uint8_t hdr[4];
            if (!br.bytes(hdr, 4)) return false;
            uint32_t len = hdr[0] | (hdr[1] << 8), nlen = hdr[2] | (hdr[3] << 8);
            if ((len ^ 0xFFFFu) != nlen) return false;
            if (out.size() + len > limit) return false;
            size_t at = out.size();
            out.resize(at + len);
            if (!br.bytes(out.data() + at, len)) return false;
        } else if (type == 1 || type == 2) {
            Huffman lit, dist;
            if (type == 1) { // fixed tables
                uint8_t l[288];
                for (int i = 0; i < 144; ++i) l[i] = 8;
                for (int i = 144; i < 256; ++i) l[i] = 9;
                for (int i = 256; i < 280; ++i) l[i] = 7;
                for (int i = 280; i < 288; ++i) l[i] = 8;
                lit.build(l, 288);
                uint8_t d[30];
                for (int i = 0; i < 30; ++i) d[i] = 5;
                dist.build(d, 30);
            } else { // dynamic tables
                int nlen = (int)br.bits(5) + 257, ndist = (int)br.bits(5) + 1, ncode = (int)br.bits(4) + 4;
                if (br.failed() || nlen > 286 || ndist > 30) return false;
                uint8_t cl[19] = {0};
                for (int i = 0; i < ncode; ++i) cl[clOrder[i]] = (uint8_t)br.bits(3);
                if (br.failed()) return false;
                Huffman clh;
                if (!clh.build(cl, 19)) return false;
                uint8_t lengths[320] = {0};
                int idx = 0;
                while (idx < nlen + ndist) {
                    int sym = clh.decode(br);
                    if (sym < 0) return false;
                    if (sym < 16) { lengths[idx++] = (uint8_t)sym; }
                    else {
                        int rep, val = 0;
                        if (sym == 16) { if (idx == 0) return false; val = lengths[idx - 1]; rep = 3 + (int)br.bits(2); }
                        else if (sym == 17) rep = 3 + (int)br.bits(3);
                        else rep = 11 + (int)br.bits(7);
                        if (br.failed() || idx + rep > nlen + ndist) return false;
                        while (rep--) lengths[idx++] = (uint8_t)val;
                    }
                }
                if (lengths[256] == 0) return false; // no end-of-block code
                if (!lit.build(lengths, nlen)) return false;
                if (!dist.build(lengths + nlen, ndist)) return false;
            }
            for (;;) {
                int sym = lit.decode(br);
                if (sym < 0) return false;
                if (sym < 256) {
                    if (out.size() + 1 > limit) return false;
                    out.push_back((uint8_t)sym);
                } else if (sym == 256) {
                    break;
                } else {
                    sym -= 257;
                    if (sym >= 29) return false;
                    int len = lenBase[sym] + (int)br.bits(lenExtra[sym]);
                    int ds = dist.decode(br);
                    if (ds < 0 || ds >= 30) return false;
                    size_t d = (size_t)distBase[ds] + br.bits(distExtra[ds]);
                    if (br.failed() || d > out.size()) return false;
                    if (out.size() + (size_t)len > limit) return false;
                    size_t from = out.size() - d;
                    for (int i = 0; i < len; ++i) out.push_back(out[from + i]); // may overlap: copy byte by byte
                }
            }
        } else {
            return false; // reserved block type
        }
    }
    return true;
}

// zlib stream = 2 header bytes + DEFLATE + 4 byte Adler-32 (not verified:
// PNG's own CRC already guards the chunk and a bad checksum should not
// reject an otherwise readable icon).
inline bool ZlibInflate(const uint8_t* src, size_t n, std::vector<uint8_t>& out, size_t limit) {
    if (n < 2) return false;
    if ((src[0] & 0x0F) != 8) return false;                 // must be deflate
    if (((src[0] << 8) | src[1]) % 31 != 0) return false;   // header check bits
    if (src[1] & 0x20) return false;                         // preset dictionary not allowed in PNG
    return Inflate(src + 2, n - 2, out, limit);
}

// -------------------------------------------------------------------------
// PNG
// -------------------------------------------------------------------------
inline uint32_t BE32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
inline uint32_t LE32(const uint8_t* p) { return ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | p[0]; }
inline uint16_t LE16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

inline bool IsPng(const uint8_t* d, size_t n) {
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    return n >= 8 && std::memcmp(d, sig, 8) == 0;
}

inline int PaethPredictor(int a, int b, int c) {
    int p = a + b - c;
    int pa = p > a ? p - a : a - p, pb = p > b ? p - b : b - p, pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) return a;
    return pb <= pc ? b : c;
}

inline bool DecodePng(const uint8_t* data, size_t size, Rgba& out, std::string& err) {
    if (!IsPng(data, size)) { err = "not a PNG file"; return false; }
    size_t pos = 8;
    uint32_t width = 0, height = 0;
    int bitDepth = 0, colorType = 0, interlace = 0;
    bool haveHeader = false;
    std::vector<uint8_t> idat;
    uint8_t palette[256][3] = {};
    uint8_t paletteAlpha[256];
    std::memset(paletteAlpha, 255, sizeof(paletteAlpha));
    int paletteCount = 0;
    bool haveTrnsKey = false;
    uint16_t trnsKey[3] = {0, 0, 0};

    while (pos + 8 <= size) {
        uint32_t len = BE32(data + pos);
        const uint8_t* type = data + pos + 4;
        pos += 8;
        if (len > size - pos || size - pos - len < 4) { err = "PNG chunk runs past the end of the file"; return false; }
        const uint8_t* body = data + pos;
        if (!std::memcmp(type, "IHDR", 4)) {
            if (len != 13) { err = "bad PNG header"; return false; }
            width = BE32(body); height = BE32(body + 4);
            bitDepth = body[8]; colorType = body[9];
            interlace = body[12];
            if (body[10] != 0 || body[11] != 0 || interlace > 1) { err = "unsupported PNG compression or filter method"; return false; }
            haveHeader = true;
        } else if (!std::memcmp(type, "PLTE", 4)) {
            if (len % 3 != 0 || len > 768) { err = "bad PNG palette"; return false; }
            paletteCount = (int)(len / 3);
            for (int i = 0; i < paletteCount; ++i) { palette[i][0] = body[i*3]; palette[i][1] = body[i*3+1]; palette[i][2] = body[i*3+2]; }
        } else if (!std::memcmp(type, "tRNS", 4)) {
            if (colorType == 3) { for (uint32_t i = 0; i < len && i < 256; ++i) paletteAlpha[i] = body[i]; }
            else if (colorType == 0 && len >= 2) { haveTrnsKey = true; trnsKey[0] = (uint16_t)((body[0] << 8) | body[1]); }
            else if (colorType == 2 && len >= 6) { haveTrnsKey = true; for (int i = 0; i < 3; ++i) trnsKey[i] = (uint16_t)((body[i*2] << 8) | body[i*2+1]); }
        } else if (!std::memcmp(type, "IDAT", 4)) {
            if (idat.size() + len > (size_t)64 * 1024 * 1024) { err = "PNG image data is too large"; return false; }
            idat.insert(idat.end(), body, body + len);
        } else if (!std::memcmp(type, "IEND", 4)) {
            break;
        }
        pos += len + 4; // body + CRC (not verified, same reasoning as Adler-32)
    }

    if (!haveHeader) { err = "PNG has no header"; return false; }
    if (width == 0 || height == 0 || width > (uint32_t)kMaxEdge || height > (uint32_t)kMaxEdge) { err = "PNG dimensions are zero or too large for an icon"; return false; }
    int channels;
    switch (colorType) {
        case 0: channels = 1; if (bitDepth != 1 && bitDepth != 2 && bitDepth != 4 && bitDepth != 8 && bitDepth != 16) { err = "bad PNG bit depth"; return false; } break;
        case 2: channels = 3; if (bitDepth != 8 && bitDepth != 16) { err = "bad PNG bit depth"; return false; } break;
        case 3: channels = 1; if (bitDepth != 1 && bitDepth != 2 && bitDepth != 4 && bitDepth != 8) { err = "bad PNG bit depth"; return false; } break;
        case 4: channels = 2; if (bitDepth != 8 && bitDepth != 16) { err = "bad PNG bit depth"; return false; } break;
        case 6: channels = 4; if (bitDepth != 8 && bitDepth != 16) { err = "bad PNG bit depth"; return false; } break;
        default: err = "bad PNG color type"; return false;
    }
    if (colorType == 3 && paletteCount == 0) { err = "PNG palette image has no palette"; return false; }

    const int bitsPerPixel = channels * bitDepth;
    const size_t bpp = (size_t)std::max(1, bitsPerPixel / 8); // filter distance in bytes

    // Worst-case raw size: all 7 interlace passes together never exceed this.
    size_t rowBytesFull = ((size_t)width * bitsPerPixel + 7) / 8;
    size_t rawLimit = (rowBytesFull + 1) * height + 7 * (height + 8) + 1024;
    std::vector<uint8_t> raw;
    if (!ZlibInflate(idat.data(), idat.size(), raw, rawLimit)) { err = "PNG image data is corrupt"; return false; }

    out.width = (int)width;
    out.height = (int)height;
    out.pixels.assign((size_t)width * height * 4, 0);

    auto sample = [&](const uint8_t* row, size_t index) -> uint16_t { // index-th sample of a row, any depth
        if (bitDepth == 8) return row[index];
        if (bitDepth == 16) return (uint16_t)((row[index * 2] << 8) | row[index * 2 + 1]);
        size_t bit = index * bitDepth;
        int shift = 8 - bitDepth - (int)(bit % 8);
        return (uint16_t)((row[bit / 8] >> shift) & ((1 << bitDepth) - 1));
    };
    auto to8 = [&](uint16_t v) -> uint8_t {
        if (bitDepth == 8) return (uint8_t)v;
        if (bitDepth == 16) return (uint8_t)(((uint32_t)v * 255u + 32767u) / 65535u); // true rounding, not just the high byte
        return (uint8_t)(v * 255 / ((1 << bitDepth) - 1));
    };
    auto emit = [&](const uint8_t* row, uint32_t px, uint32_t dstX, uint32_t dstY) {
        uint8_t* o = &out.pixels[((size_t)dstY * width + dstX) * 4];
        switch (colorType) {
            case 0: { uint16_t g = sample(row, px); o[0] = o[1] = o[2] = to8(g); o[3] = (haveTrnsKey && g == trnsKey[0]) ? 0 : 255; break; }
            case 2: { uint16_t r = sample(row, px*3), g = sample(row, px*3+1), b = sample(row, px*3+2);
                      o[0] = to8(r); o[1] = to8(g); o[2] = to8(b);
                      o[3] = (haveTrnsKey && r == trnsKey[0] && g == trnsKey[1] && b == trnsKey[2]) ? 0 : 255; break; }
            case 3: { uint16_t i = sample(row, px); if (i >= (uint16_t)paletteCount) { o[0]=o[1]=o[2]=0; o[3]=0; break; }
                      o[0] = palette[i][0]; o[1] = palette[i][1]; o[2] = palette[i][2]; o[3] = paletteAlpha[i]; break; }
            case 4: { uint8_t g = to8(sample(row, px*2)); o[0] = o[1] = o[2] = g; o[3] = to8(sample(row, px*2+1)); break; }
            case 6: { o[0] = to8(sample(row, px*4)); o[1] = to8(sample(row, px*4+1)); o[2] = to8(sample(row, px*4+2)); o[3] = to8(sample(row, px*4+3)); break; }
        }
    };

    // Unfilters one (sub)image of `w` x `h` pixels starting at raw[offset];
    // calls emit for each pixel with the destination chosen by `place`.
    size_t offset = 0;
    auto unfilterPass = [&](uint32_t w, uint32_t h, auto place) -> bool {
        if (w == 0 || h == 0) return true;
        size_t rowBytes = ((size_t)w * bitsPerPixel + 7) / 8;
        std::vector<uint8_t> prev(rowBytes, 0), cur(rowBytes);
        for (uint32_t y = 0; y < h; ++y) {
            if (offset + 1 + rowBytes > raw.size()) return false;
            uint8_t ft = raw[offset++];
            std::memcpy(cur.data(), &raw[offset], rowBytes);
            offset += rowBytes;
            for (size_t i = 0; i < rowBytes; ++i) {
                int a = i >= bpp ? cur[i - bpp] : 0, b = prev[i], c = i >= bpp ? prev[i - bpp] : 0;
                int x = cur[i];
                switch (ft) {
                    case 0: break;
                    case 1: x += a; break;
                    case 2: x += b; break;
                    case 3: x += (a + b) / 2; break;
                    case 4: x += PaethPredictor(a, b, c); break;
                    default: return false;
                }
                cur[i] = (uint8_t)x;
            }
            for (uint32_t x = 0; x < w; ++x) {
                uint32_t dx, dy;
                place(x, y, dx, dy);
                emit(cur.data(), x, dx, dy);
            }
            prev = cur;
        }
        return true;
    };

    bool ok;
    if (interlace == 0) {
        ok = unfilterPass(width, height, [](uint32_t x, uint32_t y, uint32_t& dx, uint32_t& dy) { dx = x; dy = y; });
    } else {
        static const int xs[7] = {0,4,0,2,0,1,0}, ys[7] = {0,0,4,0,2,0,1}, dxs[7] = {8,8,4,4,2,2,1}, dys[7] = {8,8,8,4,4,2,2};
        ok = true;
        for (int p = 0; p < 7 && ok; ++p) {
            uint32_t pw = (width  + dxs[p] - 1 - xs[p]) / dxs[p];
            uint32_t ph = (height + dys[p] - 1 - ys[p]) / dys[p];
            if (width  <= (uint32_t)xs[p]) pw = 0;
            if (height <= (uint32_t)ys[p]) ph = 0;
            ok = unfilterPass(pw, ph, [&](uint32_t x, uint32_t y, uint32_t& dx, uint32_t& dy) { dx = xs[p] + x * dxs[p]; dy = ys[p] + y * dys[p]; });
        }
    }
    if (!ok) { err = "PNG image data is corrupt"; out = Rgba(); return false; }
    return true;
}

// -------------------------------------------------------------------------
// ICO (Windows icon container). Each entry is a PNG or an old-style BMP
// ("DIB") without a file header. All sizes are listed; the caller picks.
// -------------------------------------------------------------------------
struct IcoEntry { int width, height; size_t offset, length; };

inline bool ListIco(const uint8_t* d, size_t n, std::vector<IcoEntry>& entries, std::string& err) {
    if (n < 6 || LE16(d) != 0 || (LE16(d + 2) != 1 && LE16(d + 2) != 2)) { err = "not an ICO file"; return false; }
    int count = LE16(d + 4);
    if (count <= 0 || (size_t)count * 16 + 6 > n) { err = "ICO directory is truncated"; return false; }
    for (int i = 0; i < count; ++i) {
        const uint8_t* e = d + 6 + i * 16;
        IcoEntry en;
        en.width = e[0] ? e[0] : 256;
        en.height = e[1] ? e[1] : 256;
        en.length = LE32(e + 8);
        en.offset = LE32(e + 12);
        if (en.offset > n || en.length > n - en.offset || en.length < 8) continue; // skip a damaged entry, keep the rest
        entries.push_back(en);
    }
    if (entries.empty()) { err = "ICO has no readable images"; return false; }
    return true;
}

inline bool DecodeIcoEntry(const uint8_t* d, size_t n, const IcoEntry& en, Rgba& out, std::string& err) {
    const uint8_t* p = d + en.offset;
    size_t len = en.length;
    (void)n;
    if (IsPng(p, len)) return DecodePng(p, len, out, err);

    // BMP payload: BITMAPINFOHEADER, optional palette, XOR pixels, then a 1-bit AND mask.
    if (len < 40) { err = "ICO image is truncated"; return false; }
    uint32_t hdr = LE32(p);
    if (hdr < 40 || hdr > len) { err = "unsupported ICO image header"; return false; }
    int32_t w = (int32_t)LE32(p + 4);
    int32_t hh = (int32_t)LE32(p + 8);
    int planes = LE16(p + 12), bpp = LE16(p + 14);
    uint32_t comp = LE32(p + 16), colorsUsed = LE32(p + 32);
    (void)planes;
    if (w <= 0 || w > kMaxEdge) { err = "bad ICO image width"; return false; }
    int h = (hh < 0 ? -hh : hh) / 2;               // the height covers XOR + AND, so the picture is half
    if (h <= 0 || h > kMaxEdge) { err = "bad ICO image height"; return false; }
    bool topDown = hh < 0;
    if (comp != 0 && comp != 3) { err = "compressed ICO images are not supported"; return false; }
    if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24 && bpp != 32) { err = "unsupported ICO bit depth"; return false; }

    size_t palCount = 0;
    if (bpp <= 8) palCount = colorsUsed ? colorsUsed : ((size_t)1 << bpp);
    if (palCount > 256) palCount = 256;
    size_t palOff = hdr;
    size_t xorOff = palOff + palCount * 4;
    size_t xorStride = (((size_t)w * bpp + 31) / 32) * 4;
    size_t andOff = xorOff + xorStride * h;
    size_t andStride = (((size_t)w + 31) / 32) * 4;
    if (xorOff > len || andOff > len) { err = "ICO image data is truncated"; return false; }
    bool haveMask = andOff + andStride * h <= len;

    out.width = w; out.height = h;
    out.pixels.assign((size_t)w * h * 4, 0);
    bool anyAlpha = false;
    for (int y = 0; y < h; ++y) {
        int srcY = topDown ? y : (h - 1 - y);
        const uint8_t* row = p + xorOff + xorStride * srcY;
        for (int x = 0; x < w; ++x) {
            uint8_t* o = &out.pixels[((size_t)y * w + x) * 4];
            if (bpp == 32) { o[0] = row[x*4+2]; o[1] = row[x*4+1]; o[2] = row[x*4]; o[3] = row[x*4+3]; if (o[3]) anyAlpha = true; }
            else if (bpp == 24) { o[0] = row[x*3+2]; o[1] = row[x*3+1]; o[2] = row[x*3]; o[3] = 255; }
            else {
                int idx;
                if (bpp == 8) idx = row[x];
                else if (bpp == 4) idx = (x & 1) ? (row[x/2] & 15) : (row[x/2] >> 4);
                else idx = (row[x/8] >> (7 - (x & 7))) & 1;
                if ((size_t)idx >= palCount) idx = 0;
                const uint8_t* c = p + palOff + idx * 4;
                o[0] = c[2]; o[1] = c[1]; o[2] = c[0]; o[3] = 255;
            }
        }
    }
    // A 32-bit icon with real alpha ignores the AND mask; everything else uses it for transparency.
    if (!(bpp == 32 && anyAlpha) && haveMask) {
        for (int y = 0; y < h; ++y) {
            int srcY = topDown ? y : (h - 1 - y);
            const uint8_t* m = p + andOff + andStride * srcY;
            for (int x = 0; x < w; ++x) {
                bool transparent = (m[x/8] >> (7 - (x & 7))) & 1;
                uint8_t* o = &out.pixels[((size_t)y * w + x) * 4];
                o[3] = transparent ? 0 : 255;
            }
        }
    } else if (bpp == 32 && !anyAlpha) {
        for (size_t i = 3; i < out.pixels.size(); i += 4) out.pixels[i] = 255; // 32-bit but all alpha zero: treat as opaque
    }
    return true;
}

// Picks the entry closest to `wantedEdge` (the smallest one at least that
// big, otherwise the largest available).
inline const IcoEntry* PickIcoEntry(const std::vector<IcoEntry>& entries, int wantedEdge) {
    const IcoEntry* best = nullptr;
    for (const auto& e : entries) {
        int edge = std::max(e.width, e.height);
        if (edge >= wantedEdge && (!best || edge < std::max(best->width, best->height))) best = &e;
    }
    if (best) return best;
    for (const auto& e : entries)
        if (!best || std::max(e.width, e.height) > std::max(best->width, best->height)) best = &e;
    return best;
}

// -------------------------------------------------------------------------
// One entry point: decode a file's bytes, PNG or ICO, into RGBA. For an ICO
// `wantedEdge` chooses which size inside it to use.
// -------------------------------------------------------------------------
inline bool DecodeIcon(const uint8_t* d, size_t n, int wantedEdge, Rgba& out, std::string& err) {
    if (IsPng(d, n)) return DecodePng(d, n, out, err);
    std::vector<IcoEntry> entries;
    if (n >= 6 && LE16(d) == 0 && (LE16(d + 2) == 1 || LE16(d + 2) == 2)) {
        if (!ListIco(d, n, entries, err)) return false;
        const IcoEntry* pick = PickIcoEntry(entries, wantedEdge);
        return DecodeIcoEntry(d, n, *pick, out, err);
    }
    err = "unrecognised icon format (expected .png or .ico)";
    return false;
}

// Box-filter resize to a square `edge` x `edge` (aspect is preserved by
// letterboxing into transparent padding). Used to hit an exact size.
inline Rgba ResizeSquare(const Rgba& src, int edge) {
    Rgba dst;
    dst.width = dst.height = edge;
    dst.pixels.assign((size_t)edge * edge * 4, 0);
    if (src.width <= 0 || src.height <= 0 || edge <= 0) return dst;
    double scale = std::min((double)edge / src.width, (double)edge / src.height);
    int w = std::max(1, (int)(src.width * scale + 0.5)), h = std::max(1, (int)(src.height * scale + 0.5));
    int ox = (edge - w) / 2, oy = (edge - h) / 2;
    for (int y = 0; y < h; ++y) {
        double sy0 = (double)y / h * src.height, sy1 = (double)(y + 1) / h * src.height;
        for (int x = 0; x < w; ++x) {
            double sx0 = (double)x / w * src.width, sx1 = (double)(x + 1) / w * src.width;
            double r = 0, g = 0, b = 0, a = 0, wsum = 0;
            for (int yy = (int)sy0; yy < std::min(src.height, (int)std::ceil(sy1)); ++yy) {
                double wy = std::min(sy1, yy + 1.0) - std::max(sy0, (double)yy);
                for (int xx = (int)sx0; xx < std::min(src.width, (int)std::ceil(sx1)); ++xx) {
                    double wx = std::min(sx1, xx + 1.0) - std::max(sx0, (double)xx);
                    double wgt = wx * wy;
                    const uint8_t* s = &src.pixels[((size_t)yy * src.width + xx) * 4];
                    double al = s[3] * wgt; // weight colour by alpha so transparent pixels do not bleed their colour
                    r += s[0] * al; g += s[1] * al; b += s[2] * al; a += al; wsum += wgt;
                }
            }
            uint8_t* o = &dst.pixels[((size_t)(y + oy) * edge + (x + ox)) * 4];
            if (a > 0 && wsum > 0) { o[0] = (uint8_t)(r / a + 0.5); o[1] = (uint8_t)(g / a + 0.5); o[2] = (uint8_t)(b / a + 0.5); o[3] = (uint8_t)(a / wsum + 0.5); }
        }
    }
    return dst;
}

} // namespace IconDecode

#endif // ICON_DECODE_HPP
