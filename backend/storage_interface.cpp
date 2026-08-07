#include <filesystem>
#include <fstream>
#include <chrono>
#include <lua.hpp>
#include <string>
#include <iostream>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
#else
    #include <unistd.h>
#endif

using namespace std;
using namespace filesystem;

// =========================================================================
// INITIAL DIRECTORY LOCK-IN
// =========================================================================
static const path INITIAL_CWD = current_path();

static path get_exe_dir() {
#if defined(_WIN32) || defined(_WIN64)
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    return path(buffer).parent_path();
#elif defined(__APPLE__)
    char buffer[1024];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) == 0) {
        return path(buffer).parent_path();
    }
    return INITIAL_CWD;
#else
    char buffer[1024];
    ssize_t count = readlink("/proc/self/exe", buffer, sizeof(buffer));
    if (count != -1) {
        return path(string(buffer, count)).parent_path();
    }
    return INITIAL_CWD;
#endif
}

static path resolve_path(const string& input_path) {
    path p(input_path); if (p.is_absolute()) {
        return p.lexically_normal();
    }; return (INITIAL_CWD / p).lexically_normal();
}

static void push_file_properties(lua_State* L, const path& filePath, bool includeContents) {
    lua_newtable(L);

    lua_pushstring(L, filePath.filename().string().c_str());
    lua_setfield(L, -2, "name");

    lua_pushstring(L, filePath.extension().string().c_str());
    lua_setfield(L, -2, "extension");

    lua_pushstring(L, filePath.string().c_str());
    lua_setfield(L, -2, "path");

    bool isDir = is_directory(filePath);
    lua_pushboolean(L, isDir ? 1 : 0);
    lua_setfield(L, -2, "is_directory");
    
    if (exists(filePath)) {
        if (!isDir) {
            std::error_code ec;
            auto sz = file_size(filePath, ec);
            lua_pushinteger(L, ec ? 0 : static_cast<lua_Integer>(sz));
        } else {
            lua_pushinteger(L, 0);
        }
        lua_setfield(L, -2, "size");

        std::error_code ec;
        auto ftime = last_write_time(filePath, ec);
        if (!ec) {
            auto sys_time = chrono::time_point_cast<chrono::seconds>(ftime - file_time_type::clock::now() + chrono::system_clock::now());
            lua_pushinteger(L, static_cast<lua_Integer>(sys_time.time_since_epoch().count()));
        } else {
            lua_pushinteger(L, 0);
        }
        lua_setfield(L, -2, "modified_time");
    } else {
        lua_pushinteger(L, 0);
        lua_setfield(L, -2, "size");
        lua_pushinteger(L, 0);
        lua_setfield(L, -2, "modified_time");
    }

    if (includeContents && !isDir && exists(filePath)) {
        std::ifstream file(filePath, std::ios::binary);
        if (file) {
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            lua_pushlstring(L, content.c_str(), content.size());
        } else {
            lua_pushstring(L, "");
        }
    } else {
        lua_pushstring(L, "");
    }
    lua_setfield(L, -2, "contents");
}

// =========================================================================
// NATIVE BINDINGS
// =========================================================================

static int storage_get_executable_dir(lua_State* L) {
    lua_pushstring(L, get_exe_dir().string().c_str());
    return 1;
}

static int storage_get_file_info(lua_State* L) {
    const char* filepath = luaL_checkstring(L, 1);
    bool readContents = lua_toboolean(L, 2);
    push_file_properties(L, resolve_path(filepath), readContents);
    return 1;
}

static int storage_list_dir(lua_State* L) {
    const char* path_str = luaL_optstring(L, 1, ".");
    lua_newtable(L);
    int index = 1;

    try {
        path p = resolve_path(path_str);
        if (!exists(p) || !is_directory(p)) {
            return luaL_error(L, "Path does not exist or is not a directory: %s", path_str);
        }

        for (const auto& entry : directory_iterator(p)) {
            push_file_properties(L, entry.path(), false);
            lua_rawseti(L, -2, index++);
        }
    } catch (const std::exception& e) {
        return luaL_error(L, "Failed to list directory: %s", e.what());
    }

    return 1;
}

static int storage_read_file(lua_State* L) {
    const char* filepath = luaL_checkstring(L, 1);
    path fp = resolve_path(filepath);
    std::ifstream file(fp, std::ios::binary);
    if (!file) {
        lua_pushnil(L);
        lua_pushstring(L, "File not found or cannot be opened.");
        return 2;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    lua_pushlstring(L, content.c_str(), content.size());
    return 1;
}

static int storage_write_file(lua_State* L) {
    const char* filepath = luaL_checkstring(L, 1);
    size_t len;
    const char* content = luaL_checklstring(L, 2, &len);

    try {
        path fp = resolve_path(filepath);
        // std::cout << "[C++ Debug] Writing file target: " << fp.string() << std::endl;

        if (fp.has_parent_path() && !fp.parent_path().empty()) {
            std::error_code ec;
            create_directories(fp.parent_path(), ec);
            if (ec) {
                lua_pushboolean(L, 0);
                lua_pushstring(L, ("Failed to create parent directories: " + ec.message()).c_str());
                return 2;
            }
        }

        std::ofstream file(fp, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            lua_pushboolean(L, 0);
            lua_pushstring(L, "Failed to open file stream for writing.");
            return 2;
        }

        file.write(content, len);
        file.close();

        lua_pushboolean(L, 1);
        return 1;
    } catch (const std::exception& e) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, e.what());
        return 2;
    }
}

static int storage_create_dir(lua_State* L) {
    const char* path_str = luaL_checkstring(L, 1);
    try {
        create_directories(resolve_path(path_str));
        lua_pushboolean(L, 1);
        return 1;
    } catch (const std::exception& e) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, e.what());
        return 2;
    }
}

static int storage_remove_file(lua_State* L) {
    const char* filepath = luaL_checkstring(L, 1);
    try {
        std::error_code ec;
        bool deleted = remove(resolve_path(filepath), ec);
        if (ec) {
            lua_pushboolean(L, 0);
            lua_pushstring(L, ec.message().c_str());
            return 2;
        }
        lua_pushboolean(L, deleted ? 1 : 0);
        return 1;
    } catch (const std::exception& e) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, e.what());
        return 2;
    }
}

static int storage_remove_dir(lua_State* L) {
    const char* path_str = luaL_checkstring(L, 1);
    try {
        std::error_code ec;
        auto count = remove_all(resolve_path(path_str), ec);
        if (ec) {
            lua_pushboolean(L, 0);
            lua_pushstring(L, ec.message().c_str());
            return 2;
        }
        lua_pushboolean(L, count > 0 ? 1 : 0);
        lua_pushinteger(L, static_cast<lua_Integer>(count));
        return 2;
    } catch (const std::exception& e) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, e.what());
        return 2;
    }
}

// =========================================================================
// DLL EXPORT DEFINITIONS
// =========================================================================

#if defined(_WIN32) || defined(_WIN64)
    #define EXPORT_FN __declspec(dllexport)
#else
    #define EXPORT_FN
#endif

// =========================================================================
// LUA MODULE ENTRY POINT
// =========================================================================

extern "C" EXPORT_FN int luaopen_storage_interface(lua_State* L) {
    static const luaL_Reg StorageInterface[] = {
        {"get_executable_dir", storage_get_executable_dir},
        {"get_file_info", storage_get_file_info},
        {"list_dir", storage_list_dir},
        {"read_file", storage_read_file},
        {"write_file", storage_write_file},
        {"create_dir", storage_create_dir},
        {"remove_file", storage_remove_file},
        {"remove_dir", storage_remove_dir},
        {nullptr, nullptr}
    };
    
    luaL_newlib(L, StorageInterface);
    return 1;
}
