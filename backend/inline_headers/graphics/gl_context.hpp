// =========================================================================
// gl_context.hpp
// =========================================================================
// The only platform-specific part of the GPU renderer: creating an OpenGL
// 3.3 core context on an existing window, making it current, swapping, and
// looking up GL functions. Everything else (shaders, batching, textures)
// lives in gl_backend.hpp and is identical on every OS.
//
// Nothing here links against OpenGL. The library is opened at run time
// (opengl32.dll / libGL.so.1 / the OpenGL framework), so a machine without
// a GL driver fails Create() cleanly instead of failing to start.
//
// STATUS: Linux/GLX is run and tested. The Windows/WGL code compiles for
// Windows but has not been run on a real Windows machine. macOS/CGL has
// not been compiled or run at all (no Apple headers are available to
// build against).
// =========================================================================
#ifndef GL_CONTEXT_HPP
#define GL_CONTEXT_HPP

#include <string>
#include <cstring>
#include <cstdint>

#if defined(_WIN32) || defined(_WIN64)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace GlContext {

struct Context {
    bool valid = false;
#if defined(_WIN32) || defined(_WIN64)
    HWND hwnd = nullptr;
    HDC dc = nullptr;
    HGLRC rc = nullptr;
    HMODULE lib = nullptr;
    // function pointers into opengl32.dll, kept so getProc can find GL 1.1 functions
    PROC (WINAPI *wglGetProcAddress)(LPCSTR) = nullptr;
    BOOL (WINAPI *wglMakeCurrent)(HDC, HGLRC) = nullptr;
    BOOL (WINAPI *wglDeleteContext)(HGLRC) = nullptr;
#elif defined(__APPLE__)
    void* lib = nullptr;          // dlopen handle of the OpenGL framework
    void* cglContext = nullptr;   // CGLContextObj
    void* nsContext = nullptr;    // NSOpenGLContext (retained)
    void* nsView = nullptr;
    int (*CGLSetCurrentContext)(void*) = nullptr;
    int (*CGLFlushDrawable)(void*) = nullptr;
    int (*CGLSetParameter)(void*, int, const int*) = nullptr;
#else
    // GLX: the display/window/context are owned by the window code that
    // already created them (the visual must match, see TryInitX11Gpu), so
    // this only borrows them.
    void* display = nullptr;
    unsigned long window = 0;
    void* glxContext = nullptr;
    void* lib = nullptr;
    int  (*glXMakeCurrent)(void*, unsigned long, void*) = nullptr;
    void (*glXSwapBuffers)(void*, unsigned long) = nullptr;
    void* (*glXGetProcAddressARB)(const unsigned char*) = nullptr;
#endif
};

// -------------------------------------------------------------------------
// Function lookup for GlBackend::Renderer::Init (signature: void*(const char*, void*)).
// -------------------------------------------------------------------------
inline void* GetProc(const char* name, void* user) {
    Context* c = static_cast<Context*>(user);
#if defined(_WIN32) || defined(_WIN64)
    // wglGetProcAddress only knows extension/1.2+ functions and returns small
    // sentinel values (0, 1, 2, 3, -1) on failure for others. GL 1.0/1.1
    // functions (glClear, glDrawArrays, glTexImage2D...) must come from
    // opengl32.dll itself. Try both, in that order of correctness.
    void* p = c->wglGetProcAddress ? reinterpret_cast<void*>(c->wglGetProcAddress(name)) : nullptr;
    if (p == nullptr || p == (void*)1 || p == (void*)2 || p == (void*)3 || p == (void*)-1)
        p = c->lib ? reinterpret_cast<void*>(GetProcAddress(c->lib, name)) : nullptr;
    return p;
#elif defined(__APPLE__)
    return c->lib ? dlsym(c->lib, name) : nullptr;
#else
    void* p = c->glXGetProcAddressARB ? c->glXGetProcAddressARB(reinterpret_cast<const unsigned char*>(name)) : nullptr;
    if (!p && c->lib) p = dlsym(c->lib, name);
    return p;
#endif
}

#if defined(_WIN32) || defined(_WIN64)
// -------------------------------------------------------------------------
// WINDOWS (WGL)
// -------------------------------------------------------------------------
inline bool Create(Context& c, HWND hwnd, HDC dc, std::string& err) {
    c.lib = LoadLibraryA("opengl32.dll");
    if (!c.lib) { err = "opengl32.dll not found"; return false; }
    typedef HGLRC (WINAPI *CreateFn)(HDC);
    auto wglCreateContext = reinterpret_cast<CreateFn>(GetProcAddress(c.lib, "wglCreateContext"));
    c.wglGetProcAddress = reinterpret_cast<PROC (WINAPI *)(LPCSTR)>(GetProcAddress(c.lib, "wglGetProcAddress"));
    c.wglMakeCurrent = reinterpret_cast<BOOL (WINAPI *)(HDC, HGLRC)>(GetProcAddress(c.lib, "wglMakeCurrent"));
    c.wglDeleteContext = reinterpret_cast<BOOL (WINAPI *)(HGLRC)>(GetProcAddress(c.lib, "wglDeleteContext"));
    if (!wglCreateContext || !c.wglGetProcAddress || !c.wglMakeCurrent || !c.wglDeleteContext) {
        err = "opengl32.dll is missing wgl functions"; FreeLibrary(c.lib); c.lib = nullptr; return false;
    }

    // A pixel format with a depth buffer and double buffering. SetPixelFormat
    // may only be done ONCE per window, which is why the software path's
    // SetDIBitsToDevice (no pixel format) and this path are mutually exclusive.
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd); pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 32; pfd.cDepthBits = 24; pfd.cStencilBits = 8; pfd.iLayerType = PFD_MAIN_PLANE;
    int fmt = ChoosePixelFormat(dc, &pfd);
    if (!fmt || !SetPixelFormat(dc, fmt, &pfd)) { err = "no usable pixel format"; FreeLibrary(c.lib); c.lib = nullptr; return false; }

    // Step 1: a legacy context, only so wglCreateContextAttribsARB can be fetched.
    HGLRC legacy = wglCreateContext(dc);
    if (!legacy || !c.wglMakeCurrent(dc, legacy)) { err = "wglCreateContext failed"; if (legacy) c.wglDeleteContext(legacy); FreeLibrary(c.lib); c.lib = nullptr; return false; }

    typedef HGLRC (WINAPI *CreateAttribsFn)(HDC, HGLRC, const int*);
    auto createAttribs = reinterpret_cast<CreateAttribsFn>(c.wglGetProcAddress("wglCreateContextAttribsARB"));
    HGLRC core = nullptr;
    if (createAttribs) {
        // WGL_CONTEXT_MAJOR/MINOR_VERSION_ARB = 0x2091/0x2092, PROFILE_MASK = 0x9126, CORE = 1
        const int attribs[] = { 0x2091, 3, 0x2092, 3, 0x9126, 0x00000001, 0 };
        core = createAttribs(dc, nullptr, attribs);
    }
    if (core) {
        c.wglMakeCurrent(nullptr, nullptr);
        c.wglDeleteContext(legacy);
        if (!c.wglMakeCurrent(dc, core)) { err = "could not activate the 3.3 core context"; c.wglDeleteContext(core); FreeLibrary(c.lib); c.lib = nullptr; return false; }
        c.rc = core;
    } else {
        // Driver without the ARB entry point: the legacy context is a compatibility
        // context and on essentially every driver that exposes GL 3.3+ features.
        c.rc = legacy;
    }
    c.hwnd = hwnd; c.dc = dc; c.valid = true;

    // vsync off by default is wrong for a game loop that paces itself, but
    // tearing is worse than a capped frame rate: ask for vsync if available.
    typedef BOOL (WINAPI *SwapIntervalFn)(int);
    if (auto swapInterval = reinterpret_cast<SwapIntervalFn>(c.wglGetProcAddress("wglSwapIntervalEXT"))) swapInterval(1);
    return true;
}
inline bool MakeCurrent(Context& c) { return c.valid && c.wglMakeCurrent(c.dc, c.rc); }
inline void SwapBuffers_(Context& c) { if (c.valid) ::SwapBuffers(c.dc); }
inline void Destroy(Context& c) {
    if (!c.valid) return;
    c.wglMakeCurrent(nullptr, nullptr);
    c.wglDeleteContext(c.rc);
    // The DC belongs to the window (it is released by window_destroy); the library stays loaded.
    c.rc = nullptr; c.valid = false;
}

#elif defined(__APPLE__)
// -------------------------------------------------------------------------
// MACOS (CGL via NSOpenGLContext)
// -------------------------------------------------------------------------
// NOT COMPILED OR RUN: written from the documented API surface only.
// NSOpenGLContext is created through the Objective-C runtime (objc_msgSend),
// the same way the rest of the macOS window code works, so no Objective-C
// compiler or Apple headers are needed to read this.
template <typename Ret, typename... Args> Ret CtxMsg(void* obj, const char* sel, Args... args);

inline bool Create(Context& c, void* nsWindowContentView, std::string& err,
                   void* (*objc_getClass_)(const char*), void* (*sel_registerName_)(const char*), void* (*objc_msgSend_)(void*, void*, ...)) {
    c.lib = dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY | RTLD_LOCAL);
    if (!c.lib) { err = "OpenGL.framework not found"; return false; }
    c.CGLSetCurrentContext = reinterpret_cast<int (*)(void*)>(dlsym(c.lib, "CGLSetCurrentContext"));
    c.CGLFlushDrawable = reinterpret_cast<int (*)(void*)>(dlsym(c.lib, "CGLFlushDrawable"));
    c.CGLSetParameter = reinterpret_cast<int (*)(void*, int, const int*)>(dlsym(c.lib, "CGLSetParameter"));
    if (!c.CGLSetCurrentContext || !c.CGLFlushDrawable) { err = "OpenGL.framework is missing CGL functions"; return false; }

    // Pixel format attributes (NSOpenGLPixelFormatAttribute values):
    //   NSOpenGLPFAOpenGLProfile = 99, NSOpenGLProfileVersion3_2Core = 0x3200,
    //   NSOpenGLPFADoubleBuffer = 5, NSOpenGLPFADepthSize = 12, NSOpenGLPFAColorSize = 8, NSOpenGLPFAAccelerated = 73
    // 3.2 core is the highest core profile macOS exposes below 4.1, and it
    // supports GLSL 330. Without asking for a core profile macOS hands out
    // GL 2.1, which cannot compile the shaders.
    unsigned int attrs[] = { 99, 0x3200, 5, 12, 24, 8, 32, 73, 0 };
    void* pfClass = objc_getClass_("NSOpenGLPixelFormat");
    void* pf = objc_msgSend_(objc_msgSend_(pfClass, sel_registerName_("alloc")), sel_registerName_("initWithAttributes:"), attrs);
    if (!pf) { err = "no accelerated 3.2 core pixel format"; return false; }
    void* ctxClass = objc_getClass_("NSOpenGLContext");
    void* ctx = objc_msgSend_(objc_msgSend_(ctxClass, sel_registerName_("alloc")), sel_registerName_("initWithFormat:shareContext:"), pf, nullptr);
    objc_msgSend_(pf, sel_registerName_("release"));
    if (!ctx) { err = "NSOpenGLContext creation failed"; return false; }
    objc_msgSend_(ctx, sel_registerName_("setView:"), nsWindowContentView);
    objc_msgSend_(ctx, sel_registerName_("makeCurrentContext"));
    c.nsContext = ctx;
    c.nsView = nsWindowContentView;
    c.cglContext = objc_msgSend_(ctx, sel_registerName_("CGLContextObj"));
    c.valid = true;
    int one = 1; if (c.CGLSetParameter) c.CGLSetParameter(c.cglContext, 222 /* kCGLCPSwapInterval */, &one);
    return true;
}
inline bool MakeCurrent(Context& c) { return c.valid && c.CGLSetCurrentContext && c.CGLSetCurrentContext(c.cglContext) == 0; }
inline void SwapBuffers_(Context& c) { if (c.valid && c.CGLFlushDrawable) c.CGLFlushDrawable(c.cglContext); }
inline void Destroy(Context& c) {
    if (!c.valid) return;
    if (c.CGLSetCurrentContext) c.CGLSetCurrentContext(nullptr);
    // NSOpenGLContext was alloc'd by us, so we release it. The caller owns the view.
    // (released by the window code through objc_msgSend; kept out of here to avoid a second runtime dependency)
    c.valid = false;
}

#else
// -------------------------------------------------------------------------
// LINUX / X11 (GLX)
// -------------------------------------------------------------------------
// The window code already creates the GLX window and context (the visual has
// to be chosen before the window exists). This wraps what it created.
inline bool Create(Context& c, void* display, unsigned long window, void* glxContext, std::string& err) {
    c.lib = dlopen("libGL.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (!c.lib) { err = "libGL.so.1 not found"; return false; }
    c.glXMakeCurrent = reinterpret_cast<int (*)(void*, unsigned long, void*)>(dlsym(c.lib, "glXMakeCurrent"));
    c.glXSwapBuffers = reinterpret_cast<void (*)(void*, unsigned long)>(dlsym(c.lib, "glXSwapBuffers"));
    c.glXGetProcAddressARB = reinterpret_cast<void* (*)(const unsigned char*)>(dlsym(c.lib, "glXGetProcAddressARB"));
    if (!c.glXMakeCurrent || !c.glXSwapBuffers || !c.glXGetProcAddressARB) { err = "libGL is missing GLX functions"; dlclose(c.lib); c.lib = nullptr; return false; }
    c.display = display; c.window = window; c.glxContext = glxContext; c.valid = true;
    return true;
}
inline bool MakeCurrent(Context& c) { return c.valid && c.glXMakeCurrent(c.display, c.window, c.glxContext) != 0; }
inline void SwapBuffers_(Context& c) { if (c.valid) c.glXSwapBuffers(c.display, c.window); }
inline void Destroy(Context& c) {
    if (!c.valid) return;
    c.glXMakeCurrent(c.display, 0, nullptr);
    // The GLX context and window are destroyed by window_destroy (it created
    // them). libGL is deliberately NOT unloaded here: it has hooks registered
    // on the Display that XCloseDisplay still has to run, so unloading it
    // before the display closes crashes. Our handle is a second reference to
    // a library the window code already holds; dropping it later is harmless,
    // and the process keeps it for its lifetime.
    c.lib = nullptr;
    c.valid = false;
}
#endif

} // namespace GlContext

#endif // GL_CONTEXT_HPP
