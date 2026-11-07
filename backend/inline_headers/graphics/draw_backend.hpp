// =========================================================================
// draw_backend.hpp
// =========================================================================
// One interface for every way a window can draw its 3D scene. The engine's
// draw functions talk ONLY to DrawBackend; which backend sits behind it is
// chosen per window (by name) and can be changed at run time.
//
//   "opengl"   OpenGL 3.3 core, loaded at run time           (GlDrawBackend)
//   "vulkan"   Vulkan 1.1, loaded at run time                (VulkanDrawBackend)
//   "software" the engine's tiled, multi-threaded, SIMD CPU  (SoftDrawBackend)
//   "cpu"      the original single-threaded rasteriser       (no DrawBackend:
//                                                             the window draws into its own canvas)
//
// WHAT IS SHARED (written once, here): the texture cache keyed by image id +
// version, the "is there any 2D this frame" check, and the 2D overlay
// compositing. A backend implements only the primitive operations.
//
// THE FRAME
//   BeginFrame -> any number of SetDepthRange / Triangles -> Present(canvas)
//   The canvas is the 2D layer (transparent where nothing was drawn). It is
//   composited OVER the 3D picture.
// =========================================================================
#ifndef DRAW_BACKEND_HPP
#define DRAW_BACKEND_HPP

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "texture_store.hpp"

namespace DrawBackendNS {

// One vertex as every backend receives it (same layout the GL path always used).
struct Vertex {
    float x, y;       // pixels, origin top-left
    float z;          // view-space depth, smaller = closer; <= 0 means the 2D layer (always in front)
    float invW;       // 1 / z
    float u, v;       // texture coordinates (binding scale/offset already applied)
    float r, g, b, a; // colour, 0..1, already lit
    float texMix;     // 0 = flat colour, 1 = texture * colour
};

using TextureId = uint32_t;           // backend-defined handle; 0 = none

class DrawBackend {
public:
    virtual ~DrawBackend() = default;

    virtual const char* Name() const = 0;                 // "opengl", "vulkan", "software"
    // Frees every native object this backend made (GL objects need their context, Vulkan its device). Safe to call
    // twice. The window calls it BEFORE it destroys the OS window, then drops the backend object.
    virtual void Shutdown() {}
    // The latest finished frame as AARRGGBB, top row first, if this backend can provide it (used for screenshots/tests).
    virtual bool ReadFrame(std::vector<uint32_t>&) { return false; }
    virtual std::string Description() const { return Name(); }

    virtual void Resize(int w, int h) = 0;
    virtual void BeginFrame(float r, float g, float b) = 0;
    virtual void SetDepthRange(float nearPlane, float farPlane) = 0;
    virtual void Triangles(const Vertex* v, size_t count, TextureId tex) = 0;   // count % 3 == 0
    // Called once per frame with the 2D layer (AARRGGBB, transparent = alpha 0). Draws it over the scene and shows the frame.
    virtual void Present(const uint32_t* canvas, int w, int h, bool has2D) = 0;

    // ---- texture cache (shared logic, backend supplies Upload/Delete) ----
    // Returns the backend texture for an image, uploading only when the pixels
    // or sampler settings changed since last time. 0 on failure.
    TextureId TextureFor(int imageId, uint64_t version, const Graphics::ImageBuffer& img, const Graphics::TextureBinding& b) {
        const int filter = b.filter == Graphics::TextureFilter::Bilinear ? 1 : 0;
        const int wrap = (int)b.wrap;
        auto it = cache_.find(imageId);
        if (it != cache_.end() && it->second.version == version && it->second.filter == filter && it->second.wrap == wrap) return it->second.tex;
        TextureId t = UploadTexture(img, b);
        if (!t) return 0;
        if (it != cache_.end()) DeleteTexture(it->second.tex);       // superseded: free only after the new one exists
        cache_[imageId] = {t, version, filter, wrap};
        return t;
    }
    void ForgetImage(int imageId) {
        auto it = cache_.find(imageId);
        if (it == cache_.end()) return;
        DeleteTexture(it->second.tex);
        cache_.erase(it);
    }
    // Frees every cached texture. Backends call this from Shutdown while their context is valid.
    void DropAllTextures() {
        for (auto& kv : cache_) DeleteTexture(kv.second.tex);
        cache_.clear();
    }
    size_t CachedTextures() const { return cache_.size(); }

protected:
    virtual TextureId UploadTexture(const Graphics::ImageBuffer& img, const Graphics::TextureBinding& b) = 0;
    virtual void DeleteTexture(TextureId t) = 0;

private:
    struct Entry { TextureId tex; uint64_t version; int filter; int wrap; };
    std::unordered_map<int, Entry> cache_;
};

// True if any pixel of the 2D layer has alpha. Stops at the first one, so a busy canvas costs almost nothing.
inline bool CanvasHasContent(const uint32_t* px, size_t n) {
    size_t i = 0;
    while (i < n && (px[i] >> 24) == 0) ++i;
    return i != n;
}

} // namespace DrawBackendNS

#endif // DRAW_BACKEND_HPP
