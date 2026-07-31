#include <filesystem>
// #include <lua.hpp>
#include <string>
#include <regex>

using namespace std;

static filesystem::path l_GetFilePath() {
    const regex FilePattern = regex("[^(/|\\)]+", "gm");
    // cout << "PATH:\t" << FileContext::l_GetFilePath() << endl;
    return (filesystem::path(string("./")));
}

#if defined(_WIN32) || defined(_WIN64)
    #define EXPORT_FN __declspec(dllexport)
#else
    #define EXPORT_FN
#endif

// extern "C" EXPORT_FN int luaopen_sound_native(lua_State* L) {
//     static const luaL_Reg StorageInterface[] = {
//         {nullptr, nullptr},
//     };

//     #if LUA_VERSION_NUM >= 502
//         luaL_newlib(L, StorageInterface);
//     #else
//         luaL_register(L, "sound_native", StorageInterface);
//     #endif

//     return 1;
// }
