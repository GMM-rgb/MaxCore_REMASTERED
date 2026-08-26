#include <iostream>
#include <cstring>
#include <lua.hpp>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <string>
#include <cmath>

#include "inline_headers/graphics/renderer.hpp"

#if defined(_WIN32) || defined(_WIN64)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #define EXPORT_FN __declspec(dllexport)
    #include <windows.h>
    
    struct PlatformWindow {
        HWND hwnd = nullptr;
        HDC hdc = nullptr;
        BITMAPINFO bmi{};
        DWORD styleFlags = 0;
        RECT savedRect{};
    };
#elif defined(__APPLE__)
    #define EXPORT_FN
    #include <objc/runtime.h>
    #include <objc/message.h>
    #include <CoreGraphics/CoreGraphics.h>

    template<typename R, typename Target, typename... Args>
    R msgSend(Target self, SEL op, Args... args) {
        using Func = R (*)(id, SEL, Args...);
        return reinterpret_cast<Func>(objc_msgSend)(reinterpret_cast<id>(self), op, args...);
    }

    struct PlatformWindow {
        id window = nullptr;
        id view = nullptr;
    };
#else
    #define EXPORT_FN
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
    
    struct PlatformWindow {
        Display* display = nullptr;
        Window window = 0;
        GC gc = 0;
    };
#endif

struct NativeWindow {
    int width = 800;
    int height = 600;
    bool shouldClose = false;
    bool isFullscreen = false;
    std::vector<uint32_t> canvasBuffer;
    PlatformWindow platform;
};

static std::unordered_map<int, NativeWindow*> g_windows;
static std::unordered_map<int, Graphics::ImageBuffer> g_images;
static int g_next_window_id = 1;
static int g_next_image_id = 1;

#if defined(_WIN32) || defined(_WIN64)
static LRESULT CALLBACK Win32Proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    NativeWindow* win = reinterpret_cast<NativeWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_CLOSE:
            if (win) win->shouldClose = true;
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}
#endif

// =========================================================================
// HELPER RASTERIZER FOR FILLED TRIANGLES
// =========================================================================
static void FillTriangle(uint32_t* buffer, int width, int height, Graphics::Vec2 p0, Graphics::Vec2 p1, Graphics::Vec2 p2, uint32_t color) {
    int minX = (std::max)(0, (int)std::floor((std::min)({p0.x, p1.x, p2.x})));
    int maxX = (std::min)(width - 1, (int)std::ceil((std::max)({p0.x, p1.x, p2.x})));
    int minY = (std::max)(0, (int)std::floor((std::min)({p0.y, p1.y, p2.y})));
    int maxY = (std::min)(height - 1, (int)std::ceil((std::max)({p0.y, p1.y, p2.y})));

    auto edge = [](Graphics::Vec2 a, Graphics::Vec2 b, float px, float py) {
        return (px - a.x) * (b.y - a.y) - (py - a.y) * (b.x - a.x);
    };

    float area = edge(p0, p1, p2.x, p2.y);
    if (std::abs(area) < 0.0001f) return;

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            float px = x + 0.5f;
            float py = y + 0.5f;

            float w0 = edge(p1, p2, px, py);
            float w1 = edge(p2, p0, px, py);
            float w2 = edge(p0, p1, px, py);

            if ((area > 0 && w0 >= 0 && w1 >= 0 && w2 >= 0) ||
                (area < 0 && w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                buffer[y * width + x] = color;
            }
        }
    }
}

// =========================================================================
// WINDOW CORE FUNCTIONS
// =========================================================================
static int window_create(lua_State* L) {
    const char* title = luaL_optstring(L, 1, "Native Window");
    int width = static_cast<int>(luaL_optinteger(L, 2, 800));
    int height = static_cast<int>(luaL_optinteger(L, 3, 600));

    NativeWindow* win = new NativeWindow();
    win->width = width;
    win->height = height;
    win->canvasBuffer.resize(width * height, 0xFF181818);

#if defined(_WIN32) || defined(_WIN64)
    WNDCLASSEXA wc = { sizeof(WNDCLASSEXA) };
    wc.lpfnWndProc = Win32Proc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "LuaNativeWindowClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExA(&wc);

    win->platform.styleFlags = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    win->platform.hwnd = CreateWindowExA(
        0, "LuaNativeWindowClass", title,
        win->platform.styleFlags,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    SetWindowLongPtr(win->platform.hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(win));
    win->platform.hdc = GetDC(win->platform.hwnd);

    win->platform.bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    win->platform.bmi.bmiHeader.biWidth = width;
    win->platform.bmi.bmiHeader.biHeight = -height;
    win->platform.bmi.bmiHeader.biPlanes = 1;
    win->platform.bmi.bmiHeader.biBitCount = 32;
    win->platform.bmi.bmiHeader.biCompression = BI_RGB;

#elif defined(__APPLE__)
    id appClass = reinterpret_cast<id>(objc_getClass("NSApplication"));
    id app = msgSend<id>(appClass, sel_registerName("sharedApplication"));
    msgSend<void>(app, sel_registerName("setActivationPolicy:"), 0);

    CGRect cgRect = CGRectMake(0, 0, width, height);
    id windowClass = reinterpret_cast<id>(objc_getClass("NSWindow"));
    win->platform.window = msgSend<id>(msgSend<id>(windowClass, sel_registerName("alloc")),
        sel_registerName("initWithContentRect:styleMask:backing:defer:"),
        cgRect, 15, 2, false);

    id nsTitle = msgSend<id>(reinterpret_cast<id>(objc_getClass("NSString")), sel_registerName("stringWithUTF8String:"), title);
    msgSend<void>(win->platform.window, sel_registerName("setTitle:"), nsTitle);
    msgSend<void>(win->platform.window, sel_registerName("makeKeyAndOrderFront:"), static_cast<id>(nullptr));

#else
    win->platform.display = XOpenDisplay(NULL);
    if (!win->platform.display) {
        delete win;
        lua_pushnil(L);
        lua_pushstring(L, "Failed to connect to X Server");
        return 2;
    }
    int screen = DefaultScreen(win->platform.display);
    win->platform.window = XCreateSimpleWindow(
        win->platform.display, RootWindow(win->platform.display, screen),
        10, 10, width, height, 1,
        BlackPixel(win->platform.display, screen),
        WhitePixel(win->platform.display, screen)
    );
    XSelectInput(win->platform.display, win->platform.window, ExposureMask | KeyPressMask | StructureNotifyMask);
    XMapWindow(win->platform.display, win->platform.window);
    win->platform.gc = XCreateGC(win->platform.display, win->platform.window, 0, NULL);
    XStoreName(win->platform.display, win->platform.window, title);
#endif

    int winId = g_next_window_id++;
    g_windows[winId] = win;
    lua_pushinteger(L, winId);
    return 1;
}

static int window_get_dimensions(lua_State* L) {
    int winId = static_cast<int>(luaL_checkinteger(L, 1));
    auto it = g_windows.find(winId);
    if (it != g_windows.end()) {
        lua_pushinteger(L, it->second->width);
        lua_pushinteger(L, it->second->height);
        return 2;
    }
    lua_pushnil(L);
    lua_pushnil(L);
    return 2;
}

static int window_set_dimensions(lua_State* L) {
    int winId = static_cast<int>(luaL_checkinteger(L, 1));
    int w = static_cast<int>(luaL_checkinteger(L, 2));
    int h = static_cast<int>(luaL_checkinteger(L, 3));

    auto it = g_windows.find(winId);
    if (it != g_windows.end()) {
        NativeWindow* win = it->second;
        win->width = w;
        win->height = h;
        win->canvasBuffer.resize(w * h, 0xFF181818);

#if defined(_WIN32) || defined(_WIN64)
        SetWindowPos(win->platform.hwnd, NULL, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
        win->platform.bmi.bmiHeader.biWidth = w;
        win->platform.bmi.bmiHeader.biHeight = -h;
#endif
    }
    return 0;
}

static int window_set_fullscreen(lua_State* L) {
    int winId = static_cast<int>(luaL_checkinteger(L, 1));
    bool enable = lua_toboolean(L, 2);

    auto it = g_windows.find(winId);
    if (it != g_windows.end()) {
        NativeWindow* win = it->second;
        if (win->isFullscreen == enable) return 0;

        win->isFullscreen = enable;
#if defined(_WIN32) || defined(_WIN64)
        if (enable) {
            GetWindowRect(win->platform.hwnd, &win->platform.savedRect);
            SetWindowLong(win->platform.hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
            int screenW = GetSystemMetrics(SM_CXSCREEN);
            int screenH = GetSystemMetrics(SM_CYSCREEN);
            SetWindowPos(win->platform.hwnd, HWND_TOP, 0, 0, screenW, screenH, SWP_FRAMECHANGED);
            
            win->width = screenW;
            win->height = screenH;
            win->canvasBuffer.resize(screenW * screenH, 0xFF181818);
            win->platform.bmi.bmiHeader.biWidth = screenW;
            win->platform.bmi.bmiHeader.biHeight = -screenH;
        } else {
            SetWindowLong(win->platform.hwnd, GWL_STYLE, win->platform.styleFlags);
            int w = win->platform.savedRect.right - win->platform.savedRect.left;
            int h = win->platform.savedRect.bottom - win->platform.savedRect.top;
            SetWindowPos(win->platform.hwnd, NULL, win->platform.savedRect.left, win->platform.savedRect.top, w, h, SWP_NOZORDER | SWP_FRAMECHANGED);
            
            win->width = w;
            win->height = h;
            win->canvasBuffer.resize(w * h, 0xFF181818);
            win->platform.bmi.bmiHeader.biWidth = w;
            win->platform.bmi.bmiHeader.biHeight = -h;
        }
#endif
    }
    return 0;
}

static int window_poll_events(lua_State* L) {
#if defined(_WIN32) || defined(_WIN64)
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
#elif defined(__APPLE__)
    id app = msgSend<id>(reinterpret_cast<id>(objc_getClass("NSApplication")), sel_registerName("sharedApplication"));
    id event = msgSend<id>(app, sel_registerName("nextEventMatchingMask:untilDate:inMode:dequeue:"),
        ULONG_MAX, static_cast<id>(nullptr), 
        msgSend<id>(reinterpret_cast<id>(objc_getClass("NSString")), sel_registerName("stringWithUTF8String:"), "kCFRunLoopDefaultMode"), true);
    if (event) {
        msgSend<void>(app, sel_registerName("sendEvent:"), event);
    }
#else
    for (auto& pair : g_windows) {
        NativeWindow* win = pair.second;
        while (XPending(win->platform.display)) {
            XEvent ev;
            XNextEvent(win->platform.display, &ev);
            if (ev.type == DestroyNotify) win->shouldClose = true;
        }
    }
#endif
    return 0;
}

static int window_should_close(lua_State* L) {
    int winId = static_cast<int>(luaL_checkinteger(L, 1));
    auto it = g_windows.find(winId);
    lua_pushboolean(L, (it == g_windows.end() || it->second->shouldClose) ? 1 : 0);
    return 1;
}

static int window_clear_canvas(lua_State* L) {
    int winId = static_cast<int>(luaL_checkinteger(L, 1));
    uint8_t r = static_cast<uint8_t>(luaL_optinteger(L, 2, 0));
    uint8_t g = static_cast<uint8_t>(luaL_optinteger(L, 3, 0));
    uint8_t b = static_cast<uint8_t>(luaL_optinteger(L, 4, 0));

    auto it = g_windows.find(winId);
    if (it != g_windows.end()) {
        uint32_t color = (0xFF << 24) | (r << 16) | (g << 8) | b;
        std::fill(it->second->canvasBuffer.begin(), it->second->canvasBuffer.end(), color);
    }
    return 0;
}

static int window_draw_rect(lua_State* L) {
    int winId = static_cast<int>(luaL_checkinteger(L, 1));
    int rx = static_cast<int>(luaL_checkinteger(L, 2));
    int ry = static_cast<int>(luaL_checkinteger(L, 3));
    int rw = static_cast<int>(luaL_checkinteger(L, 4));
    int rh = static_cast<int>(luaL_checkinteger(L, 5));
    uint32_t col = (0xFF << 24) | (static_cast<uint8_t>(luaL_optinteger(L, 6, 255)) << 16) 
                                | (static_cast<uint8_t>(luaL_optinteger(L, 7, 255)) << 8) 
                                | static_cast<uint8_t>(luaL_optinteger(L, 8, 255));

    auto it = g_windows.find(winId);
    if (it == g_windows.end()) return 0;
    NativeWindow* win = it->second;

    int startX = (std::max)(0, rx), endX = (std::min)(win->width, rx + rw);
    int startY = (std::max)(0, ry), endY = (std::min)(win->height, ry + rh);

    for (int y = startY; y < endY; ++y)
        for (int x = startX; x < endX; ++x)
            win->canvasBuffer[y * win->width + x] = col;
    return 0;
}

static int window_draw_line(lua_State* L) {
    int winId = static_cast<int>(luaL_checkinteger(L, 1));
    int x0 = static_cast<int>(luaL_checkinteger(L, 2));
    int y0 = static_cast<int>(luaL_checkinteger(L, 3));
    int x1 = static_cast<int>(luaL_checkinteger(L, 4));
    int y1 = static_cast<int>(luaL_checkinteger(L, 5));
    uint32_t col = (0xFF << 24) | (static_cast<uint8_t>(luaL_optinteger(L, 6, 255)) << 16) 
                                | (static_cast<uint8_t>(luaL_optinteger(L, 7, 255)) << 8) 
                                | static_cast<uint8_t>(luaL_optinteger(L, 8, 255));

    auto it = g_windows.find(winId);
    if (it != g_windows.end()) {
        Graphics::DrawLine(it->second->canvasBuffer.data(), it->second->width, it->second->height, x0, y0, x1, y1, col);
    }
    return 0;
}

static int window_draw_circle(lua_State* L) {
    int winId = static_cast<int>(luaL_checkinteger(L, 1));
    int cx = static_cast<int>(luaL_checkinteger(L, 2));
    int cy = static_cast<int>(luaL_checkinteger(L, 3));
    int r  = static_cast<int>(luaL_checkinteger(L, 4));
    bool fill = lua_toboolean(L, 5);
    uint32_t col = (0xFF << 24) | (static_cast<uint8_t>(luaL_optinteger(L, 6, 255)) << 16) 
                                | (static_cast<uint8_t>(luaL_optinteger(L, 7, 255)) << 8) 
                                | static_cast<uint8_t>(luaL_optinteger(L, 8, 255));

    auto it = g_windows.find(winId);
    if (it != g_windows.end()) {
        Graphics::DrawCircle(it->second->canvasBuffer.data(), it->second->width, it->second->height, cx, cy, r, col, fill);
    }
    return 0;
}

static int image_create(lua_State* L) {
    int width = static_cast<int>(luaL_checkinteger(L, 1));
    int height = static_cast<int>(luaL_checkinteger(L, 2));

    Graphics::ImageBuffer img;
    img.width = width;
    img.height = height;
    img.pixels.resize(width * height, 0x00000000);

    if (lua_istable(L, 3)) {
        size_t len = lua_rawlen(L, 3);
        size_t count = (std::min)((size_t)(width * height), len);
        for (size_t i = 1; i <= count; ++i) {
            lua_rawgeti(L, 3, i);
            img.pixels[i - 1] = static_cast<uint32_t>(lua_tointeger(L, -1));
            lua_pop(L, 1);
        }
    }

    int imgId = g_next_image_id++;
    g_images[imgId] = img;
    lua_pushinteger(L, imgId);
    return 1;
}

static int window_draw_image(lua_State* L) {
    int winId = static_cast<int>(luaL_checkinteger(L, 1));
    int imgId = static_cast<int>(luaL_checkinteger(L, 2));
    int dx = static_cast<int>(luaL_checkinteger(L, 3));
    int dy = static_cast<int>(luaL_checkinteger(L, 4));

    auto itWin = g_windows.find(winId);
    auto itImg = g_images.find(imgId);
    if (itWin != g_windows.end() && itImg != g_images.end()) {
        Graphics::DrawImage(itWin->second->canvasBuffer.data(), itWin->second->width, itWin->second->height, itImg->second, dx, dy);
    }
    return 0;
}

// =========================================================================
// 3D OBJECT MESH RENDERER (SOLID & WIREFRAME WITH CUSTOM COLORS)
// =========================================================================
static int window_draw_cube(lua_State* L) {
    int winId = static_cast<int>(luaL_checkinteger(L, 1));
    float posX = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    float posY = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    float posZ = static_cast<float>(luaL_optnumber(L, 4, 3.0));

    float rotX = static_cast<float>(luaL_optnumber(L, 5, 0.0));
    float rotY = static_cast<float>(luaL_optnumber(L, 6, 0.0));
    float rotZ = static_cast<float>(luaL_optnumber(L, 7, 0.0));

    float scaleX = static_cast<float>(luaL_optnumber(L, 8, 1.0));
    float scaleY = static_cast<float>(luaL_optnumber(L, 9, 1.0));
    float scaleZ = static_cast<float>(luaL_optnumber(L, 10, 1.0));

    uint8_t r = static_cast<uint8_t>(luaL_optinteger(L, 11, 0));
    uint8_t g = static_cast<uint8_t>(luaL_optinteger(L, 12, 255));
    uint8_t b = static_cast<uint8_t>(luaL_optinteger(L, 13, 0));
    bool wireframe = lua_toboolean(L, 14);

    uint32_t col = (0xFF << 24) | (r << 16) | (g << 8) | b;

    auto it = g_windows.find(winId);
    if (it == g_windows.end()) return 0;
    NativeWindow* win = it->second;

    Graphics::Vec3 rawVertices[8] = {
        {-0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f}, { 0.5f,  0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f},
        {-0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f}, { 0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f}
    };

    int faces[6][4] = {
        {4, 5, 6, 7}, // Front
        {1, 0, 3, 2}, // Back
        {7, 6, 2, 3}, // Top
        {0, 1, 5, 4}, // Bottom
        {5, 1, 2, 6}, // Right
        {0, 4, 7, 3}  // Left
    };

    Graphics::Matrix4x4 matRotX = Graphics::Matrix4x4::RotationX(rotX);
    Graphics::Matrix4x4 matRotY = Graphics::Matrix4x4::RotationY(rotY);
    Graphics::Matrix4x4 matTrans = Graphics::Matrix4x4::Translation(posX, posY, posZ);
    Graphics::Matrix4x4 matProj = Graphics::Matrix4x4::Perspective(90.0f, (float)win->width / (float)win->height, 0.1f, 100.0f);

    Graphics::Vec3 worldPos[8];
    Graphics::Vec2 projected[8];

    for (int i = 0; i < 8; ++i) {
        Graphics::Vec3 scaled = { rawVertices[i].x * scaleX, rawVertices[i].y * scaleY, rawVertices[i].z * scaleZ };
        Graphics::Vec4 rY = matRotY.MultiplyVector(scaled);
        Graphics::Vec3 rY3 = {rY.x, rY.y, rY.z};
        Graphics::Vec4 rX = matRotX.MultiplyVector(rY3);
        Graphics::Vec3 rX3 = {rX.x, rX.y, rX.z};
        Graphics::Vec4 world = matTrans.MultiplyVector(rX3);

        worldPos[i] = {world.x, world.y, world.z};
        projected[i] = Graphics::Project3DPoint(worldPos[i], matProj, win->width, win->height);
    }

    if (wireframe) {
        int edges[12][2] = {
            {0,1},{1,2},{2,3},{3,0},
            {4,5},{5,6},{6,7},{7,4},
            {0,4},{1,5},{2,6},{3,7}
        };
        for (int i = 0; i < 12; ++i) {
            Graphics::Vec2 p1 = projected[edges[i][0]];
            Graphics::Vec2 p2 = projected[edges[i][1]];
            Graphics::DrawLine(win->canvasBuffer.data(), win->width, win->height, (int)p1.x, (int)p1.y, (int)p2.x, (int)p2.y, col);
        }
    } else {
        // Painter's Algorithm Depth Sort
        struct QuadFace {
            int indices[4];
            float avgZ;
        };

        QuadFace sortedFaces[6];
        for (int i = 0; i < 6; ++i) {
            sortedFaces[i].indices[0] = faces[i][0];
            sortedFaces[i].indices[1] = faces[i][1];
            sortedFaces[i].indices[2] = faces[i][2];
            sortedFaces[i].indices[3] = faces[i][3];

            float sumZ = 0;
            for (int k = 0; k < 4; ++k) {
                sumZ += worldPos[faces[i][k]].z;
            }
            sortedFaces[i].avgZ = sumZ / 4.0f;
        }

        std::sort(std::begin(sortedFaces), std::end(sortedFaces), [](const QuadFace& a, const QuadFace& b) {
            return a.avgZ > b.avgZ;
        });

        for (int i = 0; i < 6; ++i) {
            Graphics::Vec2 p0 = projected[sortedFaces[i].indices[0]];
            Graphics::Vec2 p1 = projected[sortedFaces[i].indices[1]];
            Graphics::Vec2 p2 = projected[sortedFaces[i].indices[2]];
            Graphics::Vec2 p3 = projected[sortedFaces[i].indices[3]];

            // Draw two triangles per quad face
            FillTriangle(win->canvasBuffer.data(), win->width, win->height, p0, p1, p2, col);
            FillTriangle(win->canvasBuffer.data(), win->width, win->height, p0, p2, p3, col);
        }
    }
    return 0;
}

static int window_swap_buffers(lua_State* L) {
    int winId = static_cast<int>(luaL_checkinteger(L, 1));
    auto it = g_windows.find(winId);
    if (it == g_windows.end()) return 0;
    NativeWindow* win = it->second;

#if defined(_WIN32) || defined(_WIN64)
    SetDIBitsToDevice(
        win->platform.hdc, 0, 0, win->width, win->height,
        0, 0, 0, win->height,
        win->canvasBuffer.data(), &win->platform.bmi, DIB_RGB_COLORS
    );
#elif defined(__APPLE__)
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
        win->canvasBuffer.data(), win->width, win->height, 8, win->width * 4,
        colorSpace, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
    );
    CGImageRef image = CGBitmapContextCreateImage(ctx);
    id windowView = msgSend<id>(win->platform.window, sel_registerName("contentView"));
    id layer = msgSend<id>(windowView, sel_registerName("layer"));
    msgSend<void>(layer, sel_registerName("setContents:"), reinterpret_cast<id>(image));
    CGImageRelease(image);
    CGContextRelease(ctx);
    CGColorSpaceRelease(colorSpace);
#else
    XImage* img = XCreateImage(
        win->platform.display, DefaultVisual(win->platform.display, DefaultScreen(win->platform.display)),
        24, ZPixmap, 0, reinterpret_cast<char*>(win->canvasBuffer.data()),
        win->width, win->height, 32, 0
    );
    XPutImage(win->platform.display, win->platform.window, win->platform.gc, img, 0, 0, 0, 0, win->width, win->height);
    img->data = NULL;
    XDestroyImage(img);
#endif

    return 0;
}

static int window_destroy(lua_State* L) {
    int winId = static_cast<int>(luaL_checkinteger(L, 1));
    auto it = g_windows.find(winId);
    if (it != g_windows.end()) {
        NativeWindow* win = it->second;
#if defined(_WIN32) || defined(_WIN64)
        ReleaseDC(win->platform.hwnd, win->platform.hdc);
        DestroyWindow(win->platform.hwnd);
#elif defined(__APPLE__)
        msgSend<void>(win->platform.window, sel_registerName("close"));
#else
        XDestroyWindow(win->platform.display, win->platform.window);
        XCloseDisplay(win->platform.display);
#endif
        delete win;
        g_windows.erase(it);
    }
    return 0;
}

extern "C" EXPORT_FN int luaopen_window_management(lua_State* L) {
    static const luaL_Reg WindowManagement[] = {
        {"create_window", window_create},
        {"get_dimensions", window_get_dimensions},
        {"set_dimensions", window_set_dimensions},
        {"set_fullscreen", window_set_fullscreen},
        {"poll_events", window_poll_events},
        {"should_close", window_should_close},
        {"clear_canvas", window_clear_canvas},
        {"draw_rect", window_draw_rect},
        {"draw_line", window_draw_line},
        {"draw_circle", window_draw_circle},
        {"create_image", image_create},
        {"draw_image", window_draw_image},
        {"draw_cube", window_draw_cube},
        {"swap_buffers", window_swap_buffers},
        {"destroy", window_destroy},
        {nullptr, nullptr}
    };
    
    luaL_newlib(L, WindowManagement);
    return 1;
}
