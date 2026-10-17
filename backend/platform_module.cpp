#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

// Lua Header Include
extern "C" {
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
};

// =====================================================================
// EXPORT & PLATFORM DEFINITIONS
// =====================================================================
#if defined(_WIN32) || defined(_WIN64)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif

    #define EXPORT_FN __declspec(dllexport)
    #include <windows.h>
    #include <sysinfoapi.h>
#elif defined(__APPLE__)
    #define EXPORT_FN __attribute__((visibility("default")))
    #include <objc/runtime.h>
    #include <objc/message.h>
    #include <sys/sysctl.h>
    #include <TargetConditionals.h>
#else
    #define EXPORT_FN __attribute__((visibility("default")))
    #include <unistd.h>
    #include <sys/sysinfo.h>
    #include <dlfcn.h>
#endif

struct PlatformSystemInfo {
    char osName[64] = "Unknown";
    char cpuArchitecture[32] = "Unknown";
    uint32_t logicalCores = 0;
    uint64_t totalRamMB = 0;
    uint64_t availableRamMB = 0;
    bool is64Bit = false;
    char endianness[8] = "little";
};

struct PlatformWindowInfo {
    int notchHeight = 0;
    int borderLeft = 0;
    int borderTop = 0;
    int borderRight = 0;
    int borderBottom = 0;
    int titleBarHeight = 0;
};

// =====================================================================
// SYSTEM & WINDOW METRICS IMPLEMENTATION
// =====================================================================

static PlatformSystemInfo GatherSystemInformation() {
    PlatformSystemInfo info{};

    union {
        uint32_t val;
        uint8_t bytes[4];
    } endianTest = {0x01020304};
    std::strncpy(info.endianness, (endianTest.bytes[0] == 1) ? "big" : "little", sizeof(info.endianness));

#if defined(_WIN32) || defined(_WIN64)
    std::strncpy(info.osName, "Windows", sizeof(info.osName));
    SYSTEM_INFO sysInfo;
    GetNativeSystemInfo(&sysInfo);

    info.logicalCores = sysInfo.dwNumberOfProcessors;

    switch (sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            std::strncpy(info.cpuArchitecture, "x86_64", sizeof(info.cpuArchitecture));
            info.is64Bit = true;
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            std::strncpy(info.cpuArchitecture, "ARM64", sizeof(info.cpuArchitecture));
            info.is64Bit = true;
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
            std::strncpy(info.cpuArchitecture, "x86", sizeof(info.cpuArchitecture));
            info.is64Bit = false;
            break;
        case PROCESSOR_ARCHITECTURE_ARM:
            std::strncpy(info.cpuArchitecture, "ARM", sizeof(info.cpuArchitecture));
            info.is64Bit = false;
            break;
        default:
            std::strncpy(info.cpuArchitecture, "Unknown", sizeof(info.cpuArchitecture));
            break;
    }

    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        info.totalRamMB = memStatus.ullTotalPhys / (1024 * 1024);
        info.availableRamMB = memStatus.ullAvailPhys / (1024 * 1024);
    }

#elif defined(__APPLE__)
    #if TARGET_OS_IPHONE
        std::strncpy(info.osName, "iOS", sizeof(info.osName));
    #else
        std::strncpy(info.osName, "macOS", sizeof(info.osName));
    #endif

    uint32_t cores = 0;
    size_t size = sizeof(cores);
    if (sysctlbyname("hw.logicalcpu", &cores, &size, NULL, 0) == 0) {
        info.logicalCores = cores;
    }

    uint64_t mem = 0;
    size = sizeof(mem);
    if (sysctlbyname("hw.memsize", &mem, &size, NULL, 0) == 0) {
        info.totalRamMB = mem / (1024 * 1024);
    }

#if defined(__x86_64__)
    std::strncpy(info.cpuArchitecture, "x86_64", sizeof(info.cpuArchitecture));
    info.is64Bit = true;
#elif defined(__arm64__) || defined(__aarch64__)
    std::strncpy(info.cpuArchitecture, "ARM64", sizeof(info.cpuArchitecture));
    info.is64Bit = true;
#endif

#else // Linux / POSIX
    std::strncpy(info.osName, "Linux/Unix", sizeof(info.osName));
    info.logicalCores = sysconf(_SC_NPROCESSORS_ONLN);

    struct sysinfo si{};
    if (sysinfo(&si) == 0) {
        info.totalRamMB = (si.totalram * si.mem_unit) / (1024 * 1024);
        info.availableRamMB = (si.freeram * si.mem_unit) / (1024 * 1024);
    }

#if defined(__x86_64__)
    std::strncpy(info.cpuArchitecture, "x86_64", sizeof(info.cpuArchitecture));
    info.is64Bit = true;
#elif defined(__i386__)
    std::strncpy(info.cpuArchitecture, "x86", sizeof(info.cpuArchitecture));
    info.is64Bit = false;
#elif defined(__aarch64__)
    std::strncpy(info.cpuArchitecture, "ARM64", sizeof(info.cpuArchitecture));
    info.is64Bit = true;
#elif defined(__arm__)
    std::strncpy(info.cpuArchitecture, "ARM", sizeof(info.cpuArchitecture));
    info.is64Bit = false;
#endif

#endif

    return info;
}

static PlatformWindowInfo GatherWindowInformation() {
    PlatformWindowInfo winInfo{};

#if defined(_WIN32) || defined(_WIN64)
    int cxFrame = GetSystemMetrics(SM_CXSIZEFRAME);
    int cyFrame = GetSystemMetrics(SM_CYSIZEFRAME);
    int caption = GetSystemMetrics(SM_CYCAPTION);

    winInfo.borderLeft = (cxFrame > 0) ? cxFrame : 8;
    winInfo.borderRight = winInfo.borderLeft;
    winInfo.borderBottom = (cyFrame > 0) ? cyFrame : 8;
    winInfo.titleBarHeight = (caption > 0) ? caption : 30;
    winInfo.borderTop = winInfo.titleBarHeight + winInfo.borderBottom;
    winInfo.notchHeight = 0; // Windows desktops don't feature hardware camera notches

#elif defined(__APPLE__)
    Class clsNSScreen = objc_getClass("NSScreen");
    if (clsNSScreen) {
        SEL mainScreenSel = sel_getUid("mainScreen");
        id mainScreen = ((id(*)(id, SEL))objc_msgSend)((id)clsNSScreen, mainScreenSel);
        if (mainScreen) {
            SEL safeAreaSel = sel_getUid("safeAreaInsets");
            struct NSEdgeInsets { double top; double left; double bottom; double right; } insets = {0, 0, 0, 0};
            if (((bool(*)(id, SEL))objc_msgSend)(mainScreen, sel_getUid("respondsToSelector:"), safeAreaSel)) {
                insets = ((struct NSEdgeInsets(*)(id, SEL))objc_msgSend)(mainScreen, safeAreaSel);
            }
            winInfo.notchHeight = static_cast<int>(insets.top);
        }
    }

    winInfo.borderLeft = 0;
    winInfo.borderRight = 0;
    winInfo.borderBottom = 0;
    winInfo.titleBarHeight = 28;
    winInfo.borderTop = winInfo.titleBarHeight;

#else
    winInfo.notchHeight = 0;
    winInfo.borderLeft = 4;
    winInfo.borderRight = 4;
    winInfo.borderBottom = 4;
    winInfo.titleBarHeight = 26;
    winInfo.borderTop = 30;
#endif

    return winInfo;
}

// =====================================================================
// LUA BINDINGS
// =====================================================================

static int platform_get_os(lua_State* L) {
    lua_pushstring(L, GatherSystemInformation().osName);
    return 1;
}

static int platform_get_arch(lua_State* L) {
    lua_pushstring(L, GatherSystemInformation().cpuArchitecture);
    return 1;
}

static int platform_get_cpu_cores(lua_State* L) {
    lua_pushinteger(L, GatherSystemInformation().logicalCores);
    return 1;
}

static int platform_get_memory_info(lua_State* L) {
    PlatformSystemInfo info = GatherSystemInformation();
    lua_newtable(L);
    lua_pushinteger(L, static_cast<lua_Integer>(info.totalRamMB));
    lua_setfield(L, -2, "total_mb");
    lua_pushinteger(L, static_cast<lua_Integer>(info.availableRamMB));
    lua_setfield(L, -2, "available_mb");
    return 1;
}

static int platform_get_notch_height(lua_State* L) {
    lua_pushinteger(L, GatherWindowInformation().notchHeight);
    return 1;
}

static int platform_get_title_bar_height(lua_State* L) {
    lua_pushinteger(L, GatherWindowInformation().titleBarHeight);
    return 1;
}

static int platform_get_window_metrics(lua_State* L) {
    PlatformWindowInfo winInfo = GatherWindowInformation();

    lua_newtable(L);

    lua_pushinteger(L, winInfo.notchHeight);
    lua_setfield(L, -2, "notch_height");

    lua_pushinteger(L, winInfo.titleBarHeight);
    lua_setfield(L, -2, "title_bar_height");

    lua_pushinteger(L, winInfo.borderLeft);
    lua_setfield(L, -2, "border_left");

    lua_pushinteger(L, winInfo.borderTop);
    lua_setfield(L, -2, "border_top");

    lua_pushinteger(L, winInfo.borderRight);
    lua_setfield(L, -2, "border_right");

    lua_pushinteger(L, winInfo.borderBottom);
    lua_setfield(L, -2, "border_bottom");

    return 1;
}

static int platform_get_info(lua_State* L) {
    PlatformSystemInfo info = GatherSystemInformation();
    PlatformWindowInfo winInfo = GatherWindowInformation();

    lua_newtable(L);

    lua_pushstring(L, info.osName);
    lua_setfield(L, -2, "os");

    lua_pushstring(L, info.cpuArchitecture);
    lua_setfield(L, -2, "arch");

    lua_pushinteger(L, info.logicalCores);
    lua_setfield(L, -2, "cpu_cores");

    lua_pushinteger(L, static_cast<lua_Integer>(info.totalRamMB));
    lua_setfield(L, -2, "ram_total_mb");

    lua_pushinteger(L, static_cast<lua_Integer>(info.availableRamMB));
    lua_setfield(L, -2, "ram_available_mb");

    lua_pushboolean(L, info.is64Bit);
    lua_setfield(L, -2, "is_64bit");

    lua_pushstring(L, info.endianness);
    lua_setfield(L, -2, "endianness");

    lua_pushinteger(L, winInfo.notchHeight);
    lua_setfield(L, -2, "notch_height");

    lua_pushinteger(L, winInfo.titleBarHeight);
    lua_setfield(L, -2, "title_bar_height");

    return 1;
}

// =====================================================================
// MODULE EXPORT REGISTRATION
// =====================================================================

extern "C" EXPORT_FN int luaopen_platform_module(lua_State* L) {
    static const luaL_Reg PlatformManagement[] = {
        {"get_os", platform_get_os},
        {"get_arch", platform_get_arch},
        {"get_cpu_cores", platform_get_cpu_cores},
        {"get_memory_info", platform_get_memory_info},
        {"get_notch_height", platform_get_notch_height},
        {"get_title_bar_height", platform_get_title_bar_height},
        {"get_window_metrics", platform_get_window_metrics},
        {"get_info", platform_get_info},
        {nullptr, nullptr}
    };

    luaL_newlib(L, PlatformManagement);
    return 1;
}
