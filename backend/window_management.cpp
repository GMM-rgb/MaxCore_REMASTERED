#include <iostream>
#include <cstring>
#include <cctype>
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
    #if defined(__UNIX__)
        #warning "UNIX Operating Systems are not supported with this Engine."
    #endif

    #define EXPORT_FN
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
    
    struct PlatformWindow {
        Display* display = nullptr;
        Window window = 0;
        GC gc = 0;
    };
#endif

// Minimal embedded 8x8 font bitmap for ASCII
static const uint8_t g_font8x8[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // '!'
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, // '"'
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, // '#'
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, // '$'
    {0x00,0x63,0x66,0x0C,0x18,0x33,0x63,0x00}, // '%'
    {0x1C,0x36,0x1C,0x3B,0x6E,0x66,0x3B,0x00}, // '&'
    {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00}, // '\''
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, // '('
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, // ')'
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // '*'
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, // '+'
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, // ','
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, // '-'
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, // '.'
    {0x00,0x03,0x06,0x0C,0x18,0x30,0x60,0x00}, // '/'
    {0x3E,0x63,0x6B,0x6F,0x7B,0x63,0x3E,0x00}, // '0'
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, // '1'
    {0x3C,0x66,0x06,0x1C,0x30,0x66,0x7E,0x00}, // '2'
    {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, // '3'
    {0x0E,0x1E,0x36,0x66,0x7F,0x06,0x0F,0x00}, // '4'
    {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, // '5'
    {0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00}, // '6'
    {0x7E,0x66,0x0C,0x18,0x18,0x18,0x18,0x00}, // '7'
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, // '8'
    {0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00}, // '9'
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}, // ':'
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30}, // ';'
    {0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x00}, // '<'
    {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, // '='
    {0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x00}, // '>'
    {0x3C,0x66,0x06,0x0C,0x18,0x00,0x18,0x00}, // '?'
    {0x3E,0x63,0x6F,0x6B,0x6F,0x60,0x3E,0x00}, // '@'
    {0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x00}, // 'A'
    {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}, // 'B'
    {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00}, // 'C'
    {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, // 'D'
    {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00}, // 'E'
    {0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00}, // 'F'
    {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3B,0x00}, // 'G'
    {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, // 'H'
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // 'I'
    {0x1F,0x0C,0x0C,0x0C,0x0C,0x6C,0x38,0x00}, // 'J'
    {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00}, // 'K'
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00}, // 'L'
    {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, // 'M'
    {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00}, // 'N'
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // 'O'
    {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00}, // 'P'
    {0x3C,0x66,0x66,0x66,0x66,0x3C,0x0E,0x00}, // 'Q'
    {0x7C,0x66,0x66,0x7C,0x70,0x68,0x66,0x00}, // 'R'
    {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00}, // 'S'
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // 'T'
    {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // 'U'
    {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, // 'V'
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // 'W'
    {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}, // 'X'
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00}, // 'Y'
    {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00}, // 'Z'
    {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, // '['
    {0x00,0x60,0x30,0x18,0x0C,0x06,0x03,0x00}, // '\'
    {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, // ']'
    {0x10,0x38,0x6C,0x00,0x00,0x00,0x00,0x00}, // '^'
    {0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00}, // '_'
    {0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, // '`'
    {0x00,0x00,0x3C,0x06,0x3E,0x66,0x3E,0x00}, // 'a'
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x7C,0x00}, // 'b'
    {0x00,0x00,0x3C,0x60,0x60,0x66,0x3C,0x00}, // 'c'
    {0x06,0x06,0x3E,0x66,0x66,0x66,0x3E,0x00}, // 'd'
    {0x00,0x00,0x3C,0x66,0x7E,0x60,0x3C,0x00}, // 'e'
    {0x1C,0x30,0x7C,0x30,0x30,0x30,0x30,0x00}, // 'f'
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x3C}, // 'g'
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x00}, // 'h'
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, // 'i'
    {0x06,0x00,0x06,0x06,0x06,0x06,0x66,0x3C}, // 'j'
    {0x60,0x60,0x6C,0x78,0x78,0x6C,0x66,0x00}, // 'k'
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // 'l'
    {0x00,0x00,0x66,0x7F,0x7F,0x6B,0x63,0x00}, // 'm'
    {0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x00}, // 'n'
    {0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x00}, // 'o'
    {0x00,0x00,0x7C,0x66,0x66,0x7C,0x60,0x60}, // 'p'
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x06}, // 'q'
    {0x00,0x00,0x7C,0x66,0x60,0x60,0x60,0x00}, // 'r'
    {0x00,0x00,0x3E,0x60,0x3C,0x06,0x7C,0x00}, // 's'
    {0x18,0x18,0x7E,0x18,0x18,0x18,0x0E,0x00}, // 't'
    {0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x00}, // 'u'
    {0x00,0x00,0x66,0x66,0x66,0x3C,0x18,0x00}, // 'v'
    {0x00,0x00,0x63,0x6B,0x7F,0x77,0x63,0x00}, // 'w'
    {0x00,0x00,0x66,0x3C,0x18,0x3C,0x66,0x00}, // 'x'
    {0x00,0x00,0x66,0x66,0x66,0x3E,0x06,0x3C}, // 'y'
    {0x00,0x00,0x7E,0x0C,0x18,0x30,0x7E,0x00}, // 'z'
    {0x0E,0x18,0x18,0x30,0x18,0x18,0x0E,0x00}, // '{'
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // '|'
    {0x70,0x18,0x18,0x0C,0x18,0x18,0x70,0x00}, // '}'
    {0x3B,0x6E,0x00,0x00,0x00,0x00,0x00,0x00}  // '~'
};

struct NativeWindow {
    int width = 800;
    int height = 600;
    bool shouldClose = false;
    bool isFullscreen = false;
    int activeCameraId = -1; // -1 = no camera bound, cube/mesh render falls back to a default camera at the origin
    int activeLightId = -1;  // -1 = no light bound, faces render at flat/full brightness (old behavior)
    std::vector<uint32_t> canvasBuffer;
    PlatformWindow platform;
};

static std::unordered_map<int, NativeWindow*> g_windows;
static std::unordered_map<int, Graphics::ImageBuffer> g_images;
static std::unordered_map<int, Graphics::Camera> g_cameras;
static std::unordered_map<int, Graphics::Light> g_lights;
static int g_next_window_id = 1;
static int g_next_image_id = 1;
static int g_next_camera_id = 1;
static int g_next_light_id = 1;

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
// CAMERA FUNCTIONS
// =========================================================================
// A camera is a standalone native resource (like an image) -- create one,
// position/rotate it, then bind it to a window with set_active_camera so
// draw_cube/draw_mesh render through its viewpoint. A window with no bound
// camera renders exactly like before: a fixed camera sitting at the origin
// looking down +Z with a 90 degree FOV.
static int camera_create(lua_State* L) {
    float px = static_cast<float>(luaL_optnumber(L, 1, 0.0));
    float py = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    float pz = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    float fov = static_cast<float>(luaL_optnumber(L, 4, 90.0));
    float nearPlane = static_cast<float>(luaL_optnumber(L, 5, 0.1));
    float farPlane = static_cast<float>(luaL_optnumber(L, 6, 100.0));

    Graphics::Camera cam;
    cam.position = {px, py, pz};
    cam.fov = fov;
    cam.nearPlane = nearPlane;
    cam.farPlane = farPlane;

    int camId = g_next_camera_id++;
    g_cameras[camId] = cam;
    lua_pushinteger(L, camId);
    return 1;
}

static int camera_destroy(lua_State* L) {
    int camId = static_cast<int>(luaL_checkinteger(L, 1));
    g_cameras.erase(camId);

    // Detach from any window still pointing at this camera so draw_cube/
    // draw_mesh safely fall back to the default camera instead of
    // dereferencing a dead id.
    for (auto& pair : g_windows) {
        if (pair.second->activeCameraId == camId) {
            pair.second->activeCameraId = -1;
        }
    }
    return 0;
}

static int camera_set_position(lua_State* L) {
    int camId = static_cast<int>(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));

    auto it = g_cameras.find(camId);
    if (it != g_cameras.end()) {
        it->second.position = {x, y, z};
    }
    return 0;
}

static int camera_get_position(lua_State* L) {
    int camId = static_cast<int>(luaL_checkinteger(L, 1));
    auto it = g_cameras.find(camId);
    if (it != g_cameras.end()) {
        lua_pushnumber(L, it->second.position.x);
        lua_pushnumber(L, it->second.position.y);
        lua_pushnumber(L, it->second.position.z);
        return 3;
    }
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushnil(L);
    return 3;
}

static int camera_set_rotation(lua_State* L) {
    int camId = static_cast<int>(luaL_checkinteger(L, 1));
    float pitch = static_cast<float>(luaL_checknumber(L, 2));
    float yaw = static_cast<float>(luaL_checknumber(L, 3));
    float roll = static_cast<float>(luaL_optnumber(L, 4, 0.0));

    auto it = g_cameras.find(camId);
    if (it != g_cameras.end()) {
        it->second.pitch = pitch;
        it->second.yaw = yaw;
        it->second.roll = roll;
    }
    return 0;
}

static int camera_get_rotation(lua_State* L) {
    int camId = static_cast<int>(luaL_checkinteger(L, 1));
    auto it = g_cameras.find(camId);
    if (it != g_cameras.end()) {
        lua_pushnumber(L, it->second.pitch);
        lua_pushnumber(L, it->second.yaw);
        lua_pushnumber(L, it->second.roll);
        return 3;
    }
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushnil(L);
    return 3;
}

static int camera_set_fov(lua_State* L) {
    int camId = static_cast<int>(luaL_checkinteger(L, 1));
    float fov = static_cast<float>(luaL_checknumber(L, 2));
    auto it = g_cameras.find(camId);
    if (it != g_cameras.end()) it->second.fov = fov;
    return 0;
}

static int camera_get_fov(lua_State* L) {
    int camId = static_cast<int>(luaL_checkinteger(L, 1));
    auto it = g_cameras.find(camId);
    if (it != g_cameras.end()) {
        lua_pushnumber(L, it->second.fov);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int camera_set_clip_planes(lua_State* L) {
    int camId = static_cast<int>(luaL_checkinteger(L, 1));
    float nearPlane = static_cast<float>(luaL_checknumber(L, 2));
    float farPlane = static_cast<float>(luaL_checknumber(L, 3));
    auto it = g_cameras.find(camId);
    if (it != g_cameras.end()) {
        it->second.nearPlane = nearPlane;
        it->second.farPlane = farPlane;
    }
    return 0;
}

static int window_set_active_camera(lua_State* L) {
    int winId = static_cast<int>(luaL_checkinteger(L, 1));
    int camId = static_cast<int>(luaL_checkinteger(L, 2));
    auto it = g_windows.find(winId);
    if (it != g_windows.end()) {
        it->second->activeCameraId = camId;
    }
    return 0;
}

static int window_get_active_camera(lua_State* L) {
    int winId = static_cast<int>(luaL_checkinteger(L, 1));
    auto it = g_windows.find(winId);
    if (it != g_windows.end() && it->second->activeCameraId != -1) {
        lua_pushinteger(L, it->second->activeCameraId);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

// =========================================================================
// LIGHT FUNCTIONS ("BASIC SHADERS")
// =========================================================================
// Same resource pattern as cameras: create a light, position/tune it, then
// bind it to a window with set_active_light. With no light bound, solid
// faces render at their flat input color exactly like before this feature
// existed -- shading is strictly opt-in.
static int light_create(lua_State* L) {
    float dx = static_cast<float>(luaL_optnumber(L, 1, 0.4));
    float dy = static_cast<float>(luaL_optnumber(L, 2, -0.7));
    float dz = static_cast<float>(luaL_optnumber(L, 3, 0.6));
    float ambient = static_cast<float>(luaL_optnumber(L, 4, 0.35));
    float intensity = static_cast<float>(luaL_optnumber(L, 5, 1.0));

    Graphics::Light light;
    light.direction = {dx, dy, dz};
    light.ambient = ambient;
    light.intensity = intensity;

    int lightId = g_next_light_id++;
    g_lights[lightId] = light;
    lua_pushinteger(L, lightId);
    return 1;
}

static int light_destroy(lua_State* L) {
    int lightId = static_cast<int>(luaL_checkinteger(L, 1));
    g_lights.erase(lightId);

    for (auto& pair : g_windows) {
        if (pair.second->activeLightId == lightId) {
            pair.second->activeLightId = -1;
        }
    }
    return 0;
}

static int light_set_direction(lua_State* L) {
    int lightId = static_cast<int>(luaL_checkinteger(L, 1));
    float dx = static_cast<float>(luaL_checknumber(L, 2));
    float dy = static_cast<float>(luaL_checknumber(L, 3));
    float dz = static_cast<float>(luaL_checknumber(L, 4));
    auto it = g_lights.find(lightId);
    if (it != g_lights.end()) it->second.direction = {dx, dy, dz};
    return 0;
}

static int light_get_direction(lua_State* L) {
    int lightId = static_cast<int>(luaL_checkinteger(L, 1));
    auto it = g_lights.find(lightId);
    if (it != g_lights.end()) {
        lua_pushnumber(L, it->second.direction.x);
        lua_pushnumber(L, it->second.direction.y);
        lua_pushnumber(L, it->second.direction.z);
        return 3;
    }
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushnil(L);
    return 3;
}

static int light_set_ambient(lua_State* L) {
    int lightId = static_cast<int>(luaL_checkinteger(L, 1));
    float ambient = static_cast<float>(luaL_checknumber(L, 2));
    auto it = g_lights.find(lightId);
    if (it != g_lights.end()) it->second.ambient = ambient;
    return 0;
}

static int light_get_ambient(lua_State* L) {
    int lightId = static_cast<int>(luaL_checkinteger(L, 1));
    auto it = g_lights.find(lightId);
    if (it != g_lights.end()) {
        lua_pushnumber(L, it->second.ambient);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int light_set_intensity(lua_State* L) {
    int lightId = static_cast<int>(luaL_checkinteger(L, 1));
    float intensity = static_cast<float>(luaL_checknumber(L, 2));
    auto it = g_lights.find(lightId);
    if (it != g_lights.end()) it->second.intensity = intensity;
    return 0;
}

static int light_get_intensity(lua_State* L) {
    int lightId = static_cast<int>(luaL_checkinteger(L, 1));
    auto it = g_lights.find(lightId);
    if (it != g_lights.end()) {
        lua_pushnumber(L, it->second.intensity);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int window_set_active_light(lua_State* L) {
    int winId = static_cast<int>(luaL_checkinteger(L, 1));
    int lightId = static_cast<int>(luaL_checkinteger(L, 2));
    auto it = g_windows.find(winId);
    if (it != g_windows.end()) {
        it->second->activeLightId = lightId;
    }
    return 0;
}

static int window_get_active_light(lua_State* L) {
    int winId = static_cast<int>(luaL_checkinteger(L, 1));
    auto it = g_windows.find(winId);
    if (it != g_windows.end() && it->second->activeLightId != -1) {
        lua_pushinteger(L, it->second->activeLightId);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

// =========================================================================
// CUBE / MESH FILL MODE PARSING
// =========================================================================
// Shared by draw_cube and draw_mesh -- both are "how do I rasterize this set
// of faces" and the three modes mean the same thing for either.
enum class CubeFillMode : int { Wireframe = 0, Solid = 1, Point = 2 };

static bool EqualsIgnoreCase(const char* a, const char* b) {
    while (*a && *b) {
        if (std::tolower((unsigned char)*a) != std::tolower((unsigned char)*b)) return false;
        ++a; ++b;
    }
    return *a == *b;
}

// Accepts: "wireframe" | "solid" | "point"/"points" (new), a plain
// boolean (legacy: true = wireframe, false = solid), or an integer
// (0 = wireframe, 1 = solid, 2 = point). Defaults to solid.
static CubeFillMode ParseFillMode(lua_State* L, int idx) {
    if (lua_isnoneornil(L, idx)) return CubeFillMode::Solid;

    if (lua_isstring(L, idx) && !lua_isnumber(L, idx)) {
        const char* s = lua_tostring(L, idx);
        if (EqualsIgnoreCase(s, "wireframe")) return CubeFillMode::Wireframe;
        if (EqualsIgnoreCase(s, "point") || EqualsIgnoreCase(s, "points")) return CubeFillMode::Point;
        return CubeFillMode::Solid;
    }
    if (lua_isboolean(L, idx)) {
        return lua_toboolean(L, idx) ? CubeFillMode::Wireframe : CubeFillMode::Solid;
    }
    if (lua_isnumber(L, idx)) {
        int v = static_cast<int>(lua_tointeger(L, idx));
        if (v == 0) return CubeFillMode::Wireframe;
        if (v == 2) return CubeFillMode::Point;
        return CubeFillMode::Solid;
    }
    return CubeFillMode::Solid;
}

// =========================================================================
// LUA TABLE READERS FOR draw_mesh
// =========================================================================
// Vertices: a table of {x, y, z} triples, e.g. {{0,0,0}, {1,0,0}, ...}.
static bool ReadVec3Array(lua_State* L, int idx, std::vector<Graphics::Vec3>& out) {
    if (!lua_istable(L, idx)) return false;
    size_t len = lua_rawlen(L, idx);
    out.reserve(len);
    for (size_t i = 1; i <= len; ++i) {
        lua_rawgeti(L, idx, (lua_Integer)i);
        if (lua_istable(L, -1)) {
            lua_rawgeti(L, -1, 1);
            lua_rawgeti(L, -2, 2);
            lua_rawgeti(L, -3, 3);
            float x = static_cast<float>(lua_tonumber(L, -3));
            float y = static_cast<float>(lua_tonumber(L, -2));
            float z = static_cast<float>(lua_tonumber(L, -1));
            out.push_back({x, y, z});
            lua_pop(L, 3);
        }
        lua_pop(L, 1);
    }
    return true;
}

// Faces: a table of index arrays (1-based, Lua-style), e.g.
// {{1,2,3,4}, {5,6,7,8}, ...}. Each face should be planar and wound
// consistently (counter-clockwise viewed from outside the mesh) for
// correct-facing normals when a light is bound. Indices are converted to
// 0-based here.
static bool ReadFaceArray(lua_State* L, int idx, std::vector<std::vector<int>>& out) {
    if (!lua_istable(L, idx)) return false;
    size_t len = lua_rawlen(L, idx);
    out.reserve(len);
    for (size_t i = 1; i <= len; ++i) {
        lua_rawgeti(L, idx, (lua_Integer)i);
        std::vector<int> face;
        if (lua_istable(L, -1)) {
            size_t flen = lua_rawlen(L, -1);
            face.reserve(flen);
            for (size_t j = 1; j <= flen; ++j) {
                lua_rawgeti(L, -1, (lua_Integer)j);
                int vertIdx = static_cast<int>(lua_tointeger(L, -1)) - 1;
                face.push_back(vertIdx);
                lua_pop(L, 1);
            }
        }
        out.push_back(std::move(face));
        lua_pop(L, 1);
    }
    return true;
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

// =========================================================================
// WINDOW POSITION
// =========================================================================
// NOTE (macOS only): NSWindow's coordinate space has its origin at the
// bottom-left of the primary screen, unlike Win32/X11 which use top-left.
// get/set here pass the raw Cocoa frame origin straight through -- adjust
// on the Lua side if you need top-left semantics on macOS specifically.
static int window_get_position(lua_State* L) {
    int winId = static_cast<int>(luaL_checkinteger(L, 1));
    auto it = g_windows.find(winId);
    if (it == g_windows.end()) {
        lua_pushnil(L);
        lua_pushnil(L);
        return 2;
    }
    NativeWindow* win = it->second;

#if defined(_WIN32) || defined(_WIN64)
    RECT rect{};
    GetWindowRect(win->platform.hwnd, &rect);
    lua_pushinteger(L, rect.left);
    lua_pushinteger(L, rect.top);
    return 2;
#elif defined(__APPLE__)
    CGRect frame = msgSend<CGRect>(win->platform.window, sel_registerName("frame"));
    lua_pushinteger(L, (int)frame.origin.x);
    lua_pushinteger(L, (int)frame.origin.y);
    return 2;
#else
    XWindowAttributes attrs{};
    XGetWindowAttributes(win->platform.display, win->platform.window, &attrs);
    int screen = DefaultScreen(win->platform.display);
    int x = 0, y = 0;
    Window child;
    XTranslateCoordinates(win->platform.display, win->platform.window, RootWindow(win->platform.display, screen), 0, 0, &x, &y, &child);
    lua_pushinteger(L, x);
    lua_pushinteger(L, y);
    return 2;
#endif
}

static int window_set_position(lua_State* L) {
    int winId = static_cast<int>(luaL_checkinteger(L, 1));
    int x = static_cast<int>(luaL_checkinteger(L, 2));
    int y = static_cast<int>(luaL_checkinteger(L, 3));

    auto it = g_windows.find(winId);
    if (it == g_windows.end()) return 0;
    NativeWindow* win = it->second;

#if defined(_WIN32) || defined(_WIN64)
    SetWindowPos(win->platform.hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
#elif defined(__APPLE__)
    CGPoint point = CGPointMake((CGFloat)x, (CGFloat)y);
    msgSend<void>(win->platform.window, sel_registerName("setFrameOrigin:"), point);
#else
    XMoveWindow(win->platform.display, win->platform.window, x, y);
#endif
    return 0;
}

// =========================================================================
// DISPLAY RESOLUTION (primary monitor, not window-scoped)
// =========================================================================
static int get_display_resolution(lua_State* L) {
#if defined(_WIN32) || defined(_WIN64)
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    lua_pushinteger(L, w);
    lua_pushinteger(L, h);
    return 2;
#elif defined(__APPLE__)
    CGDirectDisplayID mainDisplay = CGMainDisplayID();
    CGRect bounds = CGDisplayBounds(mainDisplay);
    lua_pushinteger(L, (int)bounds.size.width);
    lua_pushinteger(L, (int)bounds.size.height);
    return 2;
#else
    Display* tempDisplay = XOpenDisplay(NULL);
    if (!tempDisplay) {
        lua_pushnil(L);
        lua_pushnil(L);
        return 2;
    }
    int screen = DefaultScreen(tempDisplay);
    int w = DisplayWidth(tempDisplay, screen);
    int h = DisplayHeight(tempDisplay, screen);
    XCloseDisplay(tempDisplay);
    lua_pushinteger(L, w);
    lua_pushinteger(L, h);
    return 2;
#endif
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

// =========================================================================
// TEXT RENDERING QUALITY / SAMPLING
// =========================================================================
// The embedded font is a fixed 8x8 bitmap -- there's no extra detail to
// reveal past that. What DOES visibly improve with more "resolution" is
// smoothing the jagged edges you get from blowing up a 1-bit-per-pixel
// bitmap: bilinear-sample the glyph as a continuous field instead of
// nearest-neighbor, then supersample multiple sub-positions per output
// pixel and average them into a soft alpha. `quality` is that supersample
// factor (quality x quality samples per pixel); past kMaxTextQuality the
// extra samples buy nothing perceptible given the fixed source detail, so
// requests are clamped down to it.
static const int kMaxTextQuality = 8;

// Bilinearly samples the glyph's binary bitmap at a continuous (u, v)
// position, returning fractional "on-ness" in [0,1] instead of a hard 0/1.
// (u, v) are in the glyph's own 8x8 coordinate space -- bit centers sit at
// (col + 0.5, row + 0.5); positions outside the 0..8 range sample as "off".
static float SampleGlyphBilinear(const uint8_t* glyph, float u, float v) {
    float fx = u - 0.5f;
    float fy = v - 0.5f;

    int x0 = (int)std::floor(fx);
    int y0 = (int)std::floor(fy);
    float tx = fx - (float)x0;
    float ty = fy - (float)y0;

    auto bitAt = [&](int gx, int gy) -> float {
        if (gx < 0 || gx > 7 || gy < 0 || gy > 7) return 0.0f;
        return (glyph[gy] & (1 << (7 - gx))) ? 1.0f : 0.0f;
    };

    float v00 = bitAt(x0, y0);
    float v10 = bitAt(x0 + 1, y0);
    float v01 = bitAt(x0, y0 + 1);
    float v11 = bitAt(x0 + 1, y0 + 1);

    float top = v00 + (v10 - v00) * tx;
    float bottom = v01 + (v11 - v01) * tx;
    return top + (bottom - top) * ty;
}

static int get_max_text_quality(lua_State* L) {
    lua_pushinteger(L, kMaxTextQuality);
    return 1;
}

// =========================================================================
// TEXT RENDERER WITH \n AND \t CONTROL
// =========================================================================
// `quality` (last, optional arg) selects the sampling mode: 0 (default)
// keeps the exact original behavior -- hard nearest-neighbor blocky
// pixels, zero extra cost, byte-for-byte identical to before this
// feature existed. 1..kMaxTextQuality switches to the bilinear
// supersampled path described above; anything higher is clamped down to
// kMaxTextQuality. Returns the quality level actually used, so callers
// asking for more than the renderer can do can tell they got capped.
static int window_draw_text(lua_State* L) {
    int winId = static_cast<int>(luaL_checkinteger(L, 1));
    const char* text = luaL_checkstring(L, 2);
    int startX = static_cast<int>(luaL_checkinteger(L, 3));
    int startY = static_cast<int>(luaL_checkinteger(L, 4));
    int scale = static_cast<int>(luaL_optinteger(L, 5, 1));
    uint8_t r = static_cast<uint8_t>(luaL_optinteger(L, 6, 255));
    uint8_t g = static_cast<uint8_t>(luaL_optinteger(L, 7, 255));
    uint8_t b = static_cast<uint8_t>(luaL_optinteger(L, 8, 255));

    int quality = static_cast<int>(luaL_optinteger(L, 9, 0));
    if (quality < 0) quality = 0;
    if (quality > kMaxTextQuality) quality = kMaxTextQuality;

    auto it = g_windows.find(winId);
    if (it == g_windows.end()) {
        lua_pushinteger(L, quality);
        return 1;
    }
    NativeWindow* win = it->second;

    uint32_t col = (0xFF << 24) | (r << 16) | (g << 8) | b;
    int curX = startX;
    int curY = startY;
    int fontDim = 8 * scale;
    int tabSpaces = 4;

    size_t len = std::strlen(text);
    for (size_t i = 0; i < len; ++i) {
        char ch = text[i];

        if (ch == '\n') {
            curX = startX;
            curY += fontDim + (2 * scale);
            continue;
        }
        if (ch == '\t') {
            curX += fontDim * tabSpaces;
            continue;
        }
        if (ch < 32 || ch > 126) continue;

        int glyphIdx = ch - 32;
        const uint8_t* glyph = g_font8x8[glyphIdx];

        if (quality <= 0) {
            // Original path: hard nearest-neighbor blocky pixels, kept
            // exactly as-is so omitting `quality` is a complete no-op.
            for (int py = 0; py < 8; ++py) {
                for (int px = 0; px < 8; ++px) {
                    if (glyph[py] & (1 << (7 - px))) {
                        for (int sy = 0; sy < scale; ++sy) {
                            for (int sx = 0; sx < scale; ++sx) {
                                int drawX = curX + (px * scale) + sx;
                                int drawY = curY + (py * scale) + sy;
                                if (drawX >= 0 && drawX < win->width && drawY >= 0 && drawY < win->height) {
                                    win->canvasBuffer[drawY * win->width + drawX] = col;
                                }
                            }
                        }
                    }
                }
            }
        } else {
            // Anti-aliased path: bilinear-sample the glyph with
            // quality x quality sub-samples per output pixel, averaged
            // into a smooth alpha and blended over the existing canvas.
            for (int oy = 0; oy < fontDim; ++oy) {
                for (int ox = 0; ox < fontDim; ++ox) {
                    float coverage = 0.0f;
                    for (int sy = 0; sy < quality; ++sy) {
                        for (int sx = 0; sx < quality; ++sx) {
                            float u = (ox + (sx + 0.5f) / quality) / (float)scale;
                            float v = (oy + (sy + 0.5f) / quality) / (float)scale;
                            coverage += SampleGlyphBilinear(glyph, u, v);
                        }
                    }
                    coverage /= (float)(quality * quality);
                    if (coverage <= 0.003f) continue; // effectively zero -- skip the blend

                    int drawX = curX + ox;
                    int drawY = curY + oy;
                    Graphics::BlendPixel(win->canvasBuffer.data(), win->width, win->height, drawX, drawY, col, coverage);
                }
            }
        }
        curX += fontDim;
    }

    lua_pushinteger(L, quality);
    return 1;
}

// =========================================================================
// POLYGON CREATOR (SUPPORTS DIRECT TABLE OR UNPACKED TUPLES)
// =========================================================================
static int window_draw_polygon(lua_State* L) {
    int winId = static_cast<int>(luaL_checkinteger(L, 1));
    auto it = g_windows.find(winId);
    if (it == g_windows.end()) return 0;
    NativeWindow* win = it->second;

    std::vector<Graphics::Vec2> points;
    uint8_t r = 255, g = 255, b = 255;
    bool fill = false;

    if (lua_istable(L, 2)) {
        // Mode 1: Table input -> draw_polygon(winId, {{x,y}, {x,y}}, r, g, b, fill)
        // OR Flat Table -> draw_polygon(winId, {x1, y1, x2, y2, ...}, r, g, b, fill)
        size_t len = lua_rawlen(L, 2);
        for (size_t i = 1; i <= len; ++i) {
            lua_rawgeti(L, 2, i);
            if (lua_istable(L, -1)) {
                lua_rawgeti(L, -1, 1);
                lua_rawgeti(L, -2, 2);
                float px = static_cast<float>(lua_tonumber(L, -2));
                float py = static_cast<float>(lua_tonumber(L, -1));
                points.push_back({px, py});
                lua_pop(L, 2);
            } else if (lua_isnumber(L, -1)) {
                float px = static_cast<float>(lua_tonumber(L, -1));
                if (i + 1 <= len) {
                    lua_pop(L, 1);
                    lua_rawgeti(L, 2, ++i);
                    float py = static_cast<float>(lua_tonumber(L, -1));
                    points.push_back({px, py});
                }
            }
            lua_pop(L, 1);
        }
        r = static_cast<uint8_t>(luaL_optinteger(L, 3, 255));
        g = static_cast<uint8_t>(luaL_optinteger(L, 4, 255));
        b = static_cast<uint8_t>(luaL_optinteger(L, 5, 255));
        fill = lua_toboolean(L, 6);
    } else {
        // Mode 2: Direct unpacked tuple -> draw_polygon(winId, fill, r, g, b, x1, y1, x2, y2, x3, y3...)
        fill = lua_toboolean(L, 2);
        r = static_cast<uint8_t>(luaL_optinteger(L, 3, 255));
        g = static_cast<uint8_t>(luaL_optinteger(L, 4, 255));
        b = static_cast<uint8_t>(luaL_optinteger(L, 5, 255));

        int top = lua_gettop(L);
        for (int i = 6; i <= top; i += 2) {
            if (i + 1 <= top) {
                float px = static_cast<float>(luaL_checknumber(L, i));
                float py = static_cast<float>(luaL_checknumber(L, i + 1));
                points.push_back({px, py});
            }
        }
    }

    if (points.size() < 3) return 0;
    uint32_t col = (0xFF << 24) | (r << 16) | (g << 8) | b;

    if (fill) {
        // Triangle Fan Triangulation
        for (size_t i = 1; i < points.size() - 1; ++i) {
            FillTriangle(win->canvasBuffer.data(), win->width, win->height, points[0], points[i], points[i + 1], col);
        }
    } else {
        // Outline wireframe loop
        for (size_t i = 0; i < points.size(); ++i) {
            Graphics::Vec2 p1 = points[i];
            Graphics::Vec2 p2 = points[(i + 1) % points.size()];
            Graphics::DrawLine(win->canvasBuffer.data(), win->width, win->height, (int)p1.x, (int)p1.y, (int)p2.x, (int)p2.y, col);
        }
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
// 3D OBJECT MESH RENDERER: CUBE (fixed geometry, kept for convenience/perf)
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

    // Arg 14: fill mode. Backward compatible with the old boolean
    // "wireframe" flag -- true now maps to CubeFillMode::Wireframe.
    CubeFillMode fillMode = ParseFillMode(L, 14);

    uint32_t col = (0xFF << 24) | (r << 16) | (g << 8) | b;

    auto it = g_windows.find(winId);
    if (it == g_windows.end()) return 0;
    NativeWindow* win = it->second;

    // Resolve the active camera for this window, falling back to a
    // default camera at the origin (matches the old hardcoded behavior:
    // no view transform, 90 degree FOV, 0.1/100 clip planes).
    Graphics::Camera defaultCam{};
    Graphics::Camera* cam = &defaultCam;
    if (win->activeCameraId != -1) {
        auto camIt = g_cameras.find(win->activeCameraId);
        if (camIt != g_cameras.end()) cam = &camIt->second;
    }

    // Resolve the active light, if any. Unbound = no shading, faces stay
    // at their flat input color (identical to pre-lighting behavior).
    Graphics::Light* light = nullptr;
    if (win->activeLightId != -1) {
        auto lightIt = g_lights.find(win->activeLightId);
        if (lightIt != g_lights.end()) light = &lightIt->second;
    }

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
    Graphics::Matrix4x4 matProj = Graphics::Matrix4x4::Perspective(cam->fov, (float)win->width / (float)win->height, cam->nearPlane, cam->farPlane);

    Graphics::Vec3 viewPos[8];
    Graphics::Vec2 projected[8];

    for (int i = 0; i < 8; ++i) {
        Graphics::Vec3 scaled = { rawVertices[i].x * scaleX, rawVertices[i].y * scaleY, rawVertices[i].z * scaleZ };
        Graphics::Vec4 rY = matRotY.MultiplyVector(scaled);
        Graphics::Vec3 rY3 = {rY.x, rY.y, rY.z};
        Graphics::Vec4 rX = matRotX.MultiplyVector(rY3);
        Graphics::Vec3 rX3 = {rX.x, rX.y, rX.z};
        Graphics::Vec4 world = matTrans.MultiplyVector(rX3);
        Graphics::Vec3 worldPos = {world.x, world.y, world.z};

        // Run the model-space vertex through the active camera's view
        // transform before projecting, so CreateCamera/SetPosition/
        // SetRotation actually move the viewport.
        viewPos[i] = Graphics::WorldToView(worldPos, *cam);
        projected[i] = Graphics::Project3DPoint(viewPos[i], matProj, win->width, win->height);
    }

    switch (fillMode) {
        case CubeFillMode::Wireframe: {
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
            break;
        }
        case CubeFillMode::Point: {
            for (int i = 0; i < 8; ++i) {
                Graphics::DrawCircle(win->canvasBuffer.data(), win->width, win->height, (int)projected[i].x, (int)projected[i].y, 3, col, true);
            }
            break;
        }
        case CubeFillMode::Solid:
        default: {
            struct QuadFace {
                int indices[4];
                float avgZ;
                Graphics::Vec3 normal;
                bool visible;
            };

            QuadFace facesInfo[6];
            for (int i = 0; i < 6; ++i) {
                facesInfo[i].indices[0] = faces[i][0];
                facesInfo[i].indices[1] = faces[i][1];
                facesInfo[i].indices[2] = faces[i][2];
                facesInfo[i].indices[3] = faces[i][3];

                int i0 = facesInfo[i].indices[0];
                int i1 = facesInfo[i].indices[1];
                int i2 = facesInfo[i].indices[2];

                Graphics::Vec3 e1 = Graphics::Subtract(viewPos[i1], viewPos[i0]);
                Graphics::Vec3 e2 = Graphics::Subtract(viewPos[i2], viewPos[i0]);
                facesInfo[i].normal = Graphics::Cross(e1, e2);

                // Backface cull: for a convex solid like this cube, a face
                // whose outward normal points away from the camera (positive
                // dot with the camera-to-face vector; camera sits at the
                // view-space origin) is always fully hidden behind the near
                // faces. Skipping it isn't just a perf win here -- it's the
                // fix for the "notched"/discolored patches you'd otherwise
                // get with a light bound: centroid-based painter's sorting
                // can occasionally still place a hidden backface on top of a
                // visible one, and once backfaces and front faces can have
                // very different shades (instead of all being one flat
                // color), that latent mis-sort became visible as a mismatched
                // patch. Culling removes the hidden geometry outright instead
                // of relying on sort order to hide it.
                facesInfo[i].visible = Graphics::Dot(facesInfo[i].normal, viewPos[i0]) < 0.0f;

                float sumZ = 0;
                for (int k = 0; k < 4; ++k) {
                    sumZ += viewPos[faces[i][k]].z;
                }
                facesInfo[i].avgZ = sumZ / 4.0f;
            }

            std::sort(std::begin(facesInfo), std::end(facesInfo), [](const QuadFace& a, const QuadFace& b) {
                return a.avgZ > b.avgZ;
            });

            for (int i = 0; i < 6; ++i) {
                if (!facesInfo[i].visible) continue;

                int i0 = facesInfo[i].indices[0];
                int i1 = facesInfo[i].indices[1];
                int i2 = facesInfo[i].indices[2];
                int i3 = facesInfo[i].indices[3];

                Graphics::Vec2 p0 = projected[i0];
                Graphics::Vec2 p1 = projected[i1];
                Graphics::Vec2 p2 = projected[i2];
                Graphics::Vec2 p3 = projected[i3];

                uint32_t faceCol = col;
                if (light) {
                    float shade = Graphics::ComputeFaceShade(facesInfo[i].normal, *light);
                    faceCol = Graphics::ShadeColor(col, shade);
                }

                FillTriangle(win->canvasBuffer.data(), win->width, win->height, p0, p1, p2, faceCol);
                FillTriangle(win->canvasBuffer.data(), win->width, win->height, p0, p2, p3, faceCol);
            }
            break;
        }
    }
    return 0;
}

// =========================================================================
// 3D OBJECT MESH RENDERER: GENERIC MESH (custom vertices + faces)
// =========================================================================
// The 3D equivalent of draw_polygon: instead of a fixed cube shape, you
// supply your own vertex list and face list (each face a list of vertex
// indices), and it runs through the exact same camera/projection/lighting
// pipeline as draw_cube. Good for pyramids, custom props, procedural
// shapes, etc.
static int window_draw_mesh(lua_State* L) {
    int winId = static_cast<int>(luaL_checkinteger(L, 1));

    std::vector<Graphics::Vec3> localVerts;
    std::vector<std::vector<int>> faceList;
    if (!ReadVec3Array(L, 2, localVerts) || !ReadFaceArray(L, 3, faceList)) return 0;
    if (localVerts.empty() || faceList.empty()) return 0;

    float posX = static_cast<float>(luaL_optnumber(L, 4, 0.0));
    float posY = static_cast<float>(luaL_optnumber(L, 5, 0.0));
    float posZ = static_cast<float>(luaL_optnumber(L, 6, 3.0));

    float rotX = static_cast<float>(luaL_optnumber(L, 7, 0.0));
    float rotY = static_cast<float>(luaL_optnumber(L, 8, 0.0));
    float rotZ = static_cast<float>(luaL_optnumber(L, 9, 0.0));

    float scaleX = static_cast<float>(luaL_optnumber(L, 10, 1.0));
    float scaleY = static_cast<float>(luaL_optnumber(L, 11, 1.0));
    float scaleZ = static_cast<float>(luaL_optnumber(L, 12, 1.0));

    uint8_t r = static_cast<uint8_t>(luaL_optinteger(L, 13, 255));
    uint8_t g = static_cast<uint8_t>(luaL_optinteger(L, 14, 255));
    uint8_t b = static_cast<uint8_t>(luaL_optinteger(L, 15, 255));

    CubeFillMode fillMode = ParseFillMode(L, 16);

    uint32_t baseCol = (0xFF << 24) | (r << 16) | (g << 8) | b;

    auto it = g_windows.find(winId);
    if (it == g_windows.end()) return 0;
    NativeWindow* win = it->second;

    Graphics::Camera defaultCam{};
    Graphics::Camera* cam = &defaultCam;
    if (win->activeCameraId != -1) {
        auto camIt = g_cameras.find(win->activeCameraId);
        if (camIt != g_cameras.end()) cam = &camIt->second;
    }

    Graphics::Light* light = nullptr;
    if (win->activeLightId != -1) {
        auto lightIt = g_lights.find(win->activeLightId);
        if (lightIt != g_lights.end()) light = &lightIt->second;
    }

    Graphics::Matrix4x4 matRotX = Graphics::Matrix4x4::RotationX(rotX);
    Graphics::Matrix4x4 matRotY = Graphics::Matrix4x4::RotationY(rotY);
    Graphics::Matrix4x4 matRotZ = Graphics::Matrix4x4::RotationZ(rotZ);
    Graphics::Matrix4x4 matTrans = Graphics::Matrix4x4::Translation(posX, posY, posZ);
    Graphics::Matrix4x4 matProj = Graphics::Matrix4x4::Perspective(cam->fov, (float)win->width / (float)win->height, cam->nearPlane, cam->farPlane);

    size_t vertCount = localVerts.size();
    std::vector<Graphics::Vec3> viewPos(vertCount);
    std::vector<Graphics::Vec2> projected(vertCount);

    for (size_t i = 0; i < vertCount; ++i) {
        Graphics::Vec3 scaled = { localVerts[i].x * scaleX, localVerts[i].y * scaleY, localVerts[i].z * scaleZ };
        Graphics::Vec4 rZ = matRotZ.MultiplyVector(scaled);
        Graphics::Vec3 rZ3 = {rZ.x, rZ.y, rZ.z};
        Graphics::Vec4 rY = matRotY.MultiplyVector(rZ3);
        Graphics::Vec3 rY3 = {rY.x, rY.y, rY.z};
        Graphics::Vec4 rX = matRotX.MultiplyVector(rY3);
        Graphics::Vec3 rX3 = {rX.x, rX.y, rX.z};
        Graphics::Vec4 world = matTrans.MultiplyVector(rX3);
        Graphics::Vec3 worldPos = {world.x, world.y, world.z};

        viewPos[i] = Graphics::WorldToView(worldPos, *cam);
        projected[i] = Graphics::Project3DPoint(viewPos[i], matProj, win->width, win->height);
    }

    switch (fillMode) {
        case CubeFillMode::Wireframe: {
            for (auto& face : faceList) {
                size_t n = face.size();
                if (n < 2) continue;
                for (size_t i = 0; i < n; ++i) {
                    int a = face[i];
                    int bIdx = face[(i + 1) % n];
                    if (a < 0 || a >= (int)vertCount || bIdx < 0 || bIdx >= (int)vertCount) continue;
                    Graphics::Vec2 p1 = projected[a];
                    Graphics::Vec2 p2 = projected[bIdx];
                    Graphics::DrawLine(win->canvasBuffer.data(), win->width, win->height, (int)p1.x, (int)p1.y, (int)p2.x, (int)p2.y, baseCol);
                }
            }
            break;
        }
        case CubeFillMode::Point: {
            for (size_t i = 0; i < vertCount; ++i) {
                Graphics::DrawCircle(win->canvasBuffer.data(), win->width, win->height, (int)projected[i].x, (int)projected[i].y, 3, baseCol, true);
            }
            break;
        }
        case CubeFillMode::Solid:
        default: {
            struct RenderFace {
                std::vector<int> indices;
                float avgZ;
                uint32_t color;
            };
            std::vector<RenderFace> renderFaces;
            renderFaces.reserve(faceList.size());

            for (auto& face : faceList) {
                if (face.size() < 3) continue;

                bool validIdx = true;
                for (int idx : face) {
                    if (idx < 0 || idx >= (int)vertCount) { validIdx = false; break; }
                }
                if (!validIdx) continue;

                // One normal per face, from its first three vertices --
                // used for both backface culling and shading. Faces must be
                // wound consistently (CCW viewed from outside) for it to
                // point outward.
                Graphics::Vec3 e1 = Graphics::Subtract(viewPos[face[1]], viewPos[face[0]]);
                Graphics::Vec3 e2 = Graphics::Subtract(viewPos[face[2]], viewPos[face[0]]);
                Graphics::Vec3 normal = Graphics::Cross(e1, e2);

                // Same backface cull as draw_cube: skip faces pointing away
                // from the camera (camera sits at the view-space origin) so
                // hidden geometry never has a chance to get painter's-sorted
                // on top of visible faces. For a one-sided open surface
                // (e.g. a single flat quad), this means it's invisible from
                // behind -- that's expected, standard "solid faces have a
                // front" behavior, not a bug.
                bool faceVisible = Graphics::Dot(normal, viewPos[face[0]]) < 0.0f;
                if (!faceVisible) continue;

                float sumZ = 0.0f;
                for (int idx : face) sumZ += viewPos[idx].z;
                float avgZ = sumZ / (float)face.size();

                uint32_t faceCol = baseCol;
                if (light) {
                    float shade = Graphics::ComputeFaceShade(normal, *light);
                    faceCol = Graphics::ShadeColor(baseCol, shade);
                }

                renderFaces.push_back({face, avgZ, faceCol});
            }

            std::sort(renderFaces.begin(), renderFaces.end(), [](const RenderFace& a, const RenderFace& b) {
                return a.avgZ > b.avgZ;
            });

            for (auto& rf : renderFaces) {
                // Fan triangulation from the face's first vertex, same
                // approach as draw_polygon.
                for (size_t i = 1; i + 1 < rf.indices.size(); ++i) {
                    FillTriangle(win->canvasBuffer.data(), win->width, win->height,
                        projected[rf.indices[0]], projected[rf.indices[i]], projected[rf.indices[i + 1]], rf.color);
                }
            }
            break;
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
        {"get_position", window_get_position},
        {"set_position", window_set_position},
        {"get_display_resolution", get_display_resolution},
        {"poll_events", window_poll_events},
        {"should_close", window_should_close},
        {"clear_canvas", window_clear_canvas},
        {"draw_rect", window_draw_rect},
        {"draw_line", window_draw_line},
        {"draw_circle", window_draw_circle},
        {"draw_text", window_draw_text},
        {"get_max_text_quality", get_max_text_quality},
        {"draw_polygon", window_draw_polygon},
        {"create_image", image_create},
        {"draw_image", window_draw_image},
        {"draw_cube", window_draw_cube},
        {"draw_mesh", window_draw_mesh},
        {"create_camera", camera_create},
        {"destroy_camera", camera_destroy},
        {"camera_set_position", camera_set_position},
        {"camera_get_position", camera_get_position},
        {"camera_set_rotation", camera_set_rotation},
        {"camera_get_rotation", camera_get_rotation},
        {"camera_set_fov", camera_set_fov},
        {"camera_get_fov", camera_get_fov},
        {"camera_set_clip_planes", camera_set_clip_planes},
        {"set_active_camera", window_set_active_camera},
        {"get_active_camera", window_get_active_camera},
        {"create_light", light_create},
        {"destroy_light", light_destroy},
        {"light_set_direction", light_set_direction},
        {"light_get_direction", light_get_direction},
        {"light_set_ambient", light_set_ambient},
        {"light_get_ambient", light_get_ambient},
        {"light_set_intensity", light_set_intensity},
        {"light_get_intensity", light_get_intensity},
        {"set_active_light", window_set_active_light},
        {"get_active_light", window_get_active_light},
        {"swap_buffers", window_swap_buffers},
        {"destroy", window_destroy},
        {nullptr, nullptr}
    };
    
    luaL_newlib(L, WindowManagement);
    return 1;
}
