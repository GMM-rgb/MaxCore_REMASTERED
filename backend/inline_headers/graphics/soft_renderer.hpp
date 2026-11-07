// =========================================================================
// soft_renderer.hpp
// =========================================================================
// The engine's own software renderer: tile-based, multi-threaded, no GPU and
// no driver. Takes the SAME triangle stream as the GPU backend (GlBackend::
// Vertex), so one engine code path feeds either.
//
// HOW A FRAME WORKS
//   1. AddTriangles() only RECORDS. Nothing is drawn yet.
//   2. Flush() bins every triangle into the 64x64 screen tiles it touches.
//   3. A persistent pool of worker threads takes tiles from a shared counter
//      and draws them. A tile has exactly ONE owner at a time, so pixels and
//      depth are never written by two threads: no locks, no atomics per pixel.
//   4. Flush() returns when every tile is done.
//
// ORDER IS PRESERVED. Within a tile, triangles are drawn in the order they
// were submitted (binning is a stable append), so translucent blending and
// equal-depth ties resolve exactly as the single-threaded rasteriser did.
//
// LUA STAYS ON THE MAIN THREAD. Workers only ever touch the framebuffer,
// depth buffer, the recorded triangles and read-only texture snapshots.
// Nothing here calls into Lua.
//
// OWNERSHIP. SoftRenderer owns its worker threads and joins them in the
// destructor. It does not own the framebuffer or textures; the caller keeps
// those alive for the duration of Flush().
// =========================================================================
#ifndef SOFT_RENDERER_HPP
#define SOFT_RENDERER_HPP

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <algorithm>
#include <memory>
#include "texture_store.hpp"
#include "simd_sample.hpp"

namespace SoftRender {

// One screen-space vertex, exactly what the GPU path receives.
struct Vertex {
    float x, y;       // pixels, origin top-left
    float z;          // view-space depth (smaller = closer); <= 0 means "2D layer, always in front"
    float invW;       // 1/z
    float u, v;       // texture coordinates (already scaled/offset by the binding)
    float r, g, b, a; // flat colour in 0..1 (already lit) / tint multiplied with the texel
    float texMix;     // 0 = flat colour, 1 = texture * colour
};

// A texture as the rasteriser needs it: a read-only pixel snapshot plus how to sample it.
struct Texture {
    std::shared_ptr<const Graphics::ImageBuffer> image;   // keeps the pixels alive for the frame
    Graphics::TextureBinding binding;
};

struct Target {
    uint32_t* color = nullptr;     // AARRGGBB
    float* depth = nullptr;        // view-space depth; kDepthFar = nothing drawn
    int width = 0, height = 0;
};

static constexpr int kTile = 64;
static constexpr float kFar = (std::numeric_limits<float>::max)();

// A recorded triangle: three vertices and which texture (index into the frame's list, -1 = none).
struct Tri { Vertex v[3]; int tex; };

// ---------------------------------------------------------------------
// Scalar reference rasteriser for ONE triangle clipped to ONE tile.
// Its arithmetic is the same as Graphics::FillTriangleTexturedDepth /
// FillConvexPolygonDepth, restricted to [x0,x1) x [y0,y1), so tiling never
// changes a pixel. SIMD variants must match this exactly.
// ---------------------------------------------------------------------
// The single place a finished pixel is written. alpha 255 writes colour and
// depth; alpha 0 draws nothing; anything between blends and claims no depth
// (so geometry behind a translucent face still shows). `overlay` pixels (the
// 2D layer) never touch depth.
inline void StorePixel(const Target& t, size_t idx, uint32_t px32, float depth, bool overlay) {
    const uint32_t alpha = px32 >> 24;
    if (alpha == 0) return;
    if (alpha == 255) {
        t.color[idx] = 0xFF000000u | (px32 & 0x00FFFFFFu);
        if (!overlay) t.depth[idx] = depth;
    } else {
        const uint32_t dst = t.color[idx]; const float k = alpha / 255.0f;
        const uint32_t r = (uint32_t)(((px32 >> 16) & 0xFF) * k + ((dst >> 16) & 0xFF) * (1.0f - k));
        const uint32_t g = (uint32_t)(((px32 >> 8) & 0xFF) * k + ((dst >> 8) & 0xFF) * (1.0f - k));
        const uint32_t bl = (uint32_t)((px32 & 0xFF) * k + (dst & 0xFF) * (1.0f - k));
        t.color[idx] = 0xFF000000u | (r << 16) | (g << 8) | bl;
    }
}

#if defined(SIMD_SAMPLE_AVX2)
// For 8 consecutive pixels starting `o0` pixels from the span origin: which are inside the triangle,
// pass the depth test, and what their depth is. Every value is origin + step * offset, the SAME
// expressions the scalar loop uses, so both give bit-identical results.
__attribute__((target("avx2")))
inline unsigned Cover8(int o0, float bw0, float A0, float bw1, float A1, float bw2, float A2, float bIz, float dIz, float sgn,
                       bool own0, bool own1, bool own2, bool overlay, const float* depthRow, float* depthOut) {
    const __m256 lane = _mm256_setr_ps(0, 1, 2, 3, 4, 5, 6, 7);
    const __m256 o = _mm256_add_ps(_mm256_set1_ps((float)o0), lane);
    const __m256 vs = _mm256_set1_ps(sgn), zero = _mm256_setzero_ps();
    const __m256 s0 = _mm256_mul_ps(_mm256_add_ps(_mm256_set1_ps(bw0), _mm256_mul_ps(_mm256_set1_ps(A0), o)), vs);
    const __m256 s1 = _mm256_mul_ps(_mm256_add_ps(_mm256_set1_ps(bw1), _mm256_mul_ps(_mm256_set1_ps(A1), o)), vs);
    const __m256 s2 = _mm256_mul_ps(_mm256_add_ps(_mm256_set1_ps(bw2), _mm256_mul_ps(_mm256_set1_ps(A2), o)), vs);
    // edge owned by this triangle: boundary pixels count (>= 0); not owned: strictly inside (> 0)
    // (_mm256_cmp_ps needs a compile-time mode, so pick between the two results instead.)
    const __m256 e0 = own0 ? _mm256_cmp_ps(s0, zero, _CMP_GE_OQ) : _mm256_cmp_ps(s0, zero, _CMP_GT_OQ);
    const __m256 e1 = own1 ? _mm256_cmp_ps(s1, zero, _CMP_GE_OQ) : _mm256_cmp_ps(s1, zero, _CMP_GT_OQ);
    const __m256 e2 = own2 ? _mm256_cmp_ps(s2, zero, _CMP_GE_OQ) : _mm256_cmp_ps(s2, zero, _CMP_GT_OQ);
    const __m256 in = _mm256_and_ps(_mm256_and_ps(e0, e1), e2);
    if (overlay) { _mm256_storeu_ps(depthOut, zero); return (unsigned)_mm256_movemask_ps(in); }
    const __m256 iz = _mm256_add_ps(_mm256_set1_ps(bIz), _mm256_mul_ps(_mm256_set1_ps(dIz), o));
    const __m256 depth = _mm256_div_ps(_mm256_set1_ps(1.0f), iz);
    _mm256_storeu_ps(depthOut, depth);
    const __m256 ok = _mm256_and_ps(_mm256_cmp_ps(iz, zero, _CMP_GT_OQ), _mm256_cmp_ps(depth, _mm256_loadu_ps(depthRow), _CMP_LT_OQ));   // depth >= buffer -> rejected
    return (unsigned)_mm256_movemask_ps(_mm256_and_ps(in, ok));
}
#endif

// Whether the 8-wide AVX2 path may be used. Tests flip this to compare it with the scalar loop.
inline bool& SimdEnabled() { static bool on = true; return on; }

inline void DrawTriInTile(const Target& t, const Tri& tri, const Texture* tex, int x0, int y0, int x1, int y1) {
    const bool useSimd = SimdEnabled() && SimdSample::HasAvx2();
    const Vertex &a = tri.v[0], &b = tri.v[1], &c = tri.v[2];
    auto edge = [](float ax, float ay, float bx, float by, float px, float py) { return (px - ax) * (by - ay) - (py - ay) * (bx - ax); };
    const float area = edge(a.x, a.y, b.x, b.y, c.x, c.y);
    if (std::abs(area) < 0.0001f) return;
    const float invArea = 1.0f / area;
    const bool pos = area > 0.0f;

    const bool textured = tri.v[0].texMix > 0.5f && tex && tex->image;
    if (textured && (a.z <= 1e-6f || b.z <= 1e-6f || c.z <= 1e-6f)) return;
    const bool overlay = a.z <= 0.0f && b.z <= 0.0f && c.z <= 0.0f;       // 2D layer: always in front, no depth test

    int minX = std::max(x0, (int)std::floor(std::min({a.x, b.x, c.x})));
    int maxX = std::min(x1 - 1, (int)std::ceil(std::max({a.x, b.x, c.x})));
    int minY = std::max(y0, (int)std::floor(std::min({a.y, b.y, c.y})));
    int maxY = std::min(y1 - 1, (int)std::ceil(std::max({a.y, b.y, c.y})));
    if (minX > maxX || minY > maxY) return;

    // Edge functions as A*x + B*y + C, so a step of one pixel in x is "+ A" and in y is "+ B".
    // w0 is b->c, w1 is c->a, w2 is a->b (the order the reference uses).
    const float A0 = (c.y - b.y), B0 = -(c.x - b.x), A1 = (a.y - c.y), B1 = -(a.x - c.x), A2 = (b.y - a.y), B2 = -(b.x - a.x);
    // TOP-LEFT FILL RULE. A pixel centre lying exactly on an edge shared by two
    // triangles must be drawn by exactly one of them, otherwise the shared
    // edge gets holes (neither draws it) or double-draws (both do, which
    // double-blends a translucent face). An edge "owns" its boundary pixels
    // only if it is a top edge or a left edge of the triangle; for the other
    // edges the boundary is excluded. That is expressed by requiring the
    // edge value to be strictly inside (> 0) instead of >= 0 on non-owning edges.
    // The test below works in "inside is positive" orientation, so flip signs for clockwise triangles.
    auto owns = [&](float ex, float ey) {         // edge direction (ex, ey) in the orientation where inside is positive
        // With y growing downward and inside-positive winding: a TOP edge is horizontal and runs right-to-left... derived below by test.
        return (ey > 0.0f) || (ey == 0.0f && ex < 0.0f);
    };
    const float sgn = pos ? 1.0f : -1.0f;
    // direction of each edge as walked a->b, b->c, c->a (matches w2, w0, w1)
    const bool own0 = owns((c.x - b.x) * sgn, (c.y - b.y) * sgn);   // w0 : b -> c
    const bool own1 = owns((a.x - c.x) * sgn, (a.y - c.y) * sgn);   // w1 : c -> a
    const bool own2 = owns((b.x - a.x) * sgn, (b.y - a.y) * sgn);   // w2 : a -> b
    const float sx = minX + 0.5f, sy = minY + 0.5f;
    float rw0 = edge(b.x, b.y, c.x, c.y, sx, sy), rw1 = edge(c.x, c.y, a.x, a.y, sx, sy), rw2 = edge(a.x, a.y, b.x, b.y, sx, sy);

    // Per-vertex values that are linear in screen space.
    const float iza = a.z > 0 ? 1.0f / a.z : 0.0f, izb = b.z > 0 ? 1.0f / b.z : 0.0f, izc = c.z > 0 ? 1.0f / c.z : 0.0f;
    const float uza = a.u * iza, uzb = b.u * izb, uzc = c.u * izc, vza = a.v * iza, vzb = b.v * izb, vzc = c.v * izc;
    // 1/z, u/z and v/z as plane functions of the edge values: value = (w0*Va + w1*Vb + w2*Vc) * invArea.
    // Their per-pixel-in-x steps are constants.
    const float dInvZ = (A0 * iza + A1 * izb + A2 * izc) * invArea, dUz = (A0 * uza + A1 * uzb + A2 * uzc) * invArea, dVz = (A0 * vza + A1 * vzb + A2 * vzc) * invArea;

    auto to8 = [](float f) { int v = (int)(f * 255.0f + 0.5f); return (uint32_t)(v < 0 ? 0 : v > 255 ? 255 : v); };
    const uint32_t flat = (to8(a.a) << 24) | (to8(a.r) << 16) | (to8(a.g) << 8) | to8(a.b);
    const uint32_t tintR = to8(a.r), tintG = to8(a.g), tintB = to8(a.b);

    for (int y = minY; y <= maxY; ++y, rw0 += B0, rw1 += B1, rw2 += B2) {
        // Where this row is inside the triangle: each edge is linear in x, so solve for the span instead of testing every pixel.
        float xl = (float)minX, xr = (float)maxX; bool empty = false;
        const float w[3] = {rw0, rw1, rw2}, st[3] = {A0, A1, A2};
        for (int e = 0; e < 3 && !empty; ++e) {
            const float base = pos ? w[e] : -w[e], slope = pos ? st[e] : -st[e];       // inside means base + slope*k >= 0, k = pixels from minX
            if (std::fabs(slope) < 1e-12f) { if (base < 0.0f) empty = true; continue; }
            const float kb = -base / slope;
            if (slope > 0.0f) xl = std::max(xl, (float)minX + kb); else xr = std::min(xr, (float)minX + kb);
        }
        if (empty || xl > xr) continue;
        const int xs = std::max(minX, (int)std::floor(xl) - 1), xe = std::min(maxX, (int)std::ceil(xr) + 1);   // slack; exact test decides
        const int k0 = xs - minX;
        float cw0 = rw0 + A0 * k0, cw1 = rw1 + A1 * k0, cw2 = rw2 + A2 * k0;
        // running 1/z, u/z, v/z at the first pixel of the span
        float cInvZ = (cw0 * iza + cw1 * izb + cw2 * izc) * invArea, cUz = (cw0 * uza + cw1 * uzb + cw2 * uzc) * invArea, cVz = (cw0 * vza + cw1 * vzb + cw2 * vzc) * invArea;
        size_t idx = (size_t)y * (size_t)t.width + (size_t)xs;
        int x = xs;
#if defined(SIMD_SAMPLE_AVX2)
        // 8 pixels at a time when this triangle is textured and the CPU has AVX2.
        // Coverage, depth test and the texture sample are computed for 8
        // pixels; the shade/blend/store then runs per pixel with the SAME
        // code as the scalar loop, so the two paths cannot disagree.
        if (textured && useSimd) {
            while (x + 7 <= xe) {
                float d8[8]; const int o0 = x - xs;
                const unsigned live = Cover8(o0, cw0, A0, cw1, A1, cw2, A2, cInvZ, dInvZ, sgn, own0, own1, own2, overlay, t.depth + idx, d8);
                if (live) {
                    float u8[8], v8[8]; uint32_t texel8[8];
                    for (int k = 0; k < 8; ++k) { const int o = o0 + k; u8[k] = (cUz + dUz * o) * d8[k]; v8[k] = (cVz + dVz * o) * d8[k]; }
                    SimdSample::Sample8(*tex->image, tex->binding, u8, v8, texel8);
                    for (int k = 0; k < 8; ++k) {
                        if (!(live & (1u << k))) continue;
                        const uint32_t texel = texel8[k];
                        if ((texel >> 24) == 0) continue;
                        const uint32_t tr = (((texel >> 16) & 0xFF) * tintR + 127) / 255, tg = (((texel >> 8) & 0xFF) * tintG + 127) / 255, tb = ((texel & 0xFF) * tintB + 127) / 255;
                        StorePixel(t, idx + k, (texel & 0xFF000000u) | (tr << 16) | (tg << 8) | tb, d8[k], overlay);
                    }
                }
                x += 8; idx += 8;
            }
        }
#endif
        // Anything the 8-wide loop did not cover (a short span, or the tail) finishes here. Every value is
        // "span start + step * k", never a running sum, so this loop and the 8-wide one compute
        // bit-identical numbers for the same pixel.
        const float bw0 = cw0, bw1 = cw1, bw2 = cw2, bIz = cInvZ, bUz = cUz, bVz = cVz;
        for (; x <= xe; ++x, ++idx) {
            const int k = x - xs;                                    // offset from the SAME origin as the 8-wide loop
            const float c0 = bw0 + A0 * k, c1 = bw1 + A1 * k, c2 = bw2 + A2 * k;
            const float s0 = c0 * sgn, s1 = c1 * sgn, s2 = c2 * sgn;
            const bool inside = (own0 ? s0 >= 0 : s0 > 0) && (own1 ? s1 >= 0 : s1 > 0) && (own2 ? s2 >= 0 : s2 > 0);
            if (!inside) continue;
            float depth = 0.0f;
            const float iz = bIz + dInvZ * k;
            if (!overlay) {
                if (iz <= 0.0f) continue;
                depth = 1.0f / iz;
                if (depth >= t.depth[idx]) continue;
            }
            uint32_t px32 = flat;
            if (textured) {
                uint32_t texel = Graphics::SampleTexture(*tex->image, tex->binding, (bUz + dUz * k) * depth, (bVz + dVz * k) * depth);
                if ((texel >> 24) == 0) continue;
                const uint32_t tr = (((texel >> 16) & 0xFF) * tintR + 127) / 255, tg = (((texel >> 8) & 0xFF) * tintG + 127) / 255, tb = ((texel & 0xFF) * tintB + 127) / 255;
                px32 = (texel & 0xFF000000u) | (tr << 16) | (tg << 8) | tb;
            }
            StorePixel(t, idx, px32, depth, overlay);
        }
    }
}

// ---------------------------------------------------------------------
// The renderer: recording, binning, worker pool.
// ---------------------------------------------------------------------
class Renderer {
public:
    // threads == 0 picks one per hardware core (at least 1). 1 runs everything on the calling thread.
    explicit Renderer(unsigned threads = 0) {
        unsigned n = threads ? threads : std::thread::hardware_concurrency();
        if (n == 0) n = 1;
        threadCount_ = n;
        // The calling thread also works, so n-1 helpers.
        for (unsigned i = 1; i < n; ++i) workers_.emplace_back([this] { WorkerLoop(); });
    }
    ~Renderer() {
        { std::lock_guard<std::mutex> lk(mu_); quit_ = true; ++generation_; }
        cv_.notify_all();
        for (auto& w : workers_) w.join();
    }
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    unsigned Threads() const { return threadCount_; }

    // Registers a texture for this frame; returns its index (pass to AddTriangles).
    int AddTexture(std::shared_ptr<const Graphics::ImageBuffer> image, const Graphics::TextureBinding& binding) {
        textures_.push_back({std::move(image), binding});
        return (int)textures_.size() - 1;
    }

    void AddTriangles(const Vertex* v, size_t count, int texture) {
        if (!v || count == 0 || count % 3 != 0) return;
        for (size_t i = 0; i < count; i += 3) { Tri t; t.v[0] = v[i]; t.v[1] = v[i + 1]; t.v[2] = v[i + 2]; t.tex = texture; tris_.push_back(t); }
    }

    // Draws everything recorded since the last Flush into `target`, then clears the recording.
    void Flush(const Target& target) {
        if (tris_.empty() || !target.color || !target.depth || target.width <= 0 || target.height <= 0) { Reset(); return; }
        target_ = target;
        tilesX_ = (target.width + kTile - 1) / kTile;
        tilesY_ = (target.height + kTile - 1) / kTile;
        Bin();
        next_.store(0);
        if (workers_.empty()) { RunTiles(); }
        else {
            { std::lock_guard<std::mutex> lk(mu_); pending_ = (int)workers_.size(); ++generation_; }
            cv_.notify_all();
            RunTiles();                                   // the calling thread helps
            std::unique_lock<std::mutex> lk(mu_);
            done_.wait(lk, [this] { return pending_ == 0; });
        }
        Reset();
    }

    size_t RecordedTriangles() const { return tris_.size(); }

private:
    void Reset() { tris_.clear(); textures_.clear(); }

    // Stable bin: tile index -> list of triangle indices in submission order.
    void Bin() {
        const size_t nt = (size_t)tilesX_ * tilesY_;
        bins_.assign(nt, {});
        for (size_t i = 0; i < tris_.size(); ++i) {
            const Tri& t = tris_[i];
            float fx0 = std::min({t.v[0].x, t.v[1].x, t.v[2].x}), fx1 = std::max({t.v[0].x, t.v[1].x, t.v[2].x});
            float fy0 = std::min({t.v[0].y, t.v[1].y, t.v[2].y}), fy1 = std::max({t.v[0].y, t.v[1].y, t.v[2].y});
            if (!(fx1 >= 0) || !(fy1 >= 0) || !(fx0 <= (float)target_.width) || !(fy0 <= (float)target_.height)) continue;   // off screen (also rejects NaN)
            auto clampI = [](float v, int hi) { return (int)std::max(0.0f, std::min((float)hi, v)); };
            const int tx0 = clampI(std::floor(fx0) / kTile, tilesX_ - 1), tx1 = clampI(std::ceil(fx1) / kTile, tilesX_ - 1);
            const int ty0 = clampI(std::floor(fy0) / kTile, tilesY_ - 1), ty1 = clampI(std::ceil(fy1) / kTile, tilesY_ - 1);
            for (int ty = ty0; ty <= ty1; ++ty) for (int tx = tx0; tx <= tx1; ++tx) bins_[(size_t)ty * tilesX_ + tx].push_back((uint32_t)i);
        }
    }

    // Each thread repeatedly claims the next unclaimed tile.
    void RunTiles() {
        const int total = tilesX_ * tilesY_;
        for (;;) {
            const int ti = next_.fetch_add(1);
            if (ti >= total) break;
            const auto& list = bins_[(size_t)ti];
            if (list.empty()) continue;
            const int x0 = (ti % tilesX_) * kTile, y0 = (ti / tilesX_) * kTile;
            const int x1 = std::min(x0 + kTile, target_.width), y1 = std::min(y0 + kTile, target_.height);
            for (uint32_t idx : list) {
                const Tri& t = tris_[idx];
                const Texture* tx = (t.tex >= 0 && (size_t)t.tex < textures_.size()) ? &textures_[(size_t)t.tex] : nullptr;
                DrawTriInTile(target_, t, tx, x0, y0, x1, y1);
            }
        }
    }

    void WorkerLoop() {
        uint64_t seen = 0;
        for (;;) {
            { std::unique_lock<std::mutex> lk(mu_); cv_.wait(lk, [&] { return generation_ != seen; }); seen = generation_; if (quit_) return; }
            RunTiles();
            { std::lock_guard<std::mutex> lk(mu_); if (--pending_ == 0) done_.notify_one(); }
        }
    }

    unsigned threadCount_ = 1;
    std::vector<std::thread> workers_;
    std::mutex mu_;
    std::condition_variable cv_, done_;
    uint64_t generation_ = 0;
    int pending_ = 0;
    bool quit_ = false;

    std::vector<Tri> tris_;
    std::vector<Texture> textures_;
    std::vector<std::vector<uint32_t>> bins_;
    Target target_;
    int tilesX_ = 0, tilesY_ = 0;
    std::atomic<int> next_{0};
};

} // namespace SoftRender

#endif // SOFT_RENDERER_HPP
