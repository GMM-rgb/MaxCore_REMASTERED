// =========================================================================
// soft_backend.hpp : DrawBackend over the tiled multi-threaded software renderer
// =========================================================================
#ifndef SOFT_BACKEND_HPP
#define SOFT_BACKEND_HPP

#include <functional>
#include <vector>
#include "draw_backend.hpp"
#include "soft_renderer.hpp"

namespace DrawBackendNS {

// Shows a finished frame (w*h AARRGGBB, top row first) in the window. Supplied by the window code,
// which already knows how to put pixels on screen on each OS.
using ShowFrameFn = std::function<void(const uint32_t* pixels, int w, int h)>;

class SoftDrawBackend final : public DrawBackend {
public:
    SoftDrawBackend(int w, int h, unsigned threads, ShowFrameFn show) : renderer_(threads), show_(std::move(show)) { Resize(w, h); }
    ~SoftDrawBackend() override { DropAllTextures(); }

    const char* Name() const override { return "software"; }
    std::string Description() const override { return "software (" + std::to_string(renderer_.Threads()) + " threads" + (SimdSample::HasAvx2() && SoftRender::SimdEnabled() ? ", AVX2)" : ")"); }

    void Resize(int w, int h) override {
        if (w <= 0 || h <= 0) return;
        w_ = w; h_ = h;
        color_.assign((size_t)w * h, 0xFF181818u);
        depth_.assign((size_t)w * h, SoftRender::kFar);
    }

    void BeginFrame(float r, float g, float b) override {
        auto to8 = [](float f) { int v = (int)(f * 255.0f + 0.5f); return (uint32_t)(v < 0 ? 0 : v > 255 ? 255 : v); };
        std::fill(color_.begin(), color_.end(), 0xFF000000u | (to8(r) << 16) | (to8(g) << 8) | to8(b));
        std::fill(depth_.begin(), depth_.end(), SoftRender::kFar);
        pending_ = 0;
        std::fill(frameOf_.begin(), frameOf_.end(), -1);
    }

    // The software renderer takes z as view-space distance already, so the depth range is not needed.
    void SetDepthRange(float, float) override {}

    void Triangles(const Vertex* v, size_t count, TextureId tex) override {
        if (!v || count == 0 || count % 3 != 0) return;
        static_assert(sizeof(Vertex) == sizeof(SoftRender::Vertex), "vertex layouts must match");
        // `tex` is the id TextureFor returned. The tiled renderer wants textures registered per frame, so
        // translate on first use this frame (callers never need to know which backend they are talking to).
        int frameIndex = -1;
        if (tex && tex <= stored_.size() && stored_[tex - 1].image) {
            if (frameOf_.size() < stored_.size()) frameOf_.resize(stored_.size(), -1);
            if (frameOf_[tex - 1] < 0) frameOf_[tex - 1] = renderer_.AddTexture(stored_[tex - 1].image, stored_[tex - 1].binding);
            frameIndex = frameOf_[tex - 1];
        }
        renderer_.AddTriangles(reinterpret_cast<const SoftRender::Vertex*>(v), count, frameIndex);
        pending_ += count / 3;
    }

    void Present(const uint32_t* canvas, int w, int h, bool has2D) override {
        renderer_.Flush({color_.data(), depth_.data(), w_, h_});
        if (has2D && canvas && w == w_ && h == h_) Composite(canvas);
        if (show_) show_(color_.data(), w_, h_);
        std::fill(frameOf_.begin(), frameOf_.end(), -1);          // the renderer cleared its per-frame list in Flush
    }

    bool ReadFrame(std::vector<uint32_t>& out) override { out = color_; return true; }
    const std::vector<uint32_t>& Frame() const { return color_; }
    unsigned Threads() const { return renderer_.Threads(); }

protected:
    // Textures live in the shared registry as snapshots, so "upload" just remembers a shared pointer.
    TextureId UploadTexture(const Graphics::ImageBuffer& img, const Graphics::TextureBinding& b) override {
        stored_.push_back({std::make_shared<Graphics::ImageBuffer>(img), b});
        return (TextureId)stored_.size();
    }
    void DeleteTexture(TextureId t) override { if (t && t <= stored_.size()) stored_[t - 1].image.reset(); }

private:
    // 2D layer over the 3D picture: normal "source over" with straight alpha.
    void Composite(const uint32_t* canvas) {
        const size_t n = (size_t)w_ * h_;
        for (size_t i = 0; i < n; ++i) {
            const uint32_t s = canvas[i]; const uint32_t a = s >> 24;
            if (a == 0) continue;
            if (a == 255) { color_[i] = s | 0xFF000000u; continue; }
            const uint32_t d = color_[i], ia = 255 - a;
            const uint32_t r = (((s >> 16) & 255) * a + ((d >> 16) & 255) * ia + 127) / 255;
            const uint32_t g = (((s >> 8) & 255) * a + ((d >> 8) & 255) * ia + 127) / 255;
            const uint32_t b = ((s & 255) * a + (d & 255) * ia + 127) / 255;
            color_[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }

    SoftRender::Renderer renderer_;
    ShowFrameFn show_;
    int w_ = 0, h_ = 0;
    std::vector<uint32_t> color_;
    std::vector<float> depth_;
    std::vector<SoftRender::Texture> stored_;
    std::vector<int> frameOf_;          // stored texture index -> this frame's renderer index, -1 = not registered yet
    size_t pending_ = 0;
};

} // namespace DrawBackendNS

#endif // SOFT_BACKEND_HPP
