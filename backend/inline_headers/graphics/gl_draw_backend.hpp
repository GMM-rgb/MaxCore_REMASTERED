// =========================================================================
// gl_draw_backend.hpp : DrawBackend over the OpenGL 3.3 renderer
// =========================================================================
#ifndef GL_DRAW_BACKEND_HPP
#define GL_DRAW_BACKEND_HPP

#include <memory>
#include "draw_backend.hpp"
#include "gl_backend.hpp"
#include "gl_context.hpp"

namespace DrawBackendNS {

class GlDrawBackend final : public DrawBackend {
public:
    // The platform layer has already created `ctx` (window + GL context). On failure Create returns null and `err` says why.
    static std::unique_ptr<GlDrawBackend> Create(GlContext::Context&& ctx, int w, int h, std::string& err) {
        std::unique_ptr<GlDrawBackend> be(new GlDrawBackend());
        be->ctx_ = ctx;
        if (!GlContext::MakeCurrent(be->ctx_)) { err = "could not make the GL context current"; return nullptr; }
        if (!be->gl_.Init(GlContext::GetProc, &be->ctx_, w, h, err)) { GlContext::Destroy(be->ctx_); return nullptr; }
        be->w_ = w; be->h_ = h;
        return be;
    }
    ~GlDrawBackend() override { Shutdown(); }

    // Must run with the window and display still alive (before the platform layer destroys them).
    void Shutdown() override {
        if (!ctx_.valid) return;
        if (GlContext::MakeCurrent(ctx_)) { DropAllTextures(); if (overlay_) gl_.DeleteTexture(overlay_); overlay_ = 0; gl_.Shutdown(); }
        GlContext::Destroy(ctx_);
    }

    const char* Name() const override { return "opengl"; }
    std::string Description() const override { return "opengl: " + gl_.Description(); }

    void Resize(int w, int h) override { w_ = w; h_ = h; gl_.Resize(w, h); }
    void BeginFrame(float r, float g, float b) override { GlContext::MakeCurrent(ctx_); gl_.BeginFrame(r, g, b); }
    void SetDepthRange(float n, float f) override { GlContext::MakeCurrent(ctx_); gl_.SetDepthRange(n, f); }
    void Triangles(const Vertex* v, size_t count, TextureId tex) override {
        static_assert(sizeof(Vertex) == sizeof(GlBackend::Vertex), "vertex layouts must match");
        gl_.AddTriangles(reinterpret_cast<const GlBackend::Vertex*>(v), count, (GlBackend::GLuint)tex);
    }

    void Present(const uint32_t* canvas, int w, int h, bool has2D) override {
        if (!GlContext::MakeCurrent(ctx_)) return;
        gl_.Flush();
        if (has2D && canvas && CanvasHasContent(canvas, (size_t)w * h)) {
            if (overlay_) gl_.DeleteTexture(overlay_);            // re-filled from this frame's canvas
            overlay_ = gl_.UploadTexture(canvas, w, h, false, GlBackend::GL_CLAMP_TO_EDGE);
            if (overlay_) {
                auto mk = [&](float x, float y, float u, float v) { GlBackend::Vertex g = {}; g.x = x; g.y = y; g.z = 0; g.invW = 1; g.u = u; g.v = v; g.r = g.g = g.b = g.a = 1; g.texMix = 1; return g; };
                const float fw = (float)w, fh = (float)h;
                GlBackend::Vertex q[6] = {mk(0, 0, 0, 0), mk(fw, 0, 1, 0), mk(fw, fh, 1, 1), mk(0, 0, 0, 0), mk(fw, fh, 1, 1), mk(0, fh, 0, 1)};
                gl_.AddTriangles(q, 6, overlay_);
                gl_.FlushOverlay();
            }
        }
        GlContext::SwapBuffers_(ctx_);
    }

    bool ReadFrame(std::vector<uint32_t>& out) override { GlContext::MakeCurrent(ctx_); return gl_.ReadBack(out); }

protected:
    TextureId UploadTexture(const Graphics::ImageBuffer& img, const Graphics::TextureBinding& b) override {
        GlContext::MakeCurrent(ctx_);
        GlBackend::GLenum wrap = GlBackend::GL_CLAMP_TO_EDGE;
        if (b.wrap == Graphics::TextureWrap::Repeat) wrap = GlBackend::GL_REPEAT;
        else if (b.wrap == Graphics::TextureWrap::Mirror) wrap = GlBackend::GL_MIRRORED_REPEAT;
        return gl_.UploadTexture(img.pixels.data(), img.width, img.height, b.filter == Graphics::TextureFilter::Bilinear, wrap);
    }
    void DeleteTexture(TextureId t) override { GlContext::MakeCurrent(ctx_); gl_.DeleteTexture((GlBackend::GLuint)t); }

private:
    GlDrawBackend() = default;
    GlContext::Context ctx_;
    GlBackend::Renderer gl_;
    GlBackend::GLuint overlay_ = 0;
    int w_ = 0, h_ = 0;
};

} // namespace DrawBackendNS

#endif // GL_DRAW_BACKEND_HPP
