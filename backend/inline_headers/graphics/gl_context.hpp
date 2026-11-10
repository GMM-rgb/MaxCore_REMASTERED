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
// STATUS: Linux/GLX has been tested in a cloud environment over an
// extended period. Windows/WGL has been tested on a physical Windows
// machine with an NVIDIA GeForce RTX 4060 Ti (OpenGL 3.3.0, driver
// 617.14). macOS/CGL is currently being implemented and has not yet
// been verified.
//
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

    PROC (WINAPI *wglGetProcAddress)(LPCSTR) = nullptr;
    BOOL (WINAPI *wglMakeCurrent)(HDC, HGLRC) = nullptr;
    BOOL (WINAPI *wglDeleteContext)(HGLRC) = nullptr;
    HGLRC (WINAPI *wglGetCurrentContext)() = nullptr;

#elif defined(__APPLE__)

    void* lib = nullptr;
    void* cglContext = nullptr;
    void* nsContext = nullptr;
    void* nsView = nullptr;

    int   (*CGLSetCurrentContext)(void*) = nullptr;
    void* (*CGLGetCurrentContext)() = nullptr;
    int   (*CGLFlushDrawable)(void*) = nullptr;
    int   (*CGLSetParameter)(void*, int, const int*) = nullptr;

    // Objective-C object cleanup.
    void (*objcRelease)(void*, void*) = nullptr;
    void* releaseSelector = nullptr;

#else

    // GLX resources are borrowed from the window system.
    void* display = nullptr;
    unsigned long window = 0;
    void* glxContext = nullptr;
    void* lib = nullptr;

    int   (*glXMakeCurrent)(void*, unsigned long, void*) = nullptr;
    void  (*glXSwapBuffers)(void*, unsigned long) = nullptr;
    void* (*glXGetProcAddressARB)(const unsigned char*) = nullptr;
    void* (*glXGetCurrentContext)() = nullptr;

#endif
};

// =========================================================================
// FUNCTION LOOKUP
// =========================================================================

inline void* GetProc(const char* name, void* user) {
    if (!name || !user)
        return nullptr;

    Context* c = static_cast<Context*>(user);

#if defined(_WIN32) || defined(_WIN64)

    if (!c->valid)
        return nullptr;

    // WGL handles extension functions. OpenGL 1.1 functions are exported
    // directly by opengl32.dll.
    void* p = c->wglGetProcAddress
        ? reinterpret_cast<void*>(c->wglGetProcAddress(name))
        : nullptr;

    // WGL uses these sentinel values to indicate failure.
    if (p == nullptr ||
        p == reinterpret_cast<void*>(1) ||
        p == reinterpret_cast<void*>(2) ||
        p == reinterpret_cast<void*>(3) ||
        p == reinterpret_cast<void*>(-1)) {

        p = c->lib
            ? reinterpret_cast<void*>(GetProcAddress(c->lib, name))
            : nullptr;
    }

    return p;

#elif defined(__APPLE__)

    if (!c->valid || !c->lib)
        return nullptr;

    return dlsym(c->lib, name);

#else

    if (!c->valid)
        return nullptr;

    void* p = c->glXGetProcAddressARB
        ? c->glXGetProcAddressARB(
            reinterpret_cast<const unsigned char*>(name))
        : nullptr;

    if (!p && c->lib)
        p = dlsym(c->lib, name);

    return p;

#endif
}

// =========================================================================
// WINDOWS (WGL)
// =========================================================================

#if defined(_WIN32) || defined(_WIN64)

inline bool Create(Context& c, HWND hwnd, HDC dc, std::string& err) {
    err.clear();

    if (c.valid || c.lib || c.rc) {
        err = "OpenGL context is already initialized";
        return false;
    }

    if (!hwnd || !dc) {
        err = "invalid window handle or device context";
        return false;
    }

    c.lib = LoadLibraryA("opengl32.dll");

    if (!c.lib) {
        err = "opengl32.dll not found";
        return false;
    }

    using CreateFn = HGLRC (WINAPI *)(HDC);

    auto wglCreateContext =
        reinterpret_cast<CreateFn>(
            GetProcAddress(c.lib, "wglCreateContext"));

    c.wglGetProcAddress =
        reinterpret_cast<PROC (WINAPI *)(LPCSTR)>(
            GetProcAddress(c.lib, "wglGetProcAddress"));

    c.wglMakeCurrent =
        reinterpret_cast<BOOL (WINAPI *)(HDC, HGLRC)>(
            GetProcAddress(c.lib, "wglMakeCurrent"));

    c.wglDeleteContext =
        reinterpret_cast<BOOL (WINAPI *)(HGLRC)>(
            GetProcAddress(c.lib, "wglDeleteContext"));

    c.wglGetCurrentContext =
        reinterpret_cast<HGLRC (WINAPI *)()>(
            GetProcAddress(c.lib, "wglGetCurrentContext"));

    if (!wglCreateContext ||
        !c.wglGetProcAddress ||
        !c.wglMakeCurrent ||
        !c.wglDeleteContext ||
        !c.wglGetCurrentContext) {

        err = "opengl32.dll is missing required WGL functions";

        FreeLibrary(c.lib);
        c.lib = nullptr;

        c.wglGetProcAddress = nullptr;
        c.wglMakeCurrent = nullptr;
        c.wglDeleteContext = nullptr;
        c.wglGetCurrentContext = nullptr;

        return false;
    }

    // Set up the pixel format.
    //
    // IMPORTANT: SetPixelFormat may only be called once per window.
    // The software renderer and OpenGL renderer must therefore not both
    // attempt to configure the same window independently.

    PIXELFORMATDESCRIPTOR pfd = {};

    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;

    pfd.dwFlags =
        PFD_DRAW_TO_WINDOW |
        PFD_SUPPORT_OPENGL |
        PFD_DOUBLEBUFFER;

    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int format = ChoosePixelFormat(dc, &pfd);

    if (!format || !SetPixelFormat(dc, format, &pfd)) {
        err = "no usable pixel format";

        FreeLibrary(c.lib);
        c.lib = nullptr;

        c.wglGetProcAddress = nullptr;
        c.wglMakeCurrent = nullptr;
        c.wglDeleteContext = nullptr;
        c.wglGetCurrentContext = nullptr;

        return false;
    }

    // Create a temporary legacy context so we can retrieve
    // wglCreateContextAttribsARB.

    HGLRC legacy = wglCreateContext(dc);

    if (!legacy || !c.wglMakeCurrent(dc, legacy)) {
        err = "wglCreateContext failed";

        if (legacy)
            c.wglDeleteContext(legacy);

        FreeLibrary(c.lib);
        c.lib = nullptr;

        c.wglGetProcAddress = nullptr;
        c.wglMakeCurrent = nullptr;
        c.wglDeleteContext = nullptr;
        c.wglGetCurrentContext = nullptr;

        return false;
    }

    using CreateAttribsFn =
        HGLRC (WINAPI *)(HDC, HGLRC, const int*);

    auto createAttribs =
        reinterpret_cast<CreateAttribsFn>(
            c.wglGetProcAddress("wglCreateContextAttribsARB"));

    HGLRC core = nullptr;

    if (createAttribs) {
        // WGL_CONTEXT_MAJOR_VERSION_ARB = 0x2091
        // WGL_CONTEXT_MINOR_VERSION_ARB = 0x2092
        // WGL_CONTEXT_PROFILE_MASK_ARB  = 0x9126
        // WGL_CONTEXT_CORE_PROFILE_BIT_ARB = 0x00000001

        const int attributes[] = {
            0x2091, 3, 0x2092, 3,
            0x9126, 0x00000001, 0
        };

        core = createAttribs(dc, nullptr, attributes);
    }

    if (core) {
        c.wglMakeCurrent(nullptr, nullptr);
        c.wglDeleteContext(legacy);

        if (!c.wglMakeCurrent(dc, core)) {
            err = "could not activate the 3.3 core context";

            c.wglDeleteContext(core);

            FreeLibrary(c.lib);
            c.lib = nullptr;

            c.wglGetProcAddress = nullptr;
            c.wglMakeCurrent = nullptr;
            c.wglDeleteContext = nullptr;
            c.wglGetCurrentContext = nullptr;

            return false;
        }

        c.rc = core;

    } else {
        // Fallback to the legacy context if the driver does not expose
        // wglCreateContextAttribsARB.
        c.rc = legacy;
    }

    c.hwnd = hwnd;
    c.dc = dc;
    c.valid = true;

    // Request vertical synchronization when supported.
    using SwapIntervalFn = BOOL (WINAPI *)(int);

    auto swapInterval =
        reinterpret_cast<SwapIntervalFn>(
            c.wglGetProcAddress("wglSwapIntervalEXT"));

    if (swapInterval)
        swapInterval(1);

    return true;
}

inline bool MakeCurrent(Context& c) {
    return c.valid &&
           c.wglMakeCurrent &&
           c.wglMakeCurrent(c.dc, c.rc);
}

inline void SwapBuffers_(Context& c) {
    if (c.valid && c.dc)
        ::SwapBuffers(c.dc);
}

inline void Destroy(Context& c) {
    // Only unbind our own context. Do not accidentally unbind another
    // renderer's context on this thread.

    if (c.rc &&
        c.wglGetCurrentContext &&
        c.wglMakeCurrent &&
        c.wglGetCurrentContext() == c.rc) {

        c.wglMakeCurrent(nullptr, nullptr);
    }

    if (c.rc && c.wglDeleteContext)
        c.wglDeleteContext(c.rc);

    // The DC and HWND belong to the window system.
    // The OpenGL library belongs to this Context.

    if (c.lib)
        FreeLibrary(c.lib);

    c.valid = false;

    c.hwnd = nullptr;
    c.dc = nullptr;
    c.rc = nullptr;
    c.lib = nullptr;

    c.wglGetProcAddress = nullptr;
    c.wglMakeCurrent = nullptr;
    c.wglDeleteContext = nullptr;
    c.wglGetCurrentContext = nullptr;
}

// =========================================================================
// MACOS (NSOpenGLContext / CGL)
// =========================================================================

#elif defined(__APPLE__)

// objc_msgSend must be called through a function pointer with the correct
// signature for the message being sent.
//
// This helper provides that typed call without requiring Objective-C++.

template <typename Ret, typename SendFn, typename... Args>
inline Ret CtxMsg(
    SendFn send,
    void* obj,
    void* selector,
    Args... args
) {
    using Fn = Ret (*)(void*, void*, Args...);

    return reinterpret_cast<Fn>(send)(
        obj,
        selector,
        args...
    );
}

inline bool Create(
    Context& c,
    void* nsWindowContentView,
    std::string& err,
    void* (*objc_getClass_)(const char*),
    void* (*sel_registerName_)(const char*),
    void* (*objc_msgSend_)(void*, void*, ...)
) {
    err.clear();

    if (c.valid || c.lib || c.nsContext) {
        err = "OpenGL context is already initialized";
        return false;
    }

    if (!nsWindowContentView) {
        err = "window content view is null";
        return false;
    }

    if (!objc_getClass_ ||
        !sel_registerName_ ||
        !objc_msgSend_) {

        err = "Objective-C runtime functions are unavailable";
        return false;
    }

    // Load the system OpenGL framework.

    void* lib = dlopen(
        "/System/Library/Frameworks/OpenGL.framework/OpenGL",
        RTLD_LAZY | RTLD_LOCAL
    );

    if (!lib) {
        err = "OpenGL.framework could not be loaded";
        return false;
    }

    // Resolve CGL functions.

    auto setCurrent =
        reinterpret_cast<int (*)(void*)>(
            dlsym(lib, "CGLSetCurrentContext"));

    auto getCurrent =
        reinterpret_cast<void* (*)()>(
            dlsym(lib, "CGLGetCurrentContext"));

    auto flushDrawable =
        reinterpret_cast<int (*)(void*)>(
            dlsym(lib, "CGLFlushDrawable"));

    auto setParameter =
        reinterpret_cast<int (*)(void*, int, const int*)>(
            dlsym(lib, "CGLSetParameter"));

    if (!setCurrent || !getCurrent || !flushDrawable) {
        err = "OpenGL.framework is missing required CGL functions";

        dlclose(lib);
        return false;
    }

    // Resolve Objective-C classes.

    void* pixelFormatClass =
        objc_getClass_("NSOpenGLPixelFormat");

    void* contextClass =
        objc_getClass_("NSOpenGLContext");

    if (!pixelFormatClass || !contextClass) {
        err = "NSOpenGLPixelFormat or NSOpenGLContext is unavailable";

        dlclose(lib);
        return false;
    }

    // Resolve Objective-C selectors.

    void* allocSelector =
        sel_registerName_("alloc");

    void* initPixelFormatSelector =
        sel_registerName_("initWithAttributes:");

    void* initContextSelector =
        sel_registerName_("initWithFormat:shareContext:");

    void* setViewSelector =
        sel_registerName_("setView:");

    void* makeCurrentSelector =
        sel_registerName_("makeCurrentContext");

    void* cglContextSelector =
        sel_registerName_("CGLContextObj");

    void* releaseSelector =
        sel_registerName_("release");

    if (!allocSelector ||
        !initPixelFormatSelector ||
        !initContextSelector ||
        !setViewSelector ||
        !makeCurrentSelector ||
        !cglContextSelector ||
        !releaseSelector) {

        err = "failed to resolve required Objective-C selectors";

        dlclose(lib);
        return false;
    }

    // NSOpenGLPixelFormatAttribute uses NSUInteger-sized values.
    //
    // Request a 3.2 Core profile, double buffering, depth, color,
    // and hardware acceleration.
    //
    // macOS exposes the NSOpenGL 3.2 Core profile token. The driver may
    // provide a newer supported core implementation.

    const uintptr_t attributes[] = {
        99,       // NSOpenGLPFAOpenGLProfile
        0x3200,   // NSOpenGLProfileVersion3_2Core
        5,        // NSOpenGLPFADoubleBuffer
        12, 24,   // NSOpenGLPFADepthSize
        8, 32,    // NSOpenGLPFAColorSize
        73,       // NSOpenGLPFAAccelerated
        0
    };

    void* pixelFormat = nullptr;
    void* nsContext = nullptr;
    void* cglContext = nullptr;

    bool madeCurrent = false;

    // objc_msgSend is used as the implementation of NSObject's release.
    using ReleaseFn = void (*)(void*, void*);

    auto releaseObject =
        reinterpret_cast<ReleaseFn>(objc_msgSend_);

    // All partially created objects are owned locally until the final
    // successful commit to Context.
    //
    // Every failure after this point passes through this cleanup routine.

    auto cleanup = [&]() {
        // Do not leave our partially initialized context current.

        if (madeCurrent &&
            cglContext &&
            getCurrent() == cglContext) {

            setCurrent(nullptr);
        }

        if (nsContext) {
            CtxMsg<void>(
                objc_msgSend_,
                nsContext,
                releaseSelector
            );

            nsContext = nullptr;
        }

        if (pixelFormat) {
            CtxMsg<void>(
                objc_msgSend_,
                pixelFormat,
                releaseSelector
            );

            pixelFormat = nullptr;
        }

        if (lib) {
            dlclose(lib);
            lib = nullptr;
        }
    };

    // ---------------------------------------------------------------------
    // Create the pixel format.
    // ---------------------------------------------------------------------

    void* allocatedPixelFormat =
        CtxMsg<void*>(
            objc_msgSend_,
            pixelFormatClass,
            allocSelector
        );

    if (!allocatedPixelFormat) {
        err = "NSOpenGLPixelFormat allocation failed";

        cleanup();
        return false;
    }

    pixelFormat =
        CtxMsg<void*>(
            objc_msgSend_,
            allocatedPixelFormat,
            initPixelFormatSelector,
            attributes
        );

    if (!pixelFormat) {
        // An Objective-C init method consumes its allocated receiver
        // when initialization fails.

        err = "no accelerated 3.2 Core OpenGL pixel format is available";

        cleanup();
        return false;
    }

    // ---------------------------------------------------------------------
    // Create the NSOpenGLContext.
    // ---------------------------------------------------------------------

    void* allocatedContext =
        CtxMsg<void*>(
            objc_msgSend_,
            contextClass,
            allocSelector
        );

    if (!allocatedContext) {
        err = "NSOpenGLContext allocation failed";

        cleanup();
        return false;
    }

    nsContext =
        CtxMsg<void*>(
            objc_msgSend_,
            allocatedContext,
            initContextSelector,
            pixelFormat,
            static_cast<void*>(nullptr)
        );

    // The context now owns the information it needs from the pixel format.
    // Release our pixel format ownership.

    CtxMsg<void>(
        objc_msgSend_,
        pixelFormat,
        releaseSelector
    );

    pixelFormat = nullptr;

    if (!nsContext) {
        err = "NSOpenGLContext creation failed";

        cleanup();
        return false;
    }

    // ---------------------------------------------------------------------
    // Attach the context to the existing window.
    // ---------------------------------------------------------------------

    CtxMsg<void>(
        objc_msgSend_,
        nsContext,
        setViewSelector,
        nsWindowContentView
    );

    // Retrieve the underlying CGL context before activating it.
    // This lets cleanup identify the context even if activation succeeds
    // but a later verification step fails.

    cglContext =
        CtxMsg<void*>(
            objc_msgSend_,
            nsContext,
            cglContextSelector
        );

    if (!cglContext) {
        err = "NSOpenGLContext did not provide a valid CGL context";

        cleanup();
        return false;
    }

    // ---------------------------------------------------------------------
    // Activate and verify the context.
    // ---------------------------------------------------------------------

    CtxMsg<void>(objc_msgSend_, nsContext, makeCurrentSelector); madeCurrent = true;
    if (getCurrent() != cglContext) { err = "NSOpenGLContext failed to activate its CGL context"; cleanup(); return false; }

    // ---------------------------------------------------------------------
    // Configure swap interval.
    // ---------------------------------------------------------------------

    // kCGLCPSwapInterval = 222.
    // Failure to configure vsync is non-fatal.

    if (setParameter) {
        int swapInterval = 1;

        setParameter(
            cglContext,
            222,
            &swapInterval
        );
    }

    // ---------------------------------------------------------------------
    // Commit resources.
    // ---------------------------------------------------------------------
    //
    // Nothing is written into Context until all required initialization
    // steps have succeeded.

    c.lib = lib;
    c.cglContext = cglContext;
    c.nsContext = nsContext;
    c.nsView = nsWindowContentView;

    c.CGLSetCurrentContext = setCurrent;
    c.CGLGetCurrentContext = getCurrent;
    c.CGLFlushDrawable = flushDrawable;
    c.CGLSetParameter = setParameter;

    c.objcRelease = releaseObject;
    c.releaseSelector = releaseSelector;

    c.valid = true;

    // Ownership has moved to Context.
    lib = nullptr;
    nsContext = nullptr;
    cglContext = nullptr;

    return true;
}

inline bool MakeCurrent(Context& c) {
    return c.valid &&
           c.CGLSetCurrentContext &&
           c.CGLSetCurrentContext(c.cglContext) == 0;
}

inline void SwapBuffers_(Context& c) {
    if (c.valid && c.CGLFlushDrawable)
        c.CGLFlushDrawable(c.cglContext);
}

inline void Destroy(Context& c) {
    // Unbind only our own context.

    if (c.cglContext &&
        c.CGLGetCurrentContext &&
        c.CGLSetCurrentContext) {

        if (c.CGLGetCurrentContext() == c.cglContext)
            c.CGLSetCurrentContext(nullptr);
    }

    // Release our retained NSOpenGLContext.

    if (c.nsContext &&
        c.objcRelease &&
        c.releaseSelector) {

        c.objcRelease(
            c.nsContext,
            c.releaseSelector
        );
    }

    // The NSView belongs to the window system, not this context.

    if (c.lib)
        dlclose(c.lib);

    // Reset everything. Destroy() is safe to call repeatedly.

    c.valid = false;

    c.lib = nullptr;
    c.cglContext = nullptr;
    c.nsContext = nullptr;
    c.nsView = nullptr;

    c.CGLSetCurrentContext = nullptr;
    c.CGLGetCurrentContext = nullptr;
    c.CGLFlushDrawable = nullptr;
    c.CGLSetParameter = nullptr;

    c.objcRelease = nullptr;
    c.releaseSelector = nullptr;
}

// =========================================================================
// LINUX / X11 (GLX)
// =========================================================================

#else

inline bool Create(
    Context& c,
    void* display,
    unsigned long window,
    void* glxContext,
    std::string& err
) {
    err.clear();

    if (c.valid || c.lib || c.glxContext) {
        err = "OpenGL context is already initialized";
        return false;
    }

    if (!display || !window || !glxContext) {
        err = "invalid GLX display, window, or context";
        return false;
    }

    c.lib = dlopen(
        "libGL.so.1",
        RTLD_LAZY | RTLD_LOCAL
    );

    if (!c.lib) {
        err = "libGL.so.1 not found";
        return false;
    }

    c.glXMakeCurrent =
        reinterpret_cast<int (*)(void*, unsigned long, void*)>(
            dlsym(c.lib, "glXMakeCurrent"));

    c.glXSwapBuffers =
        reinterpret_cast<void (*)(void*, unsigned long)>(
            dlsym(c.lib, "glXSwapBuffers"));

    c.glXGetProcAddressARB =
        reinterpret_cast<void* (*)(const unsigned char*)>(
            dlsym(c.lib, "glXGetProcAddressARB"));

    c.glXGetCurrentContext =
        reinterpret_cast<void* (*)()>(
            dlsym(c.lib, "glXGetCurrentContext"));

    if (!c.glXMakeCurrent ||
        !c.glXSwapBuffers ||
        !c.glXGetProcAddressARB ||
        !c.glXGetCurrentContext) {

        err = "libGL is missing required GLX functions";

        dlclose(c.lib);
        c.lib = nullptr;

        c.glXMakeCurrent = nullptr;
        c.glXSwapBuffers = nullptr;
        c.glXGetProcAddressARB = nullptr;
        c.glXGetCurrentContext = nullptr;

        return false;
    }

    // Borrow the display, window, and GLX context.
    // Their lifetime remains owned by the window system.

    c.display = display;
    c.window = window;
    c.glxContext = glxContext;

    c.valid = true;

    return true;
}

inline bool MakeCurrent(Context& c) {
    return c.valid &&
           c.glXMakeCurrent &&
           c.glXMakeCurrent(
               c.display,
               c.window,
               c.glxContext
           ) != 0;
}

inline void SwapBuffers_(Context& c) {
    if (c.valid && c.glXSwapBuffers)
        c.glXSwapBuffers(c.display, c.window);
}

inline void Destroy(Context& c) {
    // Only unbind our own context. Another context may have become current
    // on this thread since this one was created.

    if (c.glxContext &&
        c.glXGetCurrentContext &&
        c.glXMakeCurrent) {

        if (c.glXGetCurrentContext() == c.glxContext) {
            c.glXMakeCurrent(
                c.display,
                0,
                nullptr
            );
        }
    }

    // The GLX context, window, and display are owned by window_destroy.
    // This only releases the additional library reference acquired here.

    if (c.lib)
        dlclose(c.lib);

    c.valid = false;

    c.display = nullptr;
    c.window = 0;
    c.glxContext = nullptr;
    c.lib = nullptr;

    c.glXMakeCurrent = nullptr;
    c.glXSwapBuffers = nullptr;
    c.glXGetProcAddressARB = nullptr;
    c.glXGetCurrentContext = nullptr;
}

#endif

} // namespace GlContext

#endif // GL_CONTEXT_HPP
