// =========================================================================
// textured_raster.hpp
// =========================================================================
// Perspective-correct, depth-tested textured triangle fill.
//
// Same inside/outside edge-function test and depth convention as
// FillTriangleDepth in window_management.cpp (smaller z = closer), plus
// per-pixel texture sampling. UVs are interpolated as u/z, v/z and 1/z
// then divided per pixel -- plain affine interpolation makes textures
// "swim" on a rotating 3D face, which is exactly the artifact a software
// 3D renderer must avoid.
//
// Ownership: this function never owns or frees anything. It borrows an
// ImageBuffer for the duration of one call (the caller holds the
// shared_ptr snapshot), so there is nothing here that can leak.
// =========================================================================
#ifndef TEXTURED_RASTER_HPP
#define TEXTURED_RASTER_HPP

#include "texture_store.hpp"
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace Graphics {

struct TexVertex {
    float x, y;   // screen position
    float z;      // view-space depth (> 0 in front of the camera)
    float u, v;   // texture coordinates at this vertex
};

// Draws one triangle. `shade` is the 0..1 lighting factor (1 = unlit).
// Texels with alpha 0 are skipped (the face behind shows through); texels
// with partial alpha are blended over the existing pixel.
// Returns the number of pixels written (useful for tests and profiling).
inline int FillTriangleTexturedDepth(
    uint32_t* buffer, float* depthBuffer, int width, int height,
    const TexVertex& a, const TexVertex& b, const TexVertex& c,
    const ImageBuffer& img, const TextureBinding& binding, float shade)
{
    if (!buffer || !depthBuffer || width <= 0 || height <= 0) return 0;
    // Perspective divide needs positive depth. A vertex at/behind the eye
    // can't be interpolated safely, so skip the triangle instead of
    // producing NaNs or garbage.
    if (a.z <= 1e-6f || b.z <= 1e-6f || c.z <= 1e-6f) return 0;

    int minX = std::max(0, (int)std::floor(std::min({a.x, b.x, c.x})));
    int maxX = std::min(width - 1, (int)std::ceil(std::max({a.x, b.x, c.x})));
    int minY = std::max(0, (int)std::floor(std::min({a.y, b.y, c.y})));
    int maxY = std::min(height - 1, (int)std::ceil(std::max({a.y, b.y, c.y})));
    if (minX > maxX || minY > maxY) return 0;

    auto edge = [](float ax, float ay, float bx, float by, float px, float py) {
        return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
    };

    float area = edge(a.x, a.y, b.x, b.y, c.x, c.y);
    if (std::abs(area) < 0.0001f) return 0;
    const float invArea = 1.0f / area;

    // Per-vertex values that interpolate linearly in screen space.
    const float iza = 1.0f / a.z, izb = 1.0f / b.z, izc = 1.0f / c.z;
    const float uza = a.u * iza, uzb = b.u * izb, uzc = c.u * izc;
    const float vza = a.v * iza, vzb = b.v * izb, vzc = c.v * izc;

    const bool pos = area > 0.0f;

    // The three edge functions, as (start, end) point pairs, in the same
    // order the per-pixel test uses: w0 is b->c, w1 is c->a, w2 is a->b.
    struct E { float ax, ay, bx, by; };
    const E E3[3] = { {b.x, b.y, c.x, c.y}, {c.x, c.y, a.x, a.y}, {a.x, a.y, b.x, b.y} };

    int written = 0;
    for (int y = minY; y <= maxY; ++y) {
        const float py = y + 0.5f;

        // Narrow the row to the pixels that can be inside all three edges.
        // An edge function is linear in x, so its value at two x positions
        // gives the boundary directly, using the SAME formula as the
        // per-pixel test (no separately derived coefficients to get wrong).
        float xl = (float)minX, xr = (float)maxX;
        bool empty = false;
        for (int e = 0; e < 3 && !empty; ++e) {
            const float v0 = edge(E3[e].ax, E3[e].ay, E3[e].bx, E3[e].by, 0.5f, py);   // pixel x = 0
            const float v1 = edge(E3[e].ax, E3[e].ay, E3[e].bx, E3[e].by, 1.5f, py);   // pixel x = 1
            const float slope = pos ? (v1 - v0) : (v0 - v1);       // inside means value*sign >= 0
            const float base  = pos ? v0 : -v0;                    // value at pixel x = 0
            if (std::fabs(slope) < 1e-12f) { if (base < 0.0f) empty = true; continue; }
            const float xb = -base / slope;                        // pixel index where it crosses zero
            if (slope > 0.0f) xl = (std::max)(xl, xb);             // inside to the right of xb
            else              xr = (std::min)(xr, xb);             // inside to the left of xb
        }
        if (empty || xl > xr) continue;
        // one pixel of slack each side; the exact per-pixel test below decides
        const int x0 = (std::max)(minX, (int)std::floor(xl) - 1);
        const int x1 = (std::min)(maxX, (int)std::ceil(xr) + 1);

        const size_t rowBase = (size_t)y * (size_t)width;
        for (int x = x0; x <= x1; ++x) {
            const float px = x + 0.5f;
            // Exactly the original per-pixel test and values, so the picture is unchanged.
            const float w0 = edge(b.x, b.y, c.x, c.y, px, py);
            const float w1 = edge(c.x, c.y, a.x, a.y, px, py);
            const float w2 = edge(a.x, a.y, b.x, b.y, px, py);

            const bool inside = pos ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
                                    : (w0 <= 0 && w1 <= 0 && w2 <= 0);
            if (!inside) continue;

            // Normalized barycentric weights for a, b, c.
            const float l0 = w0 * invArea, l1 = w1 * invArea, l2 = w2 * invArea;

            const float invZ = l0 * iza + l1 * izb + l2 * izc;
            if (invZ <= 0.0f) continue;
            const float depth = 1.0f / invZ;

            const size_t idx = rowBase + (size_t)x;
            if (depth >= depthBuffer[idx]) continue;

            const float u = (l0 * uza + l1 * uzb + l2 * uzc) * depth;
            const float v = (l0 * vza + l1 * vzb + l2 * vzc) * depth;

            uint32_t texel = SampleTexture(img, binding, u, v);
            uint32_t alpha = (texel >> 24) & 0xFFu;
            if (alpha == 0) continue; // fully transparent texel: leave pixel (and depth) untouched

            texel = ShadeTexel(texel, shade);

            if (alpha == 255) {
                buffer[idx] = 0xFF000000u | (texel & 0x00FFFFFFu);
                depthBuffer[idx] = depth;
            } else {
                // Translucent texel: blend over what's there, but don't
                // claim depth so geometry behind it can still show.
                uint32_t dst = buffer[idx];
                float t = alpha / 255.0f;
                uint32_t r = (uint32_t)(((texel >> 16) & 0xFF) * t + ((dst >> 16) & 0xFF) * (1.0f - t));
                uint32_t g = (uint32_t)(((texel >> 8) & 0xFF) * t + ((dst >> 8) & 0xFF) * (1.0f - t));
                uint32_t bl = (uint32_t)((texel & 0xFF) * t + (dst & 0xFF) * (1.0f - t));
                buffer[idx] = 0xFF000000u | (r << 16) | (g << 8) | bl;
            }
            ++written;
        }
    }
    return written;
}

} // namespace Graphics

#endif // TEXTURED_RASTER_HPP
