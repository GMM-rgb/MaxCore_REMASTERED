#include <lua.hpp>
#include <unordered_map>
#include <string>

// Support up to 512 keys / mouse buttons
static bool g_currKeys[512] = { false };
static bool g_prevKeys[512] = { false };

static float g_mouseX = 0.0f;
static float g_mouseY = 0.0f;
static float g_mouseDeltaX = 0.0f;
static float g_mouseDeltaY = 0.0f;

// Call this at the VERY END of every game frame in C++!
void Input_EndFrame() {
    // Copy current state to previous state array
    for (int i = 0; i < 512; ++i) {
        g_prevKeys[i] = g_currKeys[i];
    }
    // Reset mouse deltas after frame consumption
    g_mouseDeltaX = 0.0f;
    g_mouseDeltaY = 0.0f;
}

// Call this from your OS window event loop (Win32 / GLFW / SDL)
void Input_OnKey(int keyCode, bool isDown) {
    if (keyCode >= 0 && keyCode < 512) {
        g_currKeys[keyCode] = isDown;
    }
}

void Input_OnMouseMove(float x, float y) {
    g_mouseDeltaX = x - g_mouseX;
    g_mouseDeltaY = y - g_mouseY;
    g_mouseX = x;
    g_mouseY = y;
}

// =========================================================================
// LUA BINDINGS
// =========================================================================

// input_native.is_key_down(key_code) -> bool
static int l_is_key_down(lua_State* L) {
    int key = (int)luaL_checkinteger(L, 1);
    bool isDown = (key >= 0 && key < 512) ? g_currKeys[key] : false;
    lua_pushboolean(L, isDown);
    return 1;
}

// input_native.is_key_pressed(key_code) -> bool
static int l_is_key_pressed(lua_State* L) {
    int key = (int)luaL_checkinteger(L, 1);
    bool pressed = (key >= 0 && key < 512) ? (g_currKeys[key] && !g_prevKeys[key]) : false;
    lua_pushboolean(L, pressed);
    return 1;
}

// input_native.is_key_released(key_code) -> bool
static int l_is_key_released(lua_State* L) {
    int key = (int)luaL_checkinteger(L, 1);
    bool released = (key >= 0 && key < 512) ? (!g_currKeys[key] && g_prevKeys[key]) : false;
    lua_pushboolean(L, released);
    return 1;
}

// input_native.get_mouse_position() -> x, y
static int l_get_mouse_position(lua_State* L) {
    lua_pushnumber(L, g_mouseX);
    lua_pushnumber(L, g_mouseY);
    return 2;
}

// input_native.get_mouse_delta() -> dx, dy
static int l_get_mouse_delta(lua_State* L) {
    lua_pushnumber(L, g_mouseDeltaX);
    lua_pushnumber(L, g_mouseDeltaY);
    return 2;
}

extern "C" int luaopen_input_native(lua_State* L) {
    static const luaL_Reg funcs[] = {
        {"is_key_down", l_is_key_down},
        {"is_key_pressed", l_is_key_pressed},
        {"is_key_released", l_is_key_released},
        {"get_mouse_position", l_get_mouse_position},
        {"get_mouse_delta", l_get_mouse_delta},
        {nullptr, nullptr},
    }; luaL_newlib(L, funcs); return 1;
}
