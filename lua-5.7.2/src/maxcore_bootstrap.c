/*
** $Id: maxcore_bootstrap.c $
** MaxCore Game Engine bootstrapper (initializer)
** See Copyright Notice in max_core.lua
*/

#include "maxcore_bootstrap.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <direct.h>

#define MAXCORE_MKDIR(path) _mkdir(path)
#define MAXCORE_PATH_SEPARATOR '\\'
#define MAXCORE_PATH_LIST_SEPARATOR ';'

#else

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>

#define MAXCORE_MKDIR(path) mkdir(path, 0775)
#define MAXCORE_PATH_SEPARATOR '/'
#define MAXCORE_PATH_LIST_SEPARATOR ';'

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#endif

/*
** ---------------------------------------------------------------------------
** Configuration
** ---------------------------------------------------------------------------
*/

/*
** Maximum number of parent directories searched when locating max_core.lua.
**
** This allows the executable to reside in a build directory while the
** project root remains in one of its parent directories.
*/
#ifndef MAXCORE_ROOT_SEARCH_DEPTH
#define MAXCORE_ROOT_SEARCH_DEPTH 6
#endif

/*
** Optional fallback project root.
**
** Define this to an absolute path in your build configuration if you want
** a known development installation as a fallback.
**
** Example:
**   -DMAXCORE_DEFAULT_ROOT=\"C:/Projects/MaxCore_REMASTERED\"
**
** The default empty string disables this fallback.
*/
#ifndef MAXCORE_DEFAULT_ROOT
#define MAXCORE_DEFAULT_ROOT ""
#endif

/*
** Optional embedded-module registration.
**
** When MAXCORE_EMBEDDED_MODULES is defined, the build must provide:
**
**   void MaxCore_RegisterEmbeddedModules(lua_State *L);
**
** That function should register compiled Lua module loaders in
** package.preload, allowing require() to resolve embedded modules.
*/
#ifdef MAXCORE_EMBEDDED_MODULES
extern void MaxCore_RegisterEmbeddedModules(lua_State *L);
#endif

#define MAXCORE_PATH_BUFFER (PATH_MAX * 4)
#define MAXCORE_META_MAX_DEPTH 8
#define MAXCORE_META_MAX_ENTRIES 4096

/*
** ---------------------------------------------------------------------------
** Experimental LuaLS metadata generation
** ---------------------------------------------------------------------------
*/

#ifdef MAXCORE_EXPERIMENTAL_METADATA

static const void *visited_tables[MAXCORE_META_MAX_ENTRIES];
static size_t visited_count;
static size_t emitted_count;

static int is_identifier(const char *value) {
    const unsigned char *p = (const unsigned char *)value;

    if (p == NULL || *p == '\0' ||
        !(isalpha(*p) || *p == '_')) {
        return 0;
    }

    for (++p; *p != '\0'; ++p) {
        if (!(isalnum(*p) || *p == '_')) {
            return 0;
        }
    }

    return 1;
}

static int was_visited(const void *pointer) {
    size_t i;

    for (i = 0; i < visited_count; ++i) {
        if (visited_tables[i] == pointer) {
            return 1;
        }
    }

    if (visited_count < MAXCORE_META_MAX_ENTRIES) {
        visited_tables[visited_count++] = pointer;
    }

    return 0;
}

static void indent(FILE *file, int depth) {
    int i;

    for (i = 0; i < depth; ++i) {
        fputs("    ", file);
    }
}

static void emit_table_fields(
    lua_State *L,
    FILE *file,
    int table_index,
    const char *path,
    int depth
) {
    int absolute_index = lua_absindex(L, table_index);

    if (depth > MAXCORE_META_MAX_DEPTH ||
        emitted_count >= MAXCORE_META_MAX_ENTRIES) {
        return;
    }

    if (was_visited(lua_topointer(L, absolute_index))) {
        return;
    }

    lua_pushnil(L);

    while (lua_next(L, absolute_index) != 0) {
        const char *key = NULL;
        int key_type = lua_type(L, -2);
        int value_type = lua_type(L, -1);
        char child_path[2048];

        if (key_type == LUA_TSTRING) {
            key = lua_tostring(L, -2);
        }

        if (key != NULL &&
            is_identifier(key) &&
            emitted_count < MAXCORE_META_MAX_ENTRIES) {

            int written = snprintf(
                child_path,
                sizeof(child_path),
                "%s.%s",
                path,
                key
            );

            if (written > 0 &&
                (size_t)written < sizeof(child_path)) {

                ++emitted_count;

                switch (value_type) {
                    case LUA_TFUNCTION:
                        indent(file, depth);
                        fputs(
                            "---@type fun(...: any): any\n",
                            file
                        );
                        indent(file, depth);
                        fprintf(
                            file,
                            "%s = function(...) end\n\n",
                            child_path
                        );
                        break;

                    case LUA_TTABLE:
                        indent(file, depth);
                        fputs("---@type table\n", file);
                        indent(file, depth);
                        fprintf(file, "%s = {}\n", child_path);

                        emit_table_fields(
                            L,
                            file,
                            -1,
                            child_path,
                            depth + 1
                        );

                        fputc('\n', file);
                        break;

                    case LUA_TBOOLEAN:
                        indent(file, depth);
                        fputs("---@type boolean\n", file);
                        indent(file, depth);
                        fprintf(file, "%s = false\n\n", child_path);
                        break;

                    case LUA_TNUMBER:
                        indent(file, depth);
                        fputs("---@type number\n", file);
                        indent(file, depth);
                        fprintf(file, "%s = 0\n\n", child_path);
                        break;

                    case LUA_TSTRING:
                        indent(file, depth);
                        fputs("---@type string\n", file);
                        indent(file, depth);
                        fprintf(file, "%s = \"\"\n\n", child_path);
                        break;

                    default:
                        indent(file, depth);
                        fputs("---@type any\n", file);
                        indent(file, depth);
                        fprintf(file, "%s = nil\n\n", child_path);
                        break;
                }
            }
        }

        /*
        ** Remove the value while retaining the key for lua_next().
        */
        lua_pop(L, 1);
    }
}

static int ensure_directory(const char *path) {
    if (MAXCORE_MKDIR(path) == 0) {
        return 1;
    }

    if (errno == EEXIST) {
        struct stat info;

        if (stat(path, &info) == 0) {
            return
#ifdef _WIN32
                (info.st_mode & _S_IFDIR) != 0;
#else
                S_ISDIR(info.st_mode);
#endif
        }
    }

    return 0;
}

static int generate_engine_meta(lua_State *L, const char *root) {
    char lua_dir[PATH_MAX];
    char bin_dir[PATH_MAX];
    char meta_path[PATH_MAX];

    FILE *file;
    int written;

    written = snprintf(
        lua_dir,
        sizeof(lua_dir),
        "%s%clua-5.7.2",
        root,
        MAXCORE_PATH_SEPARATOR
    );

    if (written < 0 || (size_t)written >= sizeof(lua_dir)) {
        return luaL_error(
            L,
            "MaxCore: Metadata directory path is too long"
        );
    }

    written = snprintf(
        bin_dir,
        sizeof(bin_dir),
        "%s%cbin",
        lua_dir,
        MAXCORE_PATH_SEPARATOR
    );

    if (written < 0 || (size_t)written >= sizeof(bin_dir)) {
        return luaL_error(
            L,
            "MaxCore: Metadata output directory path is too long"
        );
    }

    /*
    ** The lua-5.7.2 parent directory must already exist.
    ** Create the bin directory if necessary.
    */
    if (!ensure_directory(bin_dir)) {
        return luaL_error(
            L,
            "MaxCore: Could not create metadata directory: %s",
            bin_dir
        );
    }

    written = snprintf(
        meta_path,
        sizeof(meta_path),
        "%s%cgame_engine_runtime_meta.lua",
        bin_dir,
        MAXCORE_PATH_SEPARATOR
    );

    if (written < 0 || (size_t)written >= sizeof(meta_path)) {
        return luaL_error(
            L,
            "MaxCore: Metadata file path is too long"
        );
    }

    file = fopen(meta_path, "w");

    if (file == NULL) {
        return luaL_error(
            L,
            "MaxCore: Could not write metadata file: %s",
            meta_path
        );
    }

    fputs(
        "---@meta\n\n"
        "-- Generated automatically by the MaxCore runtime bootstrap.\n"
        "-- Experimental fallback metadata; do not edit manually.\n\n"
        "---@class EngineCore\n"
        "EngineCore = {}\n\n",
        file
    );

    visited_count = 0;
    emitted_count = 0;

    lua_getglobal(L, "EngineCore");

    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        fclose(file);

        return luaL_error(
            L,
            "MaxCore: EngineCore is not a table"
        );
    }

    emit_table_fields(L, file, -1, "EngineCore", 0);

    lua_pop(L, 1);

    if (fclose(file) != 0) {
        return luaL_error(
            L,
            "MaxCore: Failed closing metadata file: %s",
            meta_path
        );
    }

    return 0;
}

#endif /* MAXCORE_EXPERIMENTAL_METADATA */

/*
** ---------------------------------------------------------------------------
** Path utilities
** ---------------------------------------------------------------------------
*/

/*
** Copy a path while checking its capacity.
*/
static int MaxCore_CopyPath(
    char *output,
    size_t output_size,
    const char *source
) {
    int written;

    if (output == NULL || output_size == 0 || source == NULL) {
        return -1;
    }

    written = snprintf(output, output_size, "%s", source);

    if (written < 0 || (size_t)written >= output_size) {
        return -1;
    }

    return 0;
}

/*
** Return the absolute path to the running executable.
**
** This does not depend on the command-line working directory.
*/
static int MaxCore_GetExecutablePath(
    char *output,
    size_t output_size
) {
    if (output == NULL || output_size == 0) {
        return -1;
    }

#ifdef _WIN32

    {
        DWORD length = GetModuleFileNameA(
            NULL,
            output,
            (DWORD)output_size
        );

        if (length == 0 || length >= output_size) {
            return -1;
        }

        output[length] = '\0';
        return 0;
    }

#elif defined(__APPLE__)

    {
        uint32_t length = (uint32_t)output_size;

        if (_NSGetExecutablePath(output, &length) != 0) {
            return -1;
        }

        /*
        ** Resolve the executable path when possible.
        */
        {
            char resolved[PATH_MAX];

            if (realpath(output, resolved) != NULL) {
                return MaxCore_CopyPath(
                    output,
                    output_size,
                    resolved
                );
            }
        }

        return 0;
    }

#elif defined(__linux__)

    {
        ssize_t length = readlink(
            "/proc/self/exe",
            output,
            output_size - 1
        );

        if (length < 0 || (size_t)length >= output_size - 1) {
            return -1;
        }

        output[length] = '\0';
        return 0;
    }

#else

    /*
    ** Unsupported platform: do not silently treat the current
    ** working directory as the executable directory.
    */
    return -1;

#endif
}

/*
** Remove the executable filename, leaving its containing directory.
**
** The result retains a trailing path separator.
*/
static int MaxCore_GetExecutableDirectory(
    char *output,
    size_t output_size
) {
    char executable[PATH_MAX];
    char *slash;
    char *backslash;
    char *last_separator;

    if (MaxCore_GetExecutablePath(
            executable,
            sizeof(executable)
        ) != 0) {
        return -1;
    }

    slash = strrchr(executable, '/');
    backslash = strrchr(executable, '\\');

    if (slash == NULL) {
        last_separator = backslash;
    } else if (backslash == NULL) {
        last_separator = slash;
    } else {
        last_separator = slash > backslash ? slash : backslash;
    }

    if (last_separator == NULL) {
        return -1;
    }

    last_separator[1] = '\0';

    return MaxCore_CopyPath(
        output,
        output_size,
        executable
    );
}

/*
** Append a path component.
**
** The directory is expected to end with a separator.
*/
static int MaxCore_JoinPath(
    char *output,
    size_t output_size,
    const char *directory,
    const char *component
) {
    int written;

    if (directory == NULL || component == NULL) {
        return -1;
    }

    written = snprintf(
        output,
        output_size,
        "%s%s",
        directory,
        component
    );

    if (written < 0 || (size_t)written >= output_size) {
        return -1;
    }

    return 0;
}

/*
** Check whether max_core.lua exists at the specified root.
*/
static int MaxCore_HasCoreFile(const char *root) {
    char candidate[PATH_MAX];
    FILE *file;

    if (MaxCore_JoinPath(
            candidate,
            sizeof(candidate),
            root,
            "max_core.lua"
        ) != 0) {
        return 0;
    }

    file = fopen(candidate, "rb");

    if (file == NULL) {
        return 0;
    }

    fclose(file);
    return 1;
}

/*
** Remove the final directory component.
**
** Returns -1 when there is no parent directory to inspect.
*/
static int MaxCore_ParentDirectory(char *directory) {
    size_t length;
    char *slash;
    char *backslash;
    char *last_separator;

    if (directory == NULL) {
        return -1;
    }

    length = strlen(directory);

    if (length == 0) {
        return -1;
    }

    /*
    ** Remove trailing separators, but preserve filesystem roots.
    */
    while (length > 1 &&
           (directory[length - 1] == '/' ||
            directory[length - 1] == '\\')) {

#ifdef _WIN32
        if (length == 3 &&
            directory[1] == ':' &&
            (directory[2] == '/' || directory[2] == '\\')) {
            return -1;
        }
#endif

        directory[--length] = '\0';
    }

    slash = strrchr(directory, '/');
    backslash = strrchr(directory, '\\');

    if (slash == NULL) {
        last_separator = backslash;
    } else if (backslash == NULL) {
        last_separator = slash;
    } else {
        last_separator =
            slash > backslash ? slash : backslash;
    }

    if (last_separator == NULL) {
        return -1;
    }

    /*
    ** Preserve POSIX root.
    */
    if (last_separator == directory) {
        directory[1] = '\0';
        return -1;
    }

#ifdef _WIN32

    /*
    ** Preserve Windows drive roots, e.g. C:\.
    */
    if (last_separator == directory + 2 &&
        directory[1] == ':') {
        directory[3] = '\0';
        return -1;
    }

#endif

    last_separator[1] = '\0';

    return 0;
}

/*
** Locate the project root by searching the executable directory and
** successive parent directories for max_core.lua.
**
** A configured fallback can be supplied through MAXCORE_DEFAULT_ROOT.
*/
static int MaxCore_FindProjectRoot(
    char *root,
    size_t root_size
) {
    char directory[PATH_MAX];
    unsigned int depth;

    if (MaxCore_GetExecutableDirectory(
            directory,
            sizeof(directory)
        ) != 0) {
        return -1;
    }

    for (depth = 0; depth <= MAXCORE_ROOT_SEARCH_DEPTH; ++depth) {
        if (MaxCore_HasCoreFile(directory)) {
            return MaxCore_CopyPath(
                root,
                root_size,
                directory
            );
        }

        if (MaxCore_ParentDirectory(directory) != 0) {
            break;
        }
    }

    /*
    ** Optional absolute installation/development fallback.
    */
    if (MAXCORE_DEFAULT_ROOT[0] != '\0' &&
        MaxCore_HasCoreFile(MAXCORE_DEFAULT_ROOT)) {

        if (MaxCore_CopyPath(
                root,
                root_size,
                MAXCORE_DEFAULT_ROOT
            ) != 0) {
            return -1;
        }

        /*
        ** Ensure the fallback directory ends with a separator.
        */
        {
            size_t length = strlen(root);

            if (length == 0 || length + 1 >= root_size) {
                return -1;
            }

            if (root[length - 1] != '/' &&
                root[length - 1] != '\\') {
                root[length] = MAXCORE_PATH_SEPARATOR;
                root[length + 1] = '\0';
            }
        }

        return 0;
    }

    return -1;
}

/*
** ---------------------------------------------------------------------------
** Lua module search paths
** ---------------------------------------------------------------------------
*/

/*
** Add project-relative locations to package.path.
**
** Supported examples:
**
**   require("storage")
**   require("modules.storage")
**   require("utilities.helper")
**   require("modules.submodule")
**
** For dotted names, Lua replaces dots with path separators.
**
** This function preserves the existing package.path entries.
*/
static int MaxCore_PrepareModulePaths(
    lua_State *L,
    const char *root
) {
    char module_path[MAXCORE_PATH_BUFFER];
    const char *existing_path;
    int written;

    lua_getglobal(L, "package");

    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);

        return luaL_error(
            L,
            "MaxCore: Lua package library is unavailable"
        );
    }

    lua_getfield(L, -1, "path");

    existing_path = lua_tostring(L, -1);

    if (existing_path == NULL) {
        lua_pop(L, 2);

        return luaL_error(
            L,
            "MaxCore: package.path is unavailable"
        );
    }

    written = snprintf(
        module_path,
        sizeof(module_path),

        /*
        ** Root-level modules.
        */
        "%s?.lua;"
        "%s?%cinit.lua;"

        /*
        ** modules/ hierarchy.
        */
        "%smodules%c?.lua;"
        "%smodules%c?%cinit.lua;"

        /*
        ** utilities/ hierarchy.
        */
        "%sutilities%c?.lua;"
        "%sutilities%c?%cinit.lua;"

        /*
        ** Preserve Lua's existing search paths.
        */
        "%s",

        root,
        root, MAXCORE_PATH_SEPARATOR,

        root, MAXCORE_PATH_SEPARATOR,
        root, MAXCORE_PATH_SEPARATOR, MAXCORE_PATH_SEPARATOR,

        root, MAXCORE_PATH_SEPARATOR,
        root, MAXCORE_PATH_SEPARATOR, MAXCORE_PATH_SEPARATOR,

        existing_path
    );

    if (written < 0 ||
        (size_t)written >= sizeof(module_path)) {

        lua_pop(L, 2);

        return luaL_error(
            L,
            "MaxCore: Lua module search path is too long"
        );
    }

    /*
    ** Stack:
    **   package
    **   old package.path
    **
    ** Replace package.path.
    */
    lua_pushstring(L, module_path);
    lua_setfield(L, -3, "path");

    lua_pop(L, 2);

    return 0;
}

/*
** ---------------------------------------------------------------------------
** Bootstrap initialization
** ---------------------------------------------------------------------------
*/

void MaxCore_Initialize(lua_State *L) {
    int status;
    int module_index;

    char root[PATH_MAX];
    char core_path[PATH_MAX];

    if (L == NULL) {
        return;
    }

    /*
    ** 1. Locate the project root independently of the current
    **    command-line working directory.
    */
    if (MaxCore_FindProjectRoot(
            root,
            sizeof(root)
        ) != 0) {

        luaL_error(
            L,
            "MaxCore: Could not locate max_core.lua. "
            "Place the executable inside the project tree, "
            "or configure MAXCORE_DEFAULT_ROOT."
        );
        return;
    }

    /*
    ** 2. Construct the core utility path.
    */
    if (MaxCore_JoinPath(
            core_path,
            sizeof(core_path),
            root,
            "max_core.lua"
        ) != 0) {

        luaL_error(
            L,
            "MaxCore: Core utility path is too long"
        );
        return;
    }

    /*
    ** 3. Configure Lua module search paths.
    */
    MaxCore_PrepareModulePaths(L, root);

#ifdef MAXCORE_EMBEDDED_MODULES

    /*
    ** 4. Register optional compiled-in Lua modules.
    */
    MaxCore_RegisterEmbeddedModules(L);

#endif

    /*
    ** 5. Load the core utility.
    */
    status = luaL_loadfile(L, core_path);

    if (status != LUA_OK) {
        lua_error(L);
        return;
    }

    /*
    ** 6. Execute the core module.
    */
    status = lua_pcall(L, 0, 1, 0);

    if (status != LUA_OK) {
        lua_error(L);
        return;
    }

    if (!lua_istable(L, -1)) {
        luaL_error(
            L,
            "MaxCore: max_core.lua must return a module table"
        );
        return;
    }

    module_index = lua_gettop(L);

    /*
    ** 7. Cache the module in package.loaded.
    */
    lua_getglobal(L, "package");

    if (!lua_istable(L, -1)) {
        luaL_error(
            L,
            "MaxCore: Lua package library is unavailable"
        );
        return;
    }

    lua_getfield(L, -1, "loaded");

    if (!lua_istable(L, -1)) {
        luaL_error(
            L,
            "MaxCore: package.loaded is unavailable"
        );
        return;
    }

    lua_pushvalue(L, module_index);
    lua_setfield(L, -2, "max_core");

    lua_pop(L, 2);

    /*
    ** 8. Retrieve module.call.
    */
    lua_getfield(L, module_index, "call");

    if (!lua_isfunction(L, -1)) {
        luaL_error(
            L,
            "MaxCore: max_core.lua must return a table "
            "containing a call function"
        );
        return;
    }

    /*
    ** 9. Invoke module.call(module).
    */
    lua_pushvalue(L, module_index);

    status = lua_pcall(L, 1, 1, 0);

    if (status != LUA_OK) {
        lua_error(L);
        return;
    }

    if (!lua_istable(L, -1)) {
        luaL_error(
            L,
            "MaxCore: max_core.lua call() must return "
            "the EngineCore table"
        );
        return;
    }

    /*
    ** 10. Publish the initialized engine.
    */
    lua_setglobal(L, "EngineCore");

    /*
    ** 11. Remove the original module table.
    */
    lua_pop(L, 1);

#ifdef MAXCORE_EXPERIMENTAL_METADATA

    /*
    ** 12. Generate optional runtime metadata.
    */
    if (generate_engine_meta(L, root) != 0) {
        return;
    }

#endif
}
