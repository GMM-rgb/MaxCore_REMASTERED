// =========================================================================
// sleep_native.cpp
// =========================================================================
// Gives Lua an OS-level sleep so the main loop can idle without burning a
// core. Pure Lua has no portable sleep, which is why a "while os.clock()
// - t < n do end" loop pegs the CPU and starves everything else.
//
// Exposes (as module "sleep_native"):
//   sleep(seconds)        blocks THIS OS thread for about `seconds`
//   monotonic() -> number seconds from a monotonic clock (never goes backwards,
//                         unaffected by system clock changes; os.clock() is CPU
//                         time and os.time() is whole seconds, neither is a
//                         wall clock you can schedule against)
//
// No link-time dependencies beyond libc / kernel32.
// =========================================================================
#include <lua.hpp>
#include <chrono>
#include <thread>

#if defined(_WIN32) || defined(_WIN64)
    #define EXPORT_FN __declspec(dllexport)
    #include <windows.h>
#else
    #define EXPORT_FN
#endif

static int l_sleep(lua_State* L) {
    double seconds = luaL_checknumber(L, 1);
    // NaN, negative, and zero all mean "don't sleep". Cap absurd values so a
    // bad argument can't freeze the process for hours.
    if (!(seconds > 0.0)) return 0;
    if (seconds > 60.0) seconds = 60.0;

#if defined(_WIN32) || defined(_WIN64)
    // Default Windows timer granularity is ~15.6ms, which would make a
    // 1ms request take 15ms. Ask for 1ms resolution just for this call.
    timeBeginPeriod(1);
    Sleep((DWORD)(seconds * 1000.0 + 0.5));
    timeEndPeriod(1);
#else
    std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
#endif
    return 0;
}

static int l_monotonic(lua_State* L) {
    using clock = std::chrono::steady_clock;
    static const clock::time_point origin = clock::now();
    lua_pushnumber(L, std::chrono::duration<double>(clock::now() - origin).count());
    return 1;
}

extern "C" EXPORT_FN int luaopen_sleep_native(lua_State* L) {
    static const luaL_Reg funcs[] = {
        {"sleep", l_sleep},
        {"monotonic", l_monotonic},
        {nullptr, nullptr}
    };
    luaL_newlib(L, funcs);
    return 1;
}
