// =========================================================================
// texture_store.hpp
// =========================================================================
// Image ownership + per-face texture binding for the software rasterizer.
//
// Design goals (all deliberate, none incidental):
//
//  1. LEAK PROOF. Every pixel buffer is owned by exactly one
//     std::shared_ptr<ImageBuffer> inside ImageRegistry. A texture
//     binding holds only an image ID, never a copy and never a raw
//     pointer, so:
//       - destroying an image frees its pixels immediately, and any
//         binding that still names it simply resolves to "no texture"
//         (falls back to the flat face color) instead of dangling;
//       - a draw call that is mid-flight when an image is destroyed
//         keeps its shared_ptr snapshot alive for exactly that call
//         and releases it on scope exit (RAII, exception safe);
//       - nothing here uses new/delete/malloc, so there is no code path
//         that can forget to free.
//
//  2. DYNAMIC. There is no "a cube has 6 faces" constant anywhere in
//     this file. A TextureSet is an open map of faceSlot -> binding, and
//     slot 0 (kFaceAll) is the fallback for any face with no explicit
//     slot. Meshes with any face count work the same way as cubes.
//     Per-face UVs are optional; when absent, UVs are generated from the
//     face's own vertex positions (see GenerateFaceUVs), so no face
//     shape is special-cased.
//
//  3. NO NEW LINK-TIME DEPENDENCIES. Pure software sampling over the
//     existing Graphics::ImageBuffer. Nothing to dlopen, nothing to link.
//
// Image pixel format is the engine's existing AARRGGBB uint32_t.
// =========================================================================
#ifndef TEXTURE_STORE_HPP
#define TEXTURE_STORE_HPP

#include <cstdint>
#include <cmath>
#include <memory>
#include <unordered_map>
#include <vector>
#include <algorithm>

#ifndef TEXTURE_STORE_STANDALONE
// Same project-relative path window_management.cpp uses for renderer.hpp,
// so this resolves identically wherever the compiler found THIS file.
#include "renderer.hpp"
#endif

namespace Graphics {

// ImageBuffer comes from renderer.hpp in the engine build. The standalone
// test harness defines TEXTURE_STORE_STANDALONE to get an identical copy
// instead, so this header can be tested without the whole renderer. The
// engine build must NEVER define it (that would redefine the struct).
#ifdef TEXTURE_STORE_STANDALONE
struct ImageBuffer {
    int width = 0;
    int height = 0;
    std::vector<uint32_t> pixels;
};
#endif

// -------------------------------------------------------------------------
// FACE SLOTS
// -------------------------------------------------------------------------
// Slot 0 means "every face that has no explicit binding". Slots 1..N are
// 1-based face indices in the order the caller's face list uses (for a
// cube: the cube's own face order). There is no upper bound baked in.
constexpr int kFaceAll = 0;

// How a texture maps across a face.
enum class TextureWrap : int {
    Clamp = 0,   // UVs outside [0,1] stick to the edge pixel
    Repeat = 1,  // UVs tile
    Mirror = 2   // UVs tile, flipping every other repeat
};

// How a texture is sampled.
enum class TextureFilter : int {
    Nearest = 0,
    Bilinear = 1
};

// One face's texture state. Holds an ID, never pixels.
struct TextureBinding {
    int imageId = -1;                    // -1 = unbound
    float uOffset = 0.0f, vOffset = 0.0f;
    float uScale = 1.0f, vScale = 1.0f;  // >1 tiles more, <1 zooms in
    TextureWrap wrap = TextureWrap::Clamp;
    TextureFilter filter = TextureFilter::Nearest;
};

// An open set of per-face bindings. Value type: copying it copies a few
// small structs, never pixel data.
struct TextureSet {
    std::unordered_map<int, TextureBinding> bindings;

    bool Empty() const { return bindings.empty(); }

    // Explicit slot wins; otherwise kFaceAll; otherwise nullptr.
    const TextureBinding* Resolve(int faceSlot) const {
        auto it = bindings.find(faceSlot);
        if (it != bindings.end() && it->second.imageId >= 0) return &it->second;
        auto all = bindings.find(kFaceAll);
        if (all != bindings.end() && all->second.imageId >= 0) return &all->second;
        return nullptr;
    }
};

// -------------------------------------------------------------------------
// IMAGE REGISTRY (single owner of every pixel buffer)
// -------------------------------------------------------------------------
class ImageRegistry {
public:
    using ImagePtr = std::shared_ptr<const ImageBuffer>;

    // Takes ownership by move. Returns the new image id.
    int Add(ImageBuffer&& img) {
        int id = nextId_++;
        images_[id] = std::make_shared<ImageBuffer>(std::move(img));
        return id;
    }

    // Replaces the pixels of an existing id in place (same id, no new
    // slot). Returns false if the id is unknown.
    bool Replace(int id, ImageBuffer&& img) {
        auto it = images_.find(id);
        if (it == images_.end()) return false;
        it->second = std::make_shared<ImageBuffer>(std::move(img));
        return true;
    }

    // Frees the image now. Safe on unknown ids. Any in-flight draw that
    // already took a snapshot keeps it alive until that draw returns.
    bool Destroy(int id) { return images_.erase(id) > 0; }

    // Snapshot for use during one draw call. nullptr if unknown.
    ImagePtr Acquire(int id) const {
        auto it = images_.find(id);
        return it == images_.end() ? nullptr : ImagePtr(it->second);
    }

    bool Contains(int id) const { return images_.find(id) != images_.end(); }
    size_t Count() const { return images_.size(); }

    // Total bytes of pixel storage currently held. Lets a caller (or a
    // test) verify that destroying images actually gives memory back.
    size_t BytesHeld() const {
        size_t total = 0;
        for (const auto& kv : images_) {
            total += kv.second->pixels.size() * sizeof(uint32_t);
        }
        return total;
    }

    // Frees everything (module unload / full reset).
    void Clear() { images_.clear(); }

    // Read-only peek for existing code paths (DrawImage etc.) that only
    // need the buffer for the duration of one call.
    const ImageBuffer* Peek(int id) const {
        auto it = images_.find(id);
        return it == images_.end() ? nullptr : it->second.get();
    }

private:
    std::unordered_map<int, std::shared_ptr<ImageBuffer>> images_;
    int nextId_ = 1;
};

// -------------------------------------------------------------------------
// SAMPLING
// -------------------------------------------------------------------------
inline float ApplyWrap(float t, TextureWrap wrap) {
    switch (wrap) {
        case TextureWrap::Repeat: {
            t -= std::floor(t);
            return t;
        }
        case TextureWrap::Mirror: {
            float f = t - 2.0f * std::floor(t * 0.5f); // t mod 2, in [0,2)
            return f <= 1.0f ? f : 2.0f - f;
        }
        case TextureWrap::Clamp:
        default:
            return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    }
}

inline uint32_t TexelAt(const ImageBuffer& img, int x, int y) {
    x = x < 0 ? 0 : (x >= img.width ? img.width - 1 : x);
    y = y < 0 ? 0 : (y >= img.height ? img.height - 1 : y);
    return img.pixels[(size_t)y * (size_t)img.width + (size_t)x];
}

inline uint32_t LerpColor(uint32_t a, uint32_t b, float t) {
    auto ch = [&](int shift) {
        float av = (float)((a >> shift) & 0xFF);
        float bv = (float)((b >> shift) & 0xFF);
        return (uint32_t)std::lround(av + (bv - av) * t) & 0xFFu;
    };
    return (ch(24) << 24) | (ch(16) << 16) | (ch(8) << 8) | ch(0);
}

// Samples `img` at (u,v) after applying the binding's transform/wrap.
// Returns AARRGGBB. An empty/short image samples as fully transparent so
// the caller falls back to the face color rather than reading out of
// bounds.
inline uint32_t SampleTexture(const ImageBuffer& img, const TextureBinding& b, float u, float v) {
    if (img.width <= 0 || img.height <= 0 ||
        img.pixels.size() < (size_t)img.width * (size_t)img.height) {
        return 0x00000000u;
    }

    u = ApplyWrap(u * b.uScale + b.uOffset, b.wrap);
    v = ApplyWrap(v * b.vScale + b.vOffset, b.wrap);

    if (b.filter == TextureFilter::Nearest) {
        int x = (int)(u * (float)img.width);
        int y = (int)(v * (float)img.height);
        return TexelAt(img, x, y);
    }

    float fx = u * (float)img.width - 0.5f;
    float fy = v * (float)img.height - 0.5f;
    int x0 = (int)std::floor(fx);
    int y0 = (int)std::floor(fy);
    float tx = fx - (float)x0;
    float ty = fy - (float)y0;
    uint32_t c00 = TexelAt(img, x0,     y0);
    uint32_t c10 = TexelAt(img, x0 + 1, y0);
    uint32_t c01 = TexelAt(img, x0,     y0 + 1);
    uint32_t c11 = TexelAt(img, x0 + 1, y0 + 1);
    return LerpColor(LerpColor(c00, c10, tx), LerpColor(c01, c11, tx), ty);
}

// Multiplies a texel's RGB by a 0..1 shade (the same lighting factor the
// flat path applies via ShadeColor), preserving alpha.
inline uint32_t ShadeTexel(uint32_t texel, float shade) {
    if (shade >= 1.0f) return texel;
    if (shade < 0.0f) shade = 0.0f;
    uint32_t a = texel & 0xFF000000u;
    uint32_t r = (uint32_t)(((texel >> 16) & 0xFF) * shade);
    uint32_t g = (uint32_t)(((texel >> 8) & 0xFF) * shade);
    uint32_t b = (uint32_t)((texel & 0xFF) * shade);
    return a | (r << 16) | (g << 8) | b;
}

// -------------------------------------------------------------------------
// UV GENERATION
// -------------------------------------------------------------------------
// Face-shape-agnostic default UVs for an N-gon (N >= 3), given the face's
// own vertex positions. Builds a local 2D basis from the face's first
// edge and its normal, projects every vertex into it, then normalizes the
// projected bounds to [0,1]. A quad therefore maps the full image across
// the face, a triangle maps its bounding box, and an arbitrary polygon
// does the same -- with no per-shape special cases.
struct UV { float u, v; };

template <typename Vec3T>
inline bool GenerateFaceUVs(const std::vector<Vec3T>& positions, std::vector<UV>& out) {
    out.clear();
    const size_t n = positions.size();
    if (n < 3) return false;

    auto sub = [](const Vec3T& a, const Vec3T& b) { return Vec3T{a.x - b.x, a.y - b.y, a.z - b.z}; };
    auto dot = [](const Vec3T& a, const Vec3T& b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
    auto cross = [](const Vec3T& a, const Vec3T& b) {
        return Vec3T{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    };
    auto norm = [&](const Vec3T& a, bool& ok) {
        float len = std::sqrt(dot(a, a));
        if (len < 1e-8f) { ok = false; return Vec3T{0, 0, 0}; }
        ok = true;
        return Vec3T{a.x / len, a.y / len, a.z / len};
    };

    Vec3T e1 = sub(positions[1], positions[0]);
    Vec3T e2 = sub(positions[2], positions[0]);
    bool ok1 = false, ok2 = false;
    Vec3T axisU = norm(e1, ok1);
    Vec3T normal = norm(cross(e1, e2), ok2);
    if (!ok1 || !ok2) return false; // degenerate face: caller falls back to flat color
    Vec3T axisV = cross(normal, axisU);

    std::vector<UV> raw(n);
    float minU = 1e30f, maxU = -1e30f, minV = 1e30f, maxV = -1e30f;
    for (size_t i = 0; i < n; ++i) {
        Vec3T d = sub(positions[i], positions[0]);
        raw[i].u = dot(d, axisU);
        raw[i].v = dot(d, axisV);
        minU = std::min(minU, raw[i].u); maxU = std::max(maxU, raw[i].u);
        minV = std::min(minV, raw[i].v); maxV = std::max(maxV, raw[i].v);
    }
    float spanU = maxU - minU, spanV = maxV - minV;
    if (spanU < 1e-8f || spanV < 1e-8f) return false;

    out.resize(n);
    for (size_t i = 0; i < n; ++i) {
        out[i].u = (raw[i].u - minU) / spanU;
        // Flip V so image row 0 (top) lines up with the face's "up".
        out[i].v = 1.0f - (raw[i].v - minV) / spanV;
    }
    return true;
}

} // namespace Graphics

#endif // TEXTURE_STORE_HPP
