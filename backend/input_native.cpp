#include <lua.hpp>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <cctype>
#include <climits>
#include <cstring>

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
    #include <X11/extensions/XTest.h>
    #include <unistd.h>
    #define INPUT_EXPORT __attribute__((visibility("default")))
#else
    #define INPUT_EXPORT __attribute__((visibility("default")))
#endif

// =========================================================================
// INPUT STATE
// =========================================================================

static bool g_currKeys[512] = { false };
static bool g_prevKeys[512] = { false };

static float g_mouseX = 0.0f;
static float g_mouseY = 0.0f;
static float g_mouseDeltaX = 0.0f;
static float g_mouseDeltaY = 0.0f;

static bool g_globalInput = true;

#ifdef __APPLE__
// Caps Lock is a latched modifier on macOS, not an ordinary held key.
// Prevent repeated key-down calls from toggling it repeatedly before key-up.
static bool g_macCapsLockInjectionActive = false;
#endif

// Windows virtual-key codes are used internally on every platform.
//
// Mouse button state indices 1, 2, and 3 retain the existing convention:
// mouse1 = left, mouse2 = right, mouse3 = middle.

// =========================================================================
// KEY-NAME RESOLUTION
// =========================================================================

static std::string NormalizeKeyName(const char* name) {
    std::string result = name ? name : "";

    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        }
    );

    return result;
}

static int KeyNameToVK(const char* name) {
    const std::string key = NormalizeKeyName(name);

    // Letters use their uppercase Windows virtual-key codes.
    if (key.size() == 1) {
        const unsigned char c = static_cast<unsigned char>(key[0]);

        if (c >= 'a' && c <= 'z') {
            return static_cast<int>(c - 'a' + 'A');
        }

        if (c >= '0' && c <= '9') {
            return static_cast<int>(c);
        }
    }

    static const std::unordered_map<std::string, int> keyMap = {
        // Common control keys
        {"tab", 0x09},
        {"space", 0x20},
        {"return", 0x0D},
        {"enter", 0x0D},
        {"escape", 0x1B},
        {"backspace", 0x08},
        {"delete", 0x2E},
        {"insert", 0x2D},
        {"home", 0x24},
        {"end", 0x23},
        {"pageup", 0x21},
        {"pagedown", 0x22},

        // Function keys: F1-F12
        {"f1", 0x70},
        {"f2", 0x71},
        {"f3", 0x72},
        {"f4", 0x73},
        {"f5", 0x74},
        {"f6", 0x75},
        {"f7", 0x76},
        {"f8", 0x77},
        {"f9", 0x78},
        {"f10", 0x79},
        {"f11", 0x7A},
        {"f12", 0x7B},

        // Lock keys
        {"capslock", 0x14},
        {"numlock", 0x90},
        {"scrolllock", 0x91},

        // Generic modifier keys
        {"shift", 0x10},
        {"ctrl", 0x11},
        {"alt", 0x12},

        // Left/right modifier keys
        {"lshift", 0xA0},
        {"rshift", 0xA1},
        {"lctrl", 0xA2},
        {"rctrl", 0xA3},
        {"lalt", 0xA4},
        {"ralt", 0xA5},

        // Super / Windows / Command / Meta aliases
        {"super", 0x5B},
        {"win", 0x5B},
        {"cmd", 0x5B},
        {"meta", 0x5B},

        // Left-side aliases
        {"lsuper", 0x5B},
        {"lwin", 0x5B},
        {"lcmd", 0x5B},
        {"lmeta", 0x5B},

        // Right-side aliases
        {"rsuper", 0x5C},
        {"rwin", 0x5C},
        {"rcmd", 0x5C},
        {"rmeta", 0x5C},

        // Arrow keys
        {"left", 0x25},
        {"up", 0x26},
        {"right", 0x27},
        {"down", 0x28},

        // Punctuation keys (US-layout virtual-key positions)
        {"minus", 0xBD},
        {"equals", 0xBB},
        {"leftbracket", 0xDB},
        {"rightbracket", 0xDD},
        {"backslash", 0xDC},
        {"semicolon", 0xBA},
        {"apostrophe", 0xDE},
        {"grave", 0xC0},
        {"comma", 0xBC},
        {"period", 0xBE},
        {"slash", 0xBF},

        // Existing mouse-state indices
        {"mouse1", 1},
        {"mouse2", 2},
        {"mouse3", 3},
    };

    const auto it = keyMap.find(key);
    return it != keyMap.end() ? it->second : -1;
}

static bool IsMouseKeyName(const char* name) {
    const std::string key = NormalizeKeyName(name);

    return key == "mouse1"
        || key == "mouse2"
        || key == "mouse3";
}

// Accept either a numeric key code or a named key.
//
// Mouse names are allowed for state queries but rejected for keyboard
// simulation because this module has no mouse-button injection binding.
static int ReadKeyArgument(
    lua_State* L,
    int index,
    bool allowMouseNames
) {
    if (lua_type(L, index) == LUA_TSTRING) {
        const char* name = lua_tostring(L, index);

        if (!allowMouseNames && IsMouseKeyName(name)) {
            luaL_argerror(
                L,
                index,
                "mouse button names cannot be used as keyboard keys"
            );
            return -1;
        }

        const int vk = KeyNameToVK(name);

        if (vk < 0) {
            luaL_argerror(L, index, "unknown key name");
            return -1;
        }

        return vk;
    }

    const lua_Integer value = luaL_checkinteger(L, index);

    if (value < 0 || value >= 512) {
        return -1;
    }

    return static_cast<int>(value);
}

// =========================================================================
// MACOS KEYCODE MAPPER
// Windows virtual-key code -> macOS CGKeyCode
// =========================================================================

#ifdef __APPLE__

static CGKeyCode WinVKToMacVK(int vk) {
    // Letters
    switch (vk) {
        case 'A': return 0x00;
        case 'B': return 0x0B;
        case 'C': return 0x08;
        case 'D': return 0x02;
        case 'E': return 0x0E;
        case 'F': return 0x03;
        case 'G': return 0x05;
        case 'H': return 0x04;
        case 'I': return 0x22;
        case 'J': return 0x26;
        case 'K': return 0x28;
        case 'L': return 0x25;
        case 'M': return 0x2E;
        case 'N': return 0x2D;
        case 'O': return 0x1F;
        case 'P': return 0x23;
        case 'Q': return 0x0C;
        case 'R': return 0x0F;
        case 'S': return 0x01;
        case 'T': return 0x11;
        case 'U': return 0x20;
        case 'V': return 0x09;
        case 'W': return 0x0D;
        case 'X': return 0x07;
        case 'Y': return 0x10;
        case 'Z': return 0x06;

        // Numbers (correct ANSI macOS key positions)
        case '0': return 0x1D;
        case '1': return 0x12;
        case '2': return 0x13;
        case '3': return 0x14;
        case '4': return 0x15;
        case '5': return 0x17;
        case '6': return 0x16;
        case '7': return 0x1A;
        case '8': return 0x1C;
        case '9': return 0x19;

        // Common keys
        case 0x08: return 0x33; // Backspace
        case 0x09: return 0x30; // Tab
        case 0x0D: return 0x24; // Return
        case 0x1B: return 0x35; // Escape
        case 0x20: return 0x31; // Space

        case 0x2D: return 0x72; // Insert position / Help key
        case 0x2E: return 0x75; // Forward Delete
        case 0x21: return 0x74; // Page Up
        case 0x22: return 0x79; // Page Down
        case 0x23: return 0x77; // End
        case 0x24: return 0x73; // Home

        // Generic modifiers
        case 0x10: return 0x38; // Shift
        case 0x11: return 0x3B; // Control
        case 0x12: return 0x3A; // Option / Alt

        // Lock keys
        case 0x14: return 0x39; // Caps Lock
        case 0x90: return 0x47; // Num Lock equivalent / Clear
        case 0x91: return 0x6B; // Scroll Lock equivalent / F14

        // Command / Super
        case 0x5B: return 0x37; // Left Command
        case 0x5C: return 0x36; // Right Command

        // Left/right modifiers
        case 0xA0: return 0x38; // Left Shift
        case 0xA1: return 0x3C; // Right Shift
        case 0xA2: return 0x3B; // Left Control
        case 0xA3: return 0x3E; // Right Control
        case 0xA4: return 0x3A; // Left Option
        case 0xA5: return 0x3D; // Right Option

        // Arrow keys
        case 0x25: return 0x7B; // Left
        case 0x26: return 0x7E; // Up
        case 0x27: return 0x7C; // Right
        case 0x28: return 0x7D; // Down

        // Punctuation (US ANSI keyboard positions)
        case 0xBD: return 0x1B; // Minus
        case 0xBB: return 0x18; // Equals
        case 0xDB: return 0x21; // Left bracket
        case 0xDD: return 0x1E; // Right bracket
        case 0xDC: return 0x2A; // Backslash
        case 0xBA: return 0x29; // Semicolon
        case 0xDE: return 0x27; // Apostrophe
        case 0xC0: return 0x32; // Grave
        case 0xBC: return 0x2B; // Comma
        case 0xBE: return 0x2F; // Period
        case 0xBF: return 0x2C; // Slash

        default:
            break;
    }

    // F1-F12 do not map to sequential virtual key codes on macOS.
    switch (vk) {
        case 0x70: return 0x7A; // F1
        case 0x71: return 0x78; // F2
        case 0x72: return 0x63; // F3
        case 0x73: return 0x76; // F4
        case 0x74: return 0x60; // F5
        case 0x75: return 0x61; // F6
        case 0x76: return 0x62; // F7
        case 0x77: return 0x64; // F8
        case 0x78: return 0x65; // F9
        case 0x79: return 0x6D; // F10
        case 0x7A: return 0x67; // F11
        case 0x7B: return 0x6F; // F12
        default: return 0xFFFF;
    }
}

// Post a real Quartz flags-changed event for the macOS Caps Lock toggle.
// CGEventSourceKeyState alone does not reliably represent the latched state.
static void SimulateMacCapsLockToggle() {
    const CGEventFlags currentFlags = CGEventSourceFlagsState(
        kCGEventSourceStateCombinedSessionState
    );

    CGEventFlags nextFlags = currentFlags;

    if ((currentFlags & kCGEventFlagMaskAlphaShift) != 0) {
        nextFlags &= ~kCGEventFlagMaskAlphaShift;
    } else {
        nextFlags |= kCGEventFlagMaskAlphaShift;
    }

    CGEventRef event = CGEventCreate(NULL);

    if (!event) {
        return;
    }

    CGEventSetType(event, kCGEventFlagsChanged);
    CGEventSetIntegerValueField(event, kCGKeyboardEventKeycode, 0x39);
    CGEventSetFlags(event, nextFlags);
    CGEventPost(kCGHIDEventTap, event);
    CFRelease(event);
}

#endif

// =========================================================================
// LINUX KEYCODE MAPPER
// Windows virtual-key code -> X11 KeySym
// =========================================================================

#ifdef __linux__

static KeySym WinVKToX11Keysym(int vk) {
    if (vk >= 'A' && vk <= 'Z') {
        return XK_a + (vk - 'A');
    }

    if (vk >= '0' && vk <= '9') {
        return XK_0 + (vk - '0');
    }

    if (vk >= 0x70 && vk <= 0x7B) {
        return XK_F1 + (vk - 0x70);
    }

    switch (vk) {
        // Common keys
        case 0x08: return XK_BackSpace;
        case 0x09: return XK_Tab;
        case 0x0D: return XK_Return;
        case 0x1B: return XK_Escape;
        case 0x20: return XK_space;

        case 0x2D: return XK_Insert;
        case 0x2E: return XK_Delete;
        case 0x21: return XK_Page_Up;
        case 0x22: return XK_Page_Down;
        case 0x23: return XK_End;
        case 0x24: return XK_Home;

        // Generic modifiers
        case 0x10: return XK_Shift_L;
        case 0x11: return XK_Control_L;
        case 0x12: return XK_Alt_L;

        // Lock keys
        case 0x14: return XK_Caps_Lock;
        case 0x90: return XK_Num_Lock;
        case 0x91: return XK_Scroll_Lock;

        // Super / Windows keys
        case 0x5B: return XK_Super_L;
        case 0x5C: return XK_Super_R;

        // Left/right modifiers
        case 0xA0: return XK_Shift_L;
        case 0xA1: return XK_Shift_R;
        case 0xA2: return XK_Control_L;
        case 0xA3: return XK_Control_R;
        case 0xA4: return XK_Alt_L;
        case 0xA5: return XK_Alt_R;

        // Arrow keys
        case 0x25: return XK_Left;
        case 0x26: return XK_Up;
        case 0x27: return XK_Right;
        case 0x28: return XK_Down;

        // Punctuation
        case 0xBD: return XK_minus;
        case 0xBB: return XK_equal;
        case 0xDB: return XK_bracketleft;
        case 0xDD: return XK_bracketright;
        case 0xDC: return XK_backslash;
        case 0xBA: return XK_semicolon;
        case 0xDE: return XK_apostrophe;
        case 0xC0: return XK_grave;
        case 0xBC: return XK_comma;
        case 0xBE: return XK_period;
        case 0xBF: return XK_slash;

        default:
            return NoSymbol;
    }
}

#endif

// =========================================================================
// INPUT INJECTION
// =========================================================================

static void SimulateKeyCode(int vk, bool down) {
    if (vk < 0 || vk > 255) {
        return;
    }

#ifdef _WIN32
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = static_cast<WORD>(vk);

    if (!down) {
        input.ki.dwFlags = KEYEVENTF_KEYUP;
    }

    SendInput(1, &input, sizeof(INPUT));

#elif defined(__APPLE__)
    // macOS Caps Lock is a latched modifier. Treat the first key-down as a
    // toggle and ignore repeats until key-up; do not emit an ordinary key-up.
    if (vk == 0x14) {
        if (down) {
            if (!g_macCapsLockInjectionActive) {
                g_macCapsLockInjectionActive = true;
                SimulateMacCapsLockToggle();
            }
        } else {
            g_macCapsLockInjectionActive = false;
        }
        return;
    }

    const CGKeyCode macCode = WinVKToMacVK(vk);

    if (macCode != 0xFFFF) {
        CGEventRef event = CGEventCreateKeyboardEvent(
            NULL,
            macCode,
            down
        );

        if (event) {
            CGEventPost(kCGHIDEventTap, event);
            CFRelease(event);
        }
    }

#elif defined(__linux__)
    Display* display = XOpenDisplay(NULL);

    if (display) {
        const KeySym sym = WinVKToX11Keysym(vk);

        if (sym != NoSymbol) {
            const KeyCode keycode = XKeysymToKeycode(display, sym);

            if (keycode != 0) {
                XTestFakeKeyEvent(
                    display,
                    keycode,
                    down ? True : False,
                    CurrentTime
                );

                XFlush(display);
            }
        }

        XCloseDisplay(display);
    }
#else
    (void)vk;
    (void)down;
#endif
}

static int l_simulate_key(lua_State* L) {
    const int vk = ReadKeyArgument(L, 1, false);
    const bool down = lua_toboolean(L, 2);

    SimulateKeyCode(vk, down);
    return 0;
}

static int l_simulate_mouse_move(lua_State* L) {
    const float x = static_cast<float>(luaL_checknumber(L, 1));
    const float y = static_cast<float>(luaL_checknumber(L, 2));

    const bool absolute =
        lua_isboolean(L, 3) ? lua_toboolean(L, 3) : true;

#ifdef _WIN32
    INPUT input = {};
    input.type = INPUT_MOUSE;

    if (absolute) {
        const int screenW = GetSystemMetrics(SM_CXSCREEN);
        const int screenH = GetSystemMetrics(SM_CYSCREEN);

        if (screenW <= 0 || screenH <= 0) {
            return 0;
        }

        input.mi.dx = static_cast<LONG>((x * 65535.0f) / screenW);
        input.mi.dy = static_cast<LONG>((y * 65535.0f) / screenH);
        input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    } else {
        input.mi.dx = static_cast<LONG>(x);
        input.mi.dy = static_cast<LONG>(y);
        input.mi.dwFlags = MOUSEEVENTF_MOVE;
    }

    SendInput(1, &input, sizeof(INPUT));

#elif defined(__APPLE__)
    const CGPoint point = absolute
        ? CGPointMake(x, y)
        : CGPointMake(g_mouseX + x, g_mouseY + y);

    CGEventRef event = CGEventCreateMouseEvent(
        NULL,
        kCGEventMouseMoved,
        point,
        kCGMouseButtonLeft
    );

    if (event) {
        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);
    }

#elif defined(__linux__)
    Display* display = XOpenDisplay(NULL);

    if (display) {
        if (absolute) {
            XTestFakeMotionEvent(
                display,
                -1,
                static_cast<int>(x),
                static_cast<int>(y),
                CurrentTime
            );
        } else {
            XTestFakeRelativeMotionEvent(
                display,
                -1,
                static_cast<int>(x),
                static_cast<int>(y),
                CurrentTime
            );
        }

        XFlush(display);
        XCloseDisplay(display);
    }
#else
    (void)x;
    (void)y;
    (void)absolute;
#endif

    return 0;
}

static int l_toggle_lock_key(lua_State* L) {
    const int vk = ReadKeyArgument(L, 1, false);

    if (vk != 0x14 && vk != 0x90 && vk != 0x91) {
        return luaL_argerror(
            L,
            1,
            "expected capslock, numlock, or scrolllock"
        );
    }

    SimulateKeyCode(vk, true);
    SimulateKeyCode(vk, false);

    return 0;
}

// =========================================================================
// LUA READ BINDINGS
// =========================================================================

static int l_get_key_code(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    const int vk = KeyNameToVK(name);

    if (vk < 0) {
        return luaL_error(L, "unknown key name: %s", name);
    }

    lua_pushinteger(L, vk);
    return 1;
}

static int l_is_key_down(lua_State* L) {
    const int key = ReadKeyArgument(L, 1, true);

    const bool isDown =
        key >= 0 && key < 512
            ? g_currKeys[key]
            : false;

    lua_pushboolean(L, isDown);
    return 1;
}

static int l_is_key_pressed(lua_State* L) {
    const int key = ReadKeyArgument(L, 1, true);

    const bool pressed =
        key >= 0 && key < 512
            ? g_currKeys[key] && !g_prevKeys[key]
            : false;

    lua_pushboolean(L, pressed);
    return 1;
}

static int l_is_key_released(lua_State* L) {
    const int key = ReadKeyArgument(L, 1, true);

    const bool released =
        key >= 0 && key < 512
            ? !g_currKeys[key] && g_prevKeys[key]
            : false;

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

// =========================================================================
// INPUT POLLING
// =========================================================================

static int l_update(lua_State* L) {
    (void)L;

    // Snapshot the previous frame for pressed/released queries.
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
        HWND foreground = GetForegroundWindow();
        DWORD foregroundPid = 0;

        if (foreground) {
            GetWindowThreadProcessId(foreground, &foregroundPid);
        }

        shouldPoll = foregroundPid == GetCurrentProcessId();
    }

    if (shouldPoll) {
        // Covers letters, digits, function keys, OEM punctuation,
        // navigation keys, and sided modifier virtual-key codes.
        for (int vk = 0; vk < 256; ++vk) {
            g_currKeys[vk] =
                (GetAsyncKeyState(vk) & 0x8000) != 0;
        }

        // Retain the existing mouse index convention.
        g_currKeys[1] =
            (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

        g_currKeys[2] =
            (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

        g_currKeys[3] =
            (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;

        POINT point;

        if (GetCursorPos(&point)) {
            g_mouseDeltaX = static_cast<float>(point.x) - g_mouseX;
            g_mouseDeltaY = static_cast<float>(point.y) - g_mouseY;

            g_mouseX = static_cast<float>(point.x);
            g_mouseY = static_cast<float>(point.y);
        }
    } else {
        for (int i = 0; i < 512; ++i) {
            g_currKeys[i] = false;
        }
    }

// -------------------------------------------------------------------------
// MACOS POLLING
// -------------------------------------------------------------------------

#elif defined(__APPLE__)
    bool shouldPoll = g_globalInput;

    if (!shouldPoll) {
        id workspaceClass = (id)objc_getClass("NSWorkspace");

        if (workspaceClass) {
            SEL sharedWorkspaceSel =
                sel_registerName("sharedWorkspace");

            id workspace =
                ((id (*)(id, SEL))objc_msgSend)(
                    workspaceClass,
                    sharedWorkspaceSel
                );

            if (workspace) {
                SEL frontmostAppSel =
                    sel_registerName("frontmostApplication");

                id frontmostApp =
                    ((id (*)(id, SEL))objc_msgSend)(
                        workspace,
                        frontmostAppSel
                    );

                if (frontmostApp) {
                    SEL pidSel = sel_registerName("processIdentifier");

                    pid_t frontPid =
                        ((pid_t (*)(id, SEL))objc_msgSend)(
                            frontmostApp,
                            pidSel
                        );

                    const pid_t myPid = getpid();
                    const pid_t parentPid = getppid();

                    shouldPoll =
                        frontPid == myPid || frontPid == parentPid;
                }
            }
        }
    }

    if (shouldPoll) {
        for (int vk = 0; vk < 256; ++vk) {
            const CGKeyCode macCode = WinVKToMacVK(vk);

            if (macCode != 0xFFFF) {
                g_currKeys[vk] = CGEventSourceKeyState(
                    kCGEventSourceStateHIDSystemState,
                    macCode
                );
            }
        }

        // IsKeyDown/Pressed/Released represent the physical key event state,
        // not whether Caps Lock's latched mode is enabled.
        g_currKeys[0x14] = CGEventSourceKeyState(
            kCGEventSourceStateHIDSystemState,
            0x39
        );

        // Retain existing mouse indices.
        g_currKeys[1] = CGEventSourceButtonState(
            kCGEventSourceStateHIDSystemState,
            kCGMouseButtonLeft
        );

        g_currKeys[2] = CGEventSourceButtonState(
            kCGEventSourceStateHIDSystemState,
            kCGMouseButtonRight
        );

        g_currKeys[3] = CGEventSourceButtonState(
            kCGEventSourceStateHIDSystemState,
            kCGMouseButtonCenter
        );

        CGEventRef event = CGEventCreate(NULL);

        if (event) {
            const CGPoint point = CGEventGetLocation(event);
            CFRelease(event);

            g_mouseDeltaX = static_cast<float>(point.x) - g_mouseX;
            g_mouseDeltaY = static_cast<float>(point.y) - g_mouseY;

            g_mouseX = static_cast<float>(point.x);
            g_mouseY = static_cast<float>(point.y);
        }
    } else {
        for (int i = 0; i < 512; ++i) {
            g_currKeys[i] = false;
        }
    }

// -------------------------------------------------------------------------
// LINUX (X11) POLLING
// -------------------------------------------------------------------------

#elif defined(__linux__)
    Display* display = XOpenDisplay(NULL);

    if (display) {
        const Window root = DefaultRootWindow(display);
        bool shouldPoll = g_globalInput;

        if (!shouldPoll) {
            Window focusedWindow;
            int revertTo;

            XGetInputFocus(display, &focusedWindow, &revertTo);

            shouldPoll = focusedWindow != None;
        }

        if (shouldPoll) {
            char keymap[32] = {};
            XQueryKeymap(display, keymap);

            for (int vk = 0; vk < 256; ++vk) {
                const KeySym sym = WinVKToX11Keysym(vk);

                if (sym != NoSymbol) {
                    const KeyCode kc = XKeysymToKeycode(display, sym);

                    if (kc != 0) {
                        g_currKeys[vk] =
                            (keymap[kc / 8] & (1 << (kc % 8))) != 0;
                    }
                }
            }

            Window rootReturn;
            Window childReturn;
            int rootX;
            int rootY;
            int winX;
            int winY;
            unsigned int mask;

            if (XQueryPointer(
                    display,
                    root,
                    &rootReturn,
                    &childReturn,
                    &rootX,
                    &rootY,
                    &winX,
                    &winY,
                    &mask
                )) {
                g_currKeys[1] = (mask & Button1Mask) != 0;
                g_currKeys[2] = (mask & Button3Mask) != 0;
                g_currKeys[3] = (mask & Button2Mask) != 0;

                g_mouseDeltaX = static_cast<float>(rootX) - g_mouseX;
                g_mouseDeltaY = static_cast<float>(rootY) - g_mouseY;

                g_mouseX = static_cast<float>(rootX);
                g_mouseY = static_cast<float>(rootY);
            }
        } else {
            for (int i = 0; i < 512; ++i) {
                g_currKeys[i] = false;
            }
        }

        XCloseDisplay(display);
    } else {
        for (int i = 0; i < 512; ++i) {
            g_currKeys[i] = false;
        }
    }
#endif

    // Generic modifier names mean either physical side on every platform.
    // Windows already reports these aggregate VK states, while macOS/X11
    // resolve the generic virtual keys to the left-side key only.
    g_currKeys[0x10] = g_currKeys[0x10] || g_currKeys[0xA0] || g_currKeys[0xA1];
    g_currKeys[0x11] = g_currKeys[0x11] || g_currKeys[0xA2] || g_currKeys[0xA3];
    g_currKeys[0x12] = g_currKeys[0x12] || g_currKeys[0xA4] || g_currKeys[0xA5];

    return 0;
}

// =========================================================================
// LUA MODULE EXPORTS
// =========================================================================

extern "C" INPUT_EXPORT int luaopen_input_native(lua_State* L) {
    static const luaL_Reg funcs[] = {
        {"get_key_code", l_get_key_code},

        {"is_key_down", l_is_key_down},
        {"is_key_pressed", l_is_key_pressed},
        {"is_key_released", l_is_key_released},

        {"get_mouse_position", l_get_mouse_position},
        {"get_mouse_delta", l_get_mouse_delta},

        {"set_global_input", l_set_global_input},
        {"update", l_update},

        {"simulate_key", l_simulate_key},
        {"simulate_mouse_move", l_simulate_mouse_move},
        {"toggle_lock_key", l_toggle_lock_key},

        {NULL, NULL}
    };

    luaL_newlib(L, funcs);
    return 1;
}
