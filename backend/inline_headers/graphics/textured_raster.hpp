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

    // Per-vertex values that interpolate linearly in screen space.
    const float iza = 1.0f / a.z, izb = 1.0f / b.z, izc = 1.0f / c.z;
    const float uza = a.u * iza, uzb = b.u * izb, uzc = c.u * izc;
    const float vza = a.v * iza, vzb = b.v * izb, vzc = c.v * izc;

    int written = 0;
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const float px = x + 0.5f, py = y + 0.5f;
            float w0 = edge(b.x, b.y, c.x, c.y, px, py);
            float w1 = edge(c.x, c.y, a.x, a.y, px, py);
            float w2 = edge(a.x, a.y, b.x, b.y, px, py);

            bool inside = (area > 0 && w0 >= 0 && w1 >= 0 && w2 >= 0) ||
                          (area < 0 && w0 <= 0 && w1 <= 0 && w2 <= 0);
            if (!inside) continue;

            // Normalized barycentric weights for a, b, c.
            const float l0 = w0 / area, l1 = w1 / area, l2 = w2 / area;

            const float invZ = l0 * iza + l1 * izb + l2 * izc;
            if (invZ <= 0.0f) continue;
            const float depth = 1.0f / invZ;

            const size_t idx = (size_t)y * (size_t)width + (size_t)x;
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
