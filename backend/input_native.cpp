#include <lua.hpp>
#include <unordered_map>
#include <string>

// =========================================================================
// PLATFORM-SPECIFIC INCLUDES & EXPORTS
// =========================================================================

#ifdef _WIN32
    #include <windows.h>
    #define INPUT_EXPORT __declspec(dllexport)
#elif defined(__APPLE__)
    #include <ApplicationServices/ApplicationServices.h>
    #include <objc/objc.h>
    #include <objc/message.h>
    #include <unistd.h>
    #define INPUT_EXPORT __attribute__((visibility("default")))
#elif defined(__linux__)
    #include <X11/Xlib.h>
    #include <X11/keysym.h>
    #include <unistd.h>
    #define INPUT_EXPORT __attribute__((visibility("default")))
#else
    #define INPUT_EXPORT __attribute__((visibility("default")))
#endif

// Support up to 512 keys / mouse buttons
static bool g_currKeys[512] = { false };
static bool g_prevKeys[512] = { false };

static float g_mouseX = 0.0f;
static float g_mouseY = 0.0f;
static float g_mouseDeltaX = 0.0f;
static float g_mouseDeltaY = 0.0f;

static bool g_globalInput = true;

// =========================================================================
// MACOS KEYCODE MAPPER (Windows VK Code -> macOS CGKeyCode)
// =========================================================================
#ifdef __APPLE__
static CGKeyCode WinVKToMacVK(int vk) {
    switch (vk) {
        case 65: return 0x00; // A
        case 66: return 0x0B; // B
        case 67: return 0x08; // C
        case 68: return 0x02; // D
        case 69: return 0x0E; // E
        case 70: return 0x03; // F
        case 71: return 0x05; // G
        case 72: return 0x04; // H
        case 73: return 0x22; // I
        case 74: return 0x26; // J
        case 75: return 0x28; // K
        case 76: return 0x25; // L
        case 77: return 0x2E; // M
        case 78: return 0x2D; // N
        case 79: return 0x1F; // O
        case 80: return 0x23; // P
        case 81: return 0x0C; // Q
        case 82: return 0x0F; // R
        case 83: return 0x01; // S
        case 84: return 0x11; // T
        case 85: return 0x20; // U
        case 86: return 0x09; // V
        case 87: return 0x0D; // W
        case 88: return 0x07; // X
        case 89: return 0x10; // Y
        case 90: return 0x06; // Z
        case 48: return 0x1D; // 0
        case 49: return 0x12; // 1
        case 50: return 0x13; // 2
        case 51: return 0x14; // 3
        case 52: return 0x15; // 4
        case 53: return 0x17; // 5
        case 54: return 0x1C; // 6
        case 55: return 0x19; // 7
        case 56: return 0x1B; // 8
        case 57: return 0x1A; // 9
        case 32: return 0x49; // Space
        case 13: return 0x24; // Return/Enter
        case 27: return 0x35; // Escape
        case 9:  return 0x30; // Tab
        case 16: return 0x38; // Left Shift
        case 17: return 0x3B; // Left Control
        case 18: return 0x3A; // Left Option/Alt
        case 91: return 0x37; // Left Command
        case 92: return 0x36; // Right Command
        case 37: return 0x7B; // Left
        case 38: return 0x7E; // Up
        case 39: return 0x7C; // Right
        case 40: return 0x7D; // Down
        default: return 0xFFFF;
    }
}
#endif

// =========================================================================
// LINUX KEYCODE MAPPER (Windows VK Code -> X11 KeySym)
// =========================================================================
#ifdef __linux__
static KeySym WinVKToX11Keysym(int vk) {
    if (vk >= 'A' && vk <= 'Z') return XK_a + (vk - 'A');
    if (vk >= '0' && vk <= '9') return XK_0 + (vk - '0');
    switch (vk) {
        case 32: return XK_space;
        case 13: return XK_Return;
        case 27: return XK_Escape;
        case 9:  return XK_Tab;
        case 16: return XK_Shift_L;
        case 17: return XK_Control_L;
        case 18: return XK_Alt_L;
        case 91: return XK_Super_L;
        case 92: return XK_Super_R;
        case 37: return XK_Left;
        case 38: return XK_Up;
        case 39: return XK_Right;
        case 40: return XK_Down;
        default: return NoSymbol;
    }
}
#endif

// =========================================================================
// LUA BINDINGS
// =========================================================================

static int l_is_key_down(lua_State* L) {
    int key = (int)luaL_checkinteger(L, 1);
    bool isDown = (key >= 0 && key < 512) ? g_currKeys[key] : false;
    lua_pushboolean(L, isDown);
    return 1;
}

static int l_is_key_pressed(lua_State* L) {
    int key = (int)luaL_checkinteger(L, 1);
    bool pressed = (key >= 0 && key < 512) ? (g_currKeys[key] && !g_prevKeys[key]) : false;
    lua_pushboolean(L, pressed);
    return 1;
}

static int l_is_key_released(lua_State* L) {
    int key = (int)luaL_checkinteger(L, 1);
    bool released = (key >= 0 && key < 512) ? (!g_currKeys[key] && g_prevKeys[key]) : false;
    lua_pushboolean(L, released);
    return 1;
}

static int l_get_mouse_position(lua_State* L) {
    lua_pushnumber(L, g_mouseX);
    lua_pushnumber(L, g_mouseY);
    return 2;
}

static int l_get_mouse_delta(lua_State* L) {
    lua_pushnumber(L, g_mouseDeltaX);
    lua_pushnumber(L, g_mouseDeltaY);
    return 2;
}

static int l_set_global_input(lua_State* L) {
    g_globalInput = lua_toboolean(L, 1);
    return 0;
}

static int l_update(lua_State* L) {
    for (int i = 0; i < 512; ++i) {
        g_prevKeys[i] = g_currKeys[i];
    }
    g_mouseDeltaX = 0.0f;
    g_mouseDeltaY = 0.0f;

// -------------------------------------------------------------------------
// WINDOWS POLLING
// -------------------------------------------------------------------------
#ifdef _WIN32
    bool shouldPoll = g_globalInput;
    if (!shouldPoll) {
        HWND fgWindow = GetForegroundWindow();
        DWORD fgPid = 0;
        GetWindowThreadProcessId(fgWindow, &fgPid);
        shouldPoll = (fgPid == GetCurrentProcessId());
    }

    if (shouldPoll) {
        for (int i = 0; i < 256; ++i) {
            g_currKeys[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
        }
        g_currKeys[1] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        g_currKeys[2] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        g_currKeys[3] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;

        POINT p;
        if (GetCursorPos(&p)) {
            g_mouseDeltaX = (float)p.x - g_mouseX;
            g_mouseDeltaY = (float)p.y - g_mouseY;
            g_mouseX = (float)p.x;
            g_mouseY = (float)p.y;
        }
    }

// -------------------------------------------------------------------------
// MACOS POLLING (Includes terminal parent process check)
// -------------------------------------------------------------------------
#elif defined(__APPLE__)
    bool shouldPoll = g_globalInput;

    if (!shouldPoll) {
        id workspaceClass = (id)objc_getClass("NSWorkspace");
        if (workspaceClass) {
            SEL sharedWorkspaceSel = sel_registerName("sharedWorkspace");
            id workspace = ((id (*)(id, SEL))objc_msgSend)(workspaceClass, sharedWorkspaceSel);
            if (workspace) {
                SEL frontmostAppSel = sel_registerName("frontmostApplication");
                id frontmostApp = ((id (*)(id, SEL))objc_msgSend)(workspace, frontmostAppSel);
                if (frontmostApp) {
                    SEL pidSel = sel_registerName("processIdentifier");
                    pid_t frontPid = ((pid_t (*)(id, SEL))objc_msgSend)(frontmostApp, pidSel);
                    
                    pid_t myPid = getpid();
                    pid_t parentPid = getppid(); // Terminal process ID
                    
                    shouldPoll = (frontPid == myPid || frontPid == parentPid);
                }
            }
        }
    }

    if (shouldPoll) {
        for (int vk = 0; vk < 256; ++vk) {
            CGKeyCode macCode = WinVKToMacVK(vk);
            if (macCode != 0xFFFF) {
                g_currKeys[vk] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, macCode);
            }
        }

        g_currKeys[1] = CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, kCGMouseButtonLeft);
        g_currKeys[2] = CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, kCGMouseButtonRight);
        g_currKeys[3] = CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, kCGMouseButtonCenter);

        CGEventRef event = CGEventCreate(NULL);
        if (event) {
            CGPoint point = CGEventGetLocation(event);
            CFRelease(event);
            g_mouseDeltaX = (float)point.x - g_mouseX;
            g_mouseDeltaY = (float)point.y - g_mouseY;
            g_mouseX = (float)point.x;
            g_mouseY = (float)point.y;
        }
    }

// -------------------------------------------------------------------------
// LINUX (X11) POLLING
// -------------------------------------------------------------------------
#elif defined(__linux__)
    Display* display = XOpenDisplay(NULL);
    if (display) {
        Window root = DefaultRootWindow(display);
        bool shouldPoll = g_globalInput;

        if (!shouldPoll) {
            Window focusedWindow;
            int revertTo;
            XGetInputFocus(display, &focusedWindow, &revertTo);
            shouldPoll = (focusedWindow != None);
        }

        if (shouldPoll) {
            char keymap[32];
            XQueryKeymap(display, keymap);

            for (int vk = 0; vk < 256; ++vk) {
                KeySym sym = WinVKToX11Keysym(vk);
                if (sym != NoSymbol) {
                    KeyCode kc = XKeysymToKeycode(display, sym);
                    if (kc != 0) {
                        g_currKeys[vk] = (keymap[kc / 8] & (1 << (kc % 8))) != 0;
                    }
                }
            }

            Window rootReturn, childReturn;
            int rootX, rootY, winX, winY;
            unsigned int mask;
            if (XQueryPointer(display, root, &rootReturn, &childReturn, &rootX, &rootY, &winX, &winY, &mask)) {
                g_currKeys[1] = (mask & Button1Mask) != 0;
                g_currKeys[2] = (mask & Button3Mask) != 0;
                g_currKeys[3] = (mask & Button2Mask) != 0;

                g_mouseDeltaX = (float)rootX - g_mouseX;
                g_mouseDeltaY = (float)rootY - g_mouseY;
                g_mouseX = (float)rootX;
                g_mouseY = (float)rootY;
            }
        }
        XCloseDisplay(display);
    }
#endif

    return 0;
}

// -------------------------------------------------------------------------
// EXPORT TABLE
// -------------------------------------------------------------------------
extern "C" INPUT_EXPORT int luaopen_input_native(lua_State* L) {
    static const luaL_Reg funcs[] = {
        {"is_key_down", l_is_key_down},
        {"is_key_pressed", l_is_key_pressed},
        {"is_key_released", l_is_key_released},
        {"get_mouse_position", l_get_mouse_position},
        {"get_mouse_delta", l_get_mouse_delta},
        {"set_global_input", l_set_global_input},
        {"update", l_update},
        {nullptr, nullptr},
    }; 
    luaL_newlib(L, funcs); 
    return 1;
}
