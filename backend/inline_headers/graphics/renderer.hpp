#ifndef RENDERER_HPP
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define RENDERER_HPP

#if !(defined(_WIN32) || defined(_WIN64))
    #warning "Windows Operating System definition not found!"
#elif defined(__APPLE__)
    #pragma never
#endif

#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace Graphics {

struct Vec2 {
    float x, y;
};

struct Vec3 {
    float x, y, z;
};

struct Vec4 {
    float x, y, z, w;
};

struct Matrix4x4 {
    float m[4][4] = {0};

    static Matrix4x4 Identity() {
        Matrix4x4 mat;
        mat.m[0][0] = 1.0f; mat.m[1][1] = 1.0f;
        mat.m[2][2] = 1.0f; mat.m[3][3] = 1.0f;
        return mat;
    }

    static Matrix4x4 Perspective(float fovDeg, float aspect, float nearPlane, float farPlane) {
        Matrix4x4 mat;
        float fovRad = 1.0f / std::tan(fovDeg * 0.5f * 3.14159265f / 180.0f);
        mat.m[0][0] = aspect * fovRad;
        mat.m[1][1] = fovRad;
        mat.m[2][2] = farPlane / (farPlane - nearPlane);
        mat.m[2][3] = 1.0f;
        mat.m[3][2] = (-farPlane * nearPlane) / (farPlane - nearPlane);
        return mat;
    }

    static Matrix4x4 RotationY(float angleRad) {
        Matrix4x4 mat = Identity();
        mat.m[0][0] = std::cos(angleRad);
        mat.m[0][2] = std::sin(angleRad);
        mat.m[2][0] = -std::sin(angleRad);
        mat.m[2][2] = std::cos(angleRad);
        return mat;
    }

    static Matrix4x4 RotationX(float angleRad) {
        Matrix4x4 mat = Identity();
        mat.m[1][1] = std::cos(angleRad);
        mat.m[1][2] = std::sin(angleRad);
        mat.m[2][1] = -std::sin(angleRad);
        mat.m[2][2] = std::cos(angleRad);
        return mat;
    }

    // Added alongside RotationX/RotationY for the generic mesh pipeline
    // (draw_cube never applied roll/Z rotation; draw_mesh does).
    static Matrix4x4 RotationZ(float angleRad) {
        Matrix4x4 mat = Identity();
        mat.m[0][0] = std::cos(angleRad);
        mat.m[0][1] = std::sin(angleRad);
        mat.m[1][0] = -std::sin(angleRad);
        mat.m[1][1] = std::cos(angleRad);
        return mat;
    }

    static Matrix4x4 Translation(float x, float y, float z) {
        Matrix4x4 mat = Identity();
        mat.m[3][0] = x;
        mat.m[3][1] = y;
        mat.m[3][2] = z;
        return mat;
    }

    Vec4 MultiplyVector(const Vec3& v) const {
        Vec4 out;
        out.x = v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0] + m[3][0];
        out.y = v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1] + m[3][1];
        out.z = v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2] + m[3][2];
        out.w = v.x * m[0][3] + v.y * m[1][3] + v.z * m[2][3] + m[3][3];
        return out;
    }
};

struct ImageBuffer {
    int width = 0;
    int height = 0;
    std::vector<uint32_t> pixels;
};

// =========================================================================
// VEC3 MATH HELPERS
// =========================================================================
inline float Dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 Cross(const Vec3& a, const Vec3& b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

inline Vec3 Subtract(const Vec3& a, const Vec3& b) {
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

inline Vec3 Normalize(const Vec3& v) {
    float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 0.00001f) return { 0.0f, 0.0f, 0.0f };
    return { v.x / len, v.y / len, v.z / len };
}

// =========================================================================
// CAMERA
// =========================================================================
// A minimal free-look camera. Position + yaw/pitch define the viewport
// transform; roll is stored but not applied to geometry transforms (reserved
// for screen-space tilt effects later). fov/near/far feed straight into
// Matrix4x4::Perspective.
struct Camera {
    Vec3 position{0.0f, 0.0f, 0.0f};
    float pitch = 0.0f; // rotation around X axis, radians
    float yaw   = 0.0f; // rotation around Y axis, radians
    float roll  = 0.0f; // rotation around Z axis, radians (reserved)
    float fov = 90.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
};

// Transforms a world-space point into this camera's view space (i.e. what
// you'd get from an inverse camera transform: translate by -position, then
// undo yaw, then undo pitch). Feed the result into Project3DPoint instead
// of the raw world position to make geometry respect the active camera.
inline Vec3 WorldToView(const Vec3& worldPos, const Camera& cam) {
    Vec3 rel = { worldPos.x - cam.position.x, worldPos.y - cam.position.y, worldPos.z - cam.position.z };

    Matrix4x4 invYaw = Matrix4x4::RotationY(-cam.yaw);
    Vec4 afterYaw = invYaw.MultiplyVector(rel);
    Vec3 afterYaw3 = { afterYaw.x, afterYaw.y, afterYaw.z };

    Matrix4x4 invPitch = Matrix4x4::RotationX(-cam.pitch);
    Vec4 afterPitch = invPitch.MultiplyVector(afterYaw3);

    return { afterPitch.x, afterPitch.y, afterPitch.z };
}

// =========================================================================
// BASIC LIGHTING ("SHADERS")
// =========================================================================
// This is a software rasterizer, not a GPU pipeline, so there's no real
// vertex/fragment shader stage -- this is the CPU equivalent that gets you
// most of the visual benefit cheaply: one flat Lambertian (diffuse +
// ambient) shade computed per FACE from its normal, applied uniformly
// across that face's triangles. It intentionally does not do per-pixel or
// per-vertex (Gouraud/Phong) shading.
struct Light {
    Vec3 direction{0.4f, -0.7f, 0.6f}; // direction the light travels, from source toward the scene
    float ambient = 0.35f;             // base brightness even on faces facing away from the light
    float intensity = 1.0f;            // diffuse contribution multiplier
};

// Computes a single [0,1] brightness factor for a face from its (unnormalized
// is fine) normal and the active light. Faces should be wound consistently
// (counter-clockwise viewed from outside the mesh) so normals point outward --
// otherwise shading will read as inverted.
inline float ComputeFaceShade(const Vec3& faceNormal, const Light& light) {
    Vec3 n = Normalize(faceNormal);
    Vec3 travelDir = Normalize(light.direction);
    Vec3 towardLight = { -travelDir.x, -travelDir.y, -travelDir.z };

    float diffuse = (std::max)(0.0f, Dot(n, towardLight));
    float shade = light.ambient + diffuse * light.intensity;
    return (std::min)(1.0f, (std::max)(0.0f, shade));
}

// Multiplies an AARRGGBB color's RGB channels by a [0,1] shade factor,
// leaving alpha untouched.
inline uint32_t ShadeColor(uint32_t color, float shade) {
    uint8_t a = (color >> 24) & 0xFF;
    uint8_t r = (uint8_t)(std::min)(255.0f, ((color >> 16) & 0xFF) * shade);
    uint8_t g = (uint8_t)(std::min)(255.0f, ((color >> 8) & 0xFF) * shade);
    uint8_t b = (uint8_t)(std::min)(255.0f, (color & 0xFF) * shade);
    return (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}

// =========================================================================
// RASTERIZATION ENGINE
// =========================================================================

inline void PutPixel(uint32_t* buffer, int bufWidth, int bufHeight, int x, int y, uint32_t color) {
    if (x >= 0 && x < bufWidth && y >= 0 && y < bufHeight) {
        buffer[y * bufWidth + x] = color;
    }
}

// Blends an RGB color into an existing buffer pixel using an EXTERNALLY
// supplied coverage/alpha in [0,1] (ignores whatever's in color's own alpha
// byte). Used by anti-aliased rendering -- e.g. supersampled text -- where
// each pixel's coverage is computed on the fly rather than baked into a
// source image's alpha channel like DrawImage's blending does.
inline void BlendPixel(uint32_t* buffer, int bufWidth, int bufHeight, int x, int y, uint32_t rgbColor, float alpha) {
    if (x < 0 || x >= bufWidth || y < 0 || y >= bufHeight) return;
    if (alpha <= 0.0f) return;
    if (alpha >= 1.0f) {
        buffer[y * bufWidth + x] = rgbColor;
        return;
    }
    uint32_t dst = buffer[y * bufWidth + x];
    uint8_t r = (uint8_t)(((rgbColor >> 16) & 0xFF) * alpha + ((dst >> 16) & 0xFF) * (1.0f - alpha));
    uint8_t g = (uint8_t)(((rgbColor >> 8) & 0xFF) * alpha + ((dst >> 8) & 0xFF) * (1.0f - alpha));
    uint8_t b = (uint8_t)((rgbColor & 0xFF) * alpha + (dst & 0xFF) * (1.0f - alpha));
    buffer[y * bufWidth + x] = (0xFF << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}

// Generic supersampled anti-aliasing rasterizer: for every pixel in
// [minX,maxX] x [minY,maxY], averages quality*quality jittered sub-samples
// of an arbitrary "is this point inside the shape" predicate into a smooth
// [0,1] coverage value, then blends `color` over the existing buffer by
// that coverage (skipping fully-uncovered pixels). This backs the AA paths
// for lines, circles, and filled triangles -- same technique as the text
// renderer's glyph supersampling, just with a caller-supplied shape test
// instead of a fixed glyph bitmap lookup.
template <typename Predicate>
inline void RasterizeAA(uint32_t* buffer, int bufW, int bufH, int minX, int maxX, int minY, int maxY, uint32_t color, int quality, Predicate isInside) {
    if (quality < 1) quality = 1;
    minX = (std::max)(0, minX);
    minY = (std::max)(0, minY);
    maxX = (std::min)(bufW - 1, maxX);
    maxY = (std::min)(bufH - 1, maxY);

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            float coverage = 0.0f;
            for (int sy = 0; sy < quality; ++sy) {
                for (int sx = 0; sx < quality; ++sx) {
                    float px = x + (sx + 0.5f) / (float)quality;
                    float py = y + (sy + 0.5f) / (float)quality;
                    if (isInside(px, py)) coverage += 1.0f;
                }
            }
            coverage /= (float)(quality * quality);
            if (coverage <= 0.0f) continue;
            BlendPixel(buffer, bufW, bufH, x, y, color, coverage);
        }
    }
}

// Bresenham's Line Algorithm. `quality` (default 0) is an anti-aliasing
// sampling level -- 0 keeps this exact hard-pixel-stepped behavior; 1+
// switches to a supersampled path below that treats the line as a thin
// capsule and gives it smooth, sub-pixel-accurate edges.
inline void DrawLine(uint32_t* buffer, int width, int height, int x0, int y0, int x1, int y1, uint32_t color, int quality = 0) {
    if (quality < 1) {
        int dx = std::abs(x1 - x0);
        int sx = x0 < x1 ? 1 : -1;
        int dy = -std::abs(y1 - y0);
        int sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;

        while (true) {
            PutPixel(buffer, width, height, x0, y0, color);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
        return;
    }

    // Anti-aliased path: the line as a thin capsule (segment + half-width),
    // supersampled per pixel across its bounding box.
    const float halfWidth = 0.75f;
    float fx0 = (float)x0, fy0 = (float)y0, fx1 = (float)x1, fy1 = (float)y1;
    float ldx = fx1 - fx0, ldy = fy1 - fy0;
    float lenSq = ldx * ldx + ldy * ldy;

    int minX = (int)std::floor((std::min)(fx0, fx1) - halfWidth - 1.0f);
    int maxX = (int)std::ceil((std::max)(fx0, fx1) + halfWidth + 1.0f);
    int minY = (int)std::floor((std::min)(fy0, fy1) - halfWidth - 1.0f);
    int maxY = (int)std::ceil((std::max)(fy0, fy1) + halfWidth + 1.0f);

    RasterizeAA(buffer, width, height, minX, maxX, minY, maxY, color, quality, [&](float px, float py) {
        float t = 0.0f;
        if (lenSq > 0.0001f) {
            t = ((px - fx0) * ldx + (py - fy0) * ldy) / lenSq;
            t = (std::max)(0.0f, (std::min)(1.0f, t));
        }
        float closestX = fx0 + t * ldx;
        float closestY = fy0 + t * ldy;
        float distX = px - closestX;
        float distY = py - closestY;
        return (distX * distX + distY * distY) <= (halfWidth * halfWidth);
    });
}

// Midpoint Circle Algorithm. `quality` (default 0) is an anti-aliasing
// sampling level -- 0 keeps this exact hard-pixel behavior; 1+ switches to
// a supersampled path that rasterizes against the ideal circle (fill:
// inside the radius; outline: within a thin band around it) for smooth
// edges instead of the midpoint algorithm's stair-stepping.
inline void DrawCircle(uint32_t* buffer, int width, int height, int cx, int cy, int radius, uint32_t color, bool fill = false, int quality = 0) {
    if (quality < 1) {
        int x = radius;
        int y = 0;
        int err = 0;

        while (x >= y) {
            if (fill) {
                DrawLine(buffer, width, height, cx - x, cy + y, cx + x, cy + y, color);
                DrawLine(buffer, width, height, cx - x, cy - y, cx + x, cy - y, color);
                DrawLine(buffer, width, height, cx - y, cy + x, cx + y, cy + x, color);
                DrawLine(buffer, width, height, cx - y, cy - x, cx + y, cy - x, color);
            } else {
                PutPixel(buffer, width, height, cx + x, cy + y, color);
                PutPixel(buffer, width, height, cx + y, cy + x, color);
                PutPixel(buffer, width, height, cx - y, cy + x, color);
                PutPixel(buffer, width, height, cx - x, cy + y, color);
                PutPixel(buffer, width, height, cx - x, cy - y, color);
                PutPixel(buffer, width, height, cx - y, cy - x, color);
                PutPixel(buffer, width, height, cx + y, cy - x, color);
                PutPixel(buffer, width, height, cx + x, cy - y, color);
            }

            if (err <= 0) {
                y += 1;
                err += 2 * y + 1;
            }
            if (err > 0) {
                x -= 1;
                err -= 2 * x + 1;
            }
        }
        return;
    }

    const float outlineHalfWidth = 0.75f;
    int minX = cx - radius - 1;
    int maxX = cx + radius + 1;
    int minY = cy - radius - 1;
    int maxY = cy + radius + 1;
    float r = (float)radius;
    float fcx = (float)cx, fcy = (float)cy;

    RasterizeAA(buffer, width, height, minX, maxX, minY, maxY, color, quality, [&](float px, float py) {
        float dx = px - fcx;
        float dy = py - fcy;
        float dist = std::sqrt(dx * dx + dy * dy);
        return fill ? (dist <= r) : (std::fabs(dist - r) <= outlineHalfWidth);
    });
}

// Blit Image / Texture to Buffer
inline void DrawImage(uint32_t* buffer, int bufW, int bufH, const ImageBuffer& img, int destX, int destY) {
    for (int iy = 0; iy < img.height; ++iy) {
        int py = destY + iy;
        if (py < 0 || py >= bufH) continue;

        for (int ix = 0; ix < img.width; ++ix) {
            int px = destX + ix;
            if (px < 0 || px >= bufW) continue;

            uint32_t color = img.pixels[iy * img.width + ix];
            // Alpha Blending Channel (AARRGGBB)
            uint8_t alpha = (color >> 24) & 0xFF;
            if (alpha == 0) continue;

            if (alpha == 255) {
                buffer[py * bufW + px] = color;
            } else {
                uint32_t dstColor = buffer[py * bufW + px];
                float a = alpha / 255.0f;
                uint8_t r = (uint8_t)(((color >> 16) & 0xFF) * a + ((dstColor >> 16) & 0xFF) * (1.0f - a));
                uint8_t g = (uint8_t)(((color >> 8) & 0xFF) * a + ((dstColor >> 8) & 0xFF) * (1.0f - a));
                uint8_t b = (uint8_t)((color & 0xFF) * a + (dstColor & 0xFF) * (1.0f - a));
                buffer[py * bufW + px] = (0xFF << 24) | (r << 16) | (g << 8) | b;
            }
        }
    }
}

// Wireframe 3D Projection Helper
inline Vec2 Project3DPoint(const Vec3& pt, const Matrix4x4& matProj, int screenW, int screenH) {
    Vec4 p = matProj.MultiplyVector(pt);
    if (p.w != 0.0f) {
        p.x /= p.w;
        p.y /= p.w;
    }
    return Vec2{
        (p.x + 1.0f) * 0.5f * (float)screenW,
        (1.0f - (p.y + 1.0f) * 0.5f) * (float)screenH
    };
}

} // namespace Graphics

#endif
