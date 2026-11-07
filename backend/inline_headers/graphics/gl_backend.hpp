// =========================================================================
// gl_backend.hpp
// =========================================================================
// Hardware-accelerated renderer (OpenGL 3.3 core), loaded entirely at RUN
// TIME. No GL header is included and nothing is linked: every function is
// looked up through a callback the platform layer provides (glXGetProcAddress,
// wglGetProcAddress, dlsym). A machine with no GL, or a driver that is too
// old, makes Init() return false and the caller keeps using the CPU
// rasteriser, which is untouched.
//
// WHAT THIS DOES INSTEAD OF THE CPU PATH
//   The CPU path writes every pixel of every triangle itself. Here the CPU
//   only transforms vertices and appends them to a batch; the GPU does the
//   depth test, texture sampling, and shading. Draw calls are collected and
//   sent in a few large submissions at Flush(), not one per primitive.
//
// OWNERSHIP
//   GlBackend owns every GL object it creates (program, buffers, textures)
//   and deletes them in Shutdown(). It never owns the window or the context;
//   the platform layer does. Shutdown() must run with the context current.
// =========================================================================
#ifndef GL_BACKEND_HPP
#define GL_BACKEND_HPP

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

namespace GlBackend {

// ---- GL types and constants, hand-declared (the headers are never included) ----
typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int          GLint;
typedef int          GLsizei;
typedef unsigned int GLbitfield;
typedef unsigned char GLboolean;
typedef float        GLfloat;
typedef char         GLchar;
typedef ptrdiff_t    GLsizeiptr;
typedef ptrdiff_t    GLintptr;

enum : GLenum {
    GL_FALSE = 0, GL_TRUE = 1,
    GL_TRIANGLES = 0x0004,
    GL_DEPTH_BUFFER_BIT = 0x00000100, GL_COLOR_BUFFER_BIT = 0x00004000,
    GL_DEPTH_TEST = 0x0B71, GL_BLEND = 0x0BE2, GL_CULL_FACE = 0x0B44,
    GL_LESS = 0x0201, GL_LEQUAL = 0x0203,
    GL_SRC_ALPHA = 0x0302, GL_ONE_MINUS_SRC_ALPHA = 0x0303,
    GL_FLOAT = 0x1406, GL_UNSIGNED_BYTE = 0x1401,
    GL_TEXTURE_2D = 0x0DE1, GL_TEXTURE0 = 0x84C0,
    GL_TEXTURE_MIN_FILTER = 0x2801, GL_TEXTURE_MAG_FILTER = 0x2800,
    GL_TEXTURE_WRAP_S = 0x2802, GL_TEXTURE_WRAP_T = 0x2803,
    GL_NEAREST = 0x2600, GL_LINEAR = 0x2601,
    GL_REPEAT = 0x2901, GL_CLAMP_TO_EDGE = 0x812F, GL_MIRRORED_REPEAT = 0x8370,
    GL_RGBA = 0x1908, GL_RGBA8 = 0x8058, GL_BGRA = 0x80E1,
    GL_UNPACK_ALIGNMENT = 0x0CF5,
    GL_ARRAY_BUFFER = 0x8892, GL_STREAM_DRAW = 0x88E0, GL_DYNAMIC_DRAW = 0x88E8,
    GL_FRAGMENT_SHADER = 0x8B30, GL_VERTEX_SHADER = 0x8B31,
    GL_COMPILE_STATUS = 0x8B81, GL_LINK_STATUS = 0x8B82, GL_INFO_LOG_LENGTH = 0x8B84,
    GL_VERSION = 0x1F02, GL_RENDERER = 0x1F01, GL_NO_ERROR = 0
};

// Everything the backend calls, resolved once. A null entry that is
// required makes Init() fail.
struct Functions {
    void (*Clear)(GLbitfield) = nullptr;
    void (*ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
    void (*Viewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
    void (*Enable)(GLenum) = nullptr;
    void (*Disable)(GLenum) = nullptr;
    void (*DepthFunc)(GLenum) = nullptr;
    void (*DepthMask)(GLboolean) = nullptr;
    void (*BlendFunc)(GLenum, GLenum) = nullptr;
    GLenum (*GetError)() = nullptr;
    const unsigned char* (*GetString)(GLenum) = nullptr;
    void (*PixelStorei)(GLenum, GLint) = nullptr;
    void (*GenTextures)(GLsizei, GLuint*) = nullptr;
    void (*DeleteTextures)(GLsizei, const GLuint*) = nullptr;
    void (*BindTexture)(GLenum, GLuint) = nullptr;
    void (*TexParameteri)(GLenum, GLenum, GLint) = nullptr;
    void (*TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) = nullptr;
    void (*ActiveTexture)(GLenum) = nullptr;
    void (*DrawArrays)(GLenum, GLint, GLsizei) = nullptr;
    void (*ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*) = nullptr;
    GLuint (*CreateShader)(GLenum) = nullptr;
    void (*ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*) = nullptr;
    void (*CompileShader)(GLuint) = nullptr;
    void (*GetShaderiv)(GLuint, GLenum, GLint*) = nullptr;
    void (*GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
    void (*DeleteShader)(GLuint) = nullptr;
    GLuint (*CreateProgram)() = nullptr;
    void (*AttachShader)(GLuint, GLuint) = nullptr;
    void (*LinkProgram)(GLuint) = nullptr;
    void (*GetProgramiv)(GLuint, GLenum, GLint*) = nullptr;
    void (*GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
    void (*UseProgram)(GLuint) = nullptr;
    void (*DeleteProgram)(GLuint) = nullptr;
    GLint (*GetUniformLocation)(GLuint, const GLchar*) = nullptr;
    void (*Uniform1i)(GLint, GLint) = nullptr;
    void (*Uniform2f)(GLint, GLfloat, GLfloat) = nullptr;
    void (*GenVertexArrays)(GLsizei, GLuint*) = nullptr;
    void (*BindVertexArray)(GLuint) = nullptr;
    void (*DeleteVertexArrays)(GLsizei, const GLuint*) = nullptr;
    void (*GenBuffers)(GLsizei, GLuint*) = nullptr;
    void (*BindBuffer)(GLenum, GLuint) = nullptr;
    void (*BufferData)(GLenum, GLsizeiptr, const void*, GLenum) = nullptr;
    void (*DeleteBuffers)(GLsizei, const GLuint*) = nullptr;
    void (*EnableVertexAttribArray)(GLuint) = nullptr;
    void (*VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) = nullptr;
};

typedef void* (*GetProcFn)(const char* name, void* user);

// One vertex as the GPU sees it: screen-space x/y in PIXELS (origin top-left),
// a depth value, texture coordinates and an RGBA colour. Positions are
// already projected on the CPU for now, which keeps the maths identical to
// the software path while the shaders, depth and texturing move to the GPU.
struct Vertex {
    float x, y;       // pixels, origin top-left
    float z;          // view-space depth (smaller = closer)
    float invW;       // 1 / view-space depth, for perspective-correct UVs
    float u, v;       // texture coordinates
    float r, g, b, a; // flat colour (already lit); multiplied with the texel when textured
    float texMix;     // 0 = flat colour, 1 = texture * colour
};

class Renderer {
public:
    Renderer() = default;
    ~Renderer() { /* Shutdown must be called with the context current; see header. */ }
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool Init(GetProcFn getProc, void* user, int width, int height, std::string& err);
    void Shutdown();                       // frees every GL object; context must be current
    bool Ready() const { return ready_; }

    void Resize(int width, int height);
    void BeginFrame(float r, float g, float b);
    // Camera planes used to turn view-space z into a depth value. Call per
    // camera before queueing its geometry; a Flush() in between keeps earlier
    // geometry on its own planes.
    void SetDepthRange(float nearPlane, float farPlane) { Flush(); near_ = nearPlane; far_ = farPlane; }
    // Queues triangles (3 vertices each). `texture` is a GL texture name
    // from UploadTexture, or 0 for flat colour.
    void AddTriangles(const Vertex* v, size_t count, GLuint texture);
    void Flush();                          // sends everything queued so far
    void FlushOverlay();                   // same, but without writing depth (a 2D layer over the scene)
    // Copies the framebuffer into `bgra` (width*height 32-bit, top row first).
    bool ReadBack(std::vector<uint32_t>& bgra);

    // Textures: returns a GL texture name, 0 on failure. `pixels` is AARRGGBB.
    GLuint UploadTexture(const uint32_t* pixels, int w, int h, bool linear, GLenum wrap);
    void DeleteTexture(GLuint tex);

    const std::string& Description() const { return description_; }
    size_t DrawCalls() const { return drawCalls_; }
    size_t TrianglesSent() const { return trianglesSent_; }

private:
    struct Batch { GLuint texture; size_t first; size_t count; };

    Functions gl_;
    bool ready_ = false;
    int width_ = 0, height_ = 0;
    GLuint program_ = 0, vao_ = 0, vbo_ = 0;
    GLint locViewport_ = -1, locSampler_ = -1, locNearFar_ = -1;
    float near_ = 0.1f, far_ = 100.0f;
    std::vector<Vertex> verts_;
    std::vector<Batch> batches_;
    std::vector<GLuint> textures_;         // every texture we created, so Shutdown frees them all
    std::string description_;
    size_t drawCalls_ = 0, trianglesSent_ = 0;

    bool CompileProgram(std::string& err);
    GLuint Compile(GLenum type, const char* src, std::string& err);
};

// -------------------------------------------------------------------------
// IMPLEMENTATION
// -------------------------------------------------------------------------
// GLSL 3.30 core. Vertex positions arrive in pixels and are turned into
// clip space here, so the CPU does not need a projection matrix per batch.
// Depth is written from the supplied view-space depth, normalised, so
// depth testing matches the software path (smaller z wins).
static const char* kVertexShader = R"GLSL(#version 330 core
layout(location = 0) in vec3  aPosZ;      // x, y in pixels; z = view depth
layout(location = 1) in float aInvW;
layout(location = 2) in vec2  aUV;
layout(location = 3) in vec4  aColor;
layout(location = 4) in float aTexMix;
uniform vec2 uViewport;                   // width, height in pixels
uniform vec2 uNearFar;                    // camera near and far planes (view-space distance)
out vec2  vUV;       // u/w, v/w : interpolated linearly, divided per pixel
out float vInvW;
out vec4  vColor;
out float vTexMix;
void main() {
    float ndcX = aPosZ.x / uViewport.x * 2.0 - 1.0;
    float ndcY = 1.0 - aPosZ.y / uViewport.y * 2.0;   // pixel y grows downward
    // Standard perspective depth in [0,1]: far/(far-near) * (1 - near/z).
    // Monotonic in z (nearer = smaller, same ordering as the CPU depth
    // buffer) and linear in screen space, so it interpolates correctly
    // across a triangle. A flat 2D overlay passes z <= 0 and lands in front.
    float z = aPosZ.z;
    float d = (z <= 0.0) ? 0.0 : clamp(uNearFar.y / (uNearFar.y - uNearFar.x) * (1.0 - uNearFar.x / z), 0.0, 1.0);
    // w = 1 keeps varyings affine in screen space; perspective correction is
    // done by hand below with 1/w, matching the CPU path.
    gl_Position = vec4(ndcX, ndcY, d * 2.0 - 1.0, 1.0);
    vUV = aUV * aInvW;
    vInvW = aInvW;
    vColor = aColor;
    vTexMix = aTexMix;
}
)GLSL";

static const char* kFragmentShader = R"GLSL(#version 330 core
in vec2  vUV;
in float vInvW;
in vec4  vColor;
in float vTexMix;
uniform sampler2D uTex;
out vec4 outColor;
void main() {
    vec4 c = vColor;
    if (vTexMix > 0.5) {
        vec2 uv = vUV / vInvW;                 // perspective-correct, one divide per pixel
        c = texture(uTex, uv) * vColor;
        if (c.a <= 0.0) discard;               // same rule as the CPU path: alpha 0 draws nothing
    }
    outColor = c;
}
)GLSL";

inline GLuint Renderer::Compile(GLenum type, const char* src, std::string& err) {
    GLuint s = gl_.CreateShader(type);
    if (!s) { err = "glCreateShader failed"; return 0; }
    gl_.ShaderSource(s, 1, &src, nullptr);
    gl_.CompileShader(s);
    GLint ok = 0;
    gl_.GetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        gl_.GetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::string log((size_t)(len > 1 ? len : 1), '\0');
        gl_.GetShaderInfoLog(s, (GLsizei)log.size(), nullptr, &log[0]);
        err = std::string(type == GL_VERTEX_SHADER ? "vertex" : "fragment") + " shader: " + log.c_str();
        gl_.DeleteShader(s);
        return 0;
    }
    return s;
}

inline bool Renderer::CompileProgram(std::string& err) {
    GLuint vs = Compile(GL_VERTEX_SHADER, kVertexShader, err);
    if (!vs) return false;
    GLuint fs = Compile(GL_FRAGMENT_SHADER, kFragmentShader, err);
    if (!fs) { gl_.DeleteShader(vs); return false; }
    program_ = gl_.CreateProgram();
    gl_.AttachShader(program_, vs);
    gl_.AttachShader(program_, fs);
    gl_.LinkProgram(program_);
    gl_.DeleteShader(vs);       // owned by the program once linked
    gl_.DeleteShader(fs);
    GLint ok = 0;
    gl_.GetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        gl_.GetProgramiv(program_, GL_INFO_LOG_LENGTH, &len);
        std::string log((size_t)(len > 1 ? len : 1), '\0');
        gl_.GetProgramInfoLog(program_, (GLsizei)log.size(), nullptr, &log[0]);
        err = std::string("link: ") + log.c_str();
        gl_.DeleteProgram(program_);
        program_ = 0;
        return false;
    }
    locViewport_ = gl_.GetUniformLocation(program_, "uViewport");
    locSampler_ = gl_.GetUniformLocation(program_, "uTex");
    locNearFar_ = gl_.GetUniformLocation(program_, "uNearFar");
    return true;
}

inline bool Renderer::Init(GetProcFn getProc, void* user, int width, int height, std::string& err) {
    if (ready_) return true;
    if (width <= 0 || height <= 0) { err = "bad size"; return false; }

    bool missing = false;
    std::string missingName;
#define GLB_LOAD(member, name) do { \
        gl_.member = reinterpret_cast<decltype(gl_.member)>(getProc(name, user)); \
        if (!gl_.member) { missing = true; if (missingName.empty()) missingName = name; } \
    } while (0)
    GLB_LOAD(Clear, "glClear"); GLB_LOAD(ClearColor, "glClearColor"); GLB_LOAD(Viewport, "glViewport");
    GLB_LOAD(Enable, "glEnable"); GLB_LOAD(Disable, "glDisable"); GLB_LOAD(DepthFunc, "glDepthFunc");
    GLB_LOAD(DepthMask, "glDepthMask"); GLB_LOAD(BlendFunc, "glBlendFunc"); GLB_LOAD(GetError, "glGetError");
    GLB_LOAD(GetString, "glGetString"); GLB_LOAD(PixelStorei, "glPixelStorei");
    GLB_LOAD(GenTextures, "glGenTextures"); GLB_LOAD(DeleteTextures, "glDeleteTextures");
    GLB_LOAD(BindTexture, "glBindTexture"); GLB_LOAD(TexParameteri, "glTexParameteri"); GLB_LOAD(TexImage2D, "glTexImage2D");
    GLB_LOAD(ActiveTexture, "glActiveTexture"); GLB_LOAD(DrawArrays, "glDrawArrays"); GLB_LOAD(ReadPixels, "glReadPixels");
    GLB_LOAD(CreateShader, "glCreateShader"); GLB_LOAD(ShaderSource, "glShaderSource"); GLB_LOAD(CompileShader, "glCompileShader");
    GLB_LOAD(GetShaderiv, "glGetShaderiv"); GLB_LOAD(GetShaderInfoLog, "glGetShaderInfoLog"); GLB_LOAD(DeleteShader, "glDeleteShader");
    GLB_LOAD(CreateProgram, "glCreateProgram"); GLB_LOAD(AttachShader, "glAttachShader"); GLB_LOAD(LinkProgram, "glLinkProgram");
    GLB_LOAD(GetProgramiv, "glGetProgramiv"); GLB_LOAD(GetProgramInfoLog, "glGetProgramInfoLog"); GLB_LOAD(UseProgram, "glUseProgram");
    GLB_LOAD(DeleteProgram, "glDeleteProgram"); GLB_LOAD(GetUniformLocation, "glGetUniformLocation"); GLB_LOAD(Uniform1i, "glUniform1i"); GLB_LOAD(Uniform2f, "glUniform2f");
    GLB_LOAD(GenVertexArrays, "glGenVertexArrays"); GLB_LOAD(BindVertexArray, "glBindVertexArray"); GLB_LOAD(DeleteVertexArrays, "glDeleteVertexArrays");
    GLB_LOAD(GenBuffers, "glGenBuffers"); GLB_LOAD(BindBuffer, "glBindBuffer"); GLB_LOAD(BufferData, "glBufferData");
    GLB_LOAD(DeleteBuffers, "glDeleteBuffers"); GLB_LOAD(EnableVertexAttribArray, "glEnableVertexAttribArray");
    GLB_LOAD(VertexAttribPointer, "glVertexAttribPointer");
#undef GLB_LOAD
    if (missing) { err = "OpenGL 3.3 entry point missing: " + missingName; return false; }

    const unsigned char* ver = gl_.GetString(GL_VERSION);
    const unsigned char* ren = gl_.GetString(GL_RENDERER);
    description_ = std::string(ren ? (const char*)ren : "?") + " / OpenGL " + (ver ? (const char*)ver : "?");

    if (!CompileProgram(err)) return false;

    gl_.GenVertexArrays(1, &vao_);
    gl_.GenBuffers(1, &vbo_);
    gl_.BindVertexArray(vao_);
    gl_.BindBuffer(GL_ARRAY_BUFFER, vbo_);
    const GLsizei stride = (GLsizei)sizeof(Vertex);
    gl_.EnableVertexAttribArray(0); gl_.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (const void*)offsetof(Vertex, x));
    gl_.EnableVertexAttribArray(1); gl_.VertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride, (const void*)offsetof(Vertex, invW));
    gl_.EnableVertexAttribArray(2); gl_.VertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (const void*)offsetof(Vertex, u));
    gl_.EnableVertexAttribArray(3); gl_.VertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (const void*)offsetof(Vertex, r));
    gl_.EnableVertexAttribArray(4); gl_.VertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (const void*)offsetof(Vertex, texMix));
    gl_.BindVertexArray(0);

    width_ = width; height_ = height;
    ready_ = true;
    return true;
}

inline void Renderer::Shutdown() {
    if (!ready_) return;
    for (GLuint t : textures_) gl_.DeleteTextures(1, &t);
    textures_.clear();
    if (vbo_) gl_.DeleteBuffers(1, &vbo_);
    if (vao_) gl_.DeleteVertexArrays(1, &vao_);
    if (program_) gl_.DeleteProgram(program_);
    vbo_ = vao_ = program_ = 0;
    verts_.clear(); batches_.clear();
    ready_ = false;
}

inline void Renderer::Resize(int width, int height) {
    if (!ready_ || width <= 0 || height <= 0) return;
    width_ = width; height_ = height;
}

inline void Renderer::BeginFrame(float r, float g, float b) {
    if (!ready_) return;
    gl_.Viewport(0, 0, width_, height_);
    gl_.ClearColor(r, g, b, 1.0f);
    gl_.Enable(GL_DEPTH_TEST);
    gl_.DepthFunc(GL_LESS);
    gl_.DepthMask(GL_TRUE);
    gl_.Disable(GL_CULL_FACE);       // faces are drawn from both sides, like the CPU path
    gl_.Enable(GL_BLEND);
    gl_.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl_.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    verts_.clear();
    batches_.clear();
    drawCalls_ = 0;
    trianglesSent_ = 0;
}

inline void Renderer::AddTriangles(const Vertex* v, size_t count, GLuint texture) {
    if (!ready_ || !v || count == 0 || count % 3 != 0) return;
    size_t first = verts_.size();
    verts_.insert(verts_.end(), v, v + count);
    // Merge into the previous batch when the texture matches: fewer draw calls.
    if (!batches_.empty() && batches_.back().texture == texture && batches_.back().first + batches_.back().count == first)
        batches_.back().count += count;
    else
        batches_.push_back({texture, first, count});
}

inline void Renderer::Flush() {
    if (!ready_ || verts_.empty()) return;
    gl_.UseProgram(program_);
    gl_.Uniform2f(locViewport_, (GLfloat)width_, (GLfloat)height_);
    gl_.Uniform1i(locSampler_, 0);
    gl_.Uniform2f(locNearFar_, near_, far_);
    gl_.BindVertexArray(vao_);
    gl_.BindBuffer(GL_ARRAY_BUFFER, vbo_);
    // One upload per frame. BufferData with the exact size also "orphans" the
    // old storage, so the driver can give back fresh memory instead of making
    // us wait for last frame's draw to finish reading the old one.
    gl_.BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts_.size() * sizeof(Vertex)), verts_.data(), GL_STREAM_DRAW);
    gl_.ActiveTexture(GL_TEXTURE0);
    for (const Batch& b : batches_) {
        gl_.BindTexture(GL_TEXTURE_2D, b.texture);
        gl_.DrawArrays(GL_TRIANGLES, (GLint)b.first, (GLsizei)b.count);
        ++drawCalls_;
        trianglesSent_ += b.count / 3;
    }
    gl_.BindVertexArray(0);
    verts_.clear();
    batches_.clear();
}

inline void Renderer::FlushOverlay() {
    if (!ready_ || verts_.empty()) return;
    gl_.DepthMask(GL_FALSE);
    Flush();
    gl_.DepthMask(GL_TRUE);
}

inline bool Renderer::ReadBack(std::vector<uint32_t>& bgra) {
    if (!ready_) return false;
    bgra.assign((size_t)width_ * height_, 0);
    gl_.PixelStorei(0x0D05 /* GL_PACK_ALIGNMENT */, 4);
    gl_.ReadPixels(0, 0, width_, height_, GL_BGRA, GL_UNSIGNED_BYTE, bgra.data());
    // GL rows run bottom-up; flip to top-row-first like the CPU canvas.
    for (int y = 0; y < height_ / 2; ++y)
        std::swap_ranges(bgra.begin() + (size_t)y * width_, bgra.begin() + (size_t)(y + 1) * width_, bgra.begin() + (size_t)(height_ - 1 - y) * width_);
    return true;
}

inline GLuint Renderer::UploadTexture(const uint32_t* pixels, int w, int h, bool linear, GLenum wrap) {
    if (!ready_ || !pixels || w <= 0 || h <= 0) return 0;
    GLuint tex = 0;
    gl_.GenTextures(1, &tex);
    if (!tex) return 0;
    gl_.BindTexture(GL_TEXTURE_2D, tex);
    gl_.PixelStorei(GL_UNPACK_ALIGNMENT, 4);
    gl_.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? (GLint)GL_LINEAR : (GLint)GL_NEAREST);
    gl_.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? (GLint)GL_LINEAR : (GLint)GL_NEAREST);
    gl_.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint)wrap);
    gl_.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLint)wrap);
    // Our pixels are 0xAARRGGBB in a uint32 = bytes B,G,R,A in memory on little-endian machines, which is GL_BGRA.
    gl_.TexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
    textures_.push_back(tex);
    return tex;
}

inline void Renderer::DeleteTexture(GLuint tex) {
    if (!ready_ || !tex) return;
    gl_.DeleteTextures(1, &tex);
    for (size_t i = 0; i < textures_.size(); ++i)
        if (textures_[i] == tex) { textures_[i] = textures_.back(); textures_.pop_back(); break; }
}

} // namespace GlBackend

#endif // GL_BACKEND_HPP
