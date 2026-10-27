/*
** $Id: maxcore_bootstrap.c $
** MaxCore Game Engine bootstrapper
** See Copyright Notice in max_core.lua
*/

#include "maxcore_bootstrap.h"

#include "lauxlib.h"
#include "lualib.h"

#ifdef MAXCORE_EXPERIMENTAL_METADATA

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define MAXCORE_MKDIR(path) _mkdir(path)
#define MAXCORE_PATH_SEPARATOR '\\'
#else
#include <sys/stat.h>
#include <sys/types.h>
#define MAXCORE_MKDIR(path) mkdir(path, 0775)
#define MAXCORE_PATH_SEPARATOR '/'
#endif

#ifndef MAXCORE_LUA_SRC_DIR
#define MAXCORE_LUA_SRC_DIR "./lua-5.7.2/src"
#endif

#define MAXCORE_META_MAX_DEPTH 8
#define MAXCORE_META_MAX_ENTRIES 4096

static const void *visited_tables[MAXCORE_META_MAX_ENTRIES];
static size_t visited_count;
static size_t emitted_count;

static int is_identifier(const char *value) {
    const unsigned char *p = (const unsigned char *)value;

    if (p == NULL || !(isalpha(*p) || *p == '_')) {
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

        /* Remove the value while retaining the key for lua_next. */
        lua_pop(L, 1);
    }
}

static int ensure_directory(const char *path) {
    if (MAXCORE_MKDIR(path) == 0) {
        return 1;
    }

    return errno == EEXIST;
}

static int generate_engine_meta(lua_State *L) {
    char parent_dir[2048];
    char bin_dir[2300];
    char meta_path[2600];
    FILE *file;
    size_t source_length = strlen(MAXCORE_LUA_SRC_DIR);
    int written;

    if (source_length + 1 >= sizeof(parent_dir)) {
        return luaL_error(
            L,
            "MaxCore metadata output path is too long"
        );
    }

    memcpy(parent_dir, MAXCORE_LUA_SRC_DIR, source_length + 1);

    /*
     * LUA_SRC_DIR normally ends in /src or \src.
     * Place the generated metadata in the sibling bin directory.
     */
    if (source_length >= 4 &&
        strcmp(parent_dir + source_length - 4, "/src") == 0) {
        parent_dir[source_length - 4] = '\0';
    } else if (
        source_length >= 4 &&
        strcmp(parent_dir + source_length - 4, "\\src") == 0
    ) {
        parent_dir[source_length - 4] = '\0';
    } else {
        return luaL_error(
            L,
            "Unexpected Lua source directory; expected a path ending "
            "in /src: %s",
            MAXCORE_LUA_SRC_DIR
        );
    }

    written = snprintf(
        bin_dir,
        sizeof(bin_dir),
        "%s%cbin",
        parent_dir,
        MAXCORE_PATH_SEPARATOR
    );

    if (written < 0 || (size_t)written >= sizeof(bin_dir)) {
        return luaL_error(
            L,
            "MaxCore metadata directory path is too long"
        );
    }

    if (!ensure_directory(bin_dir)) {
        return luaL_error(
            L,
            "Could not create MaxCore metadata directory: %s",
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
            "MaxCore metadata file path is too long"
        );
    }

    file = fopen(meta_path, "w");

    if (file == NULL) {
        return luaL_error(
            L,
            "Could not write MaxCore metadata file: %s",
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
            "EngineCore is not a table; cannot generate metadata"
        );
    }

    emit_table_fields(L, file, -1, "EngineCore", 0);
    lua_pop(L, 1);

    if (fclose(file) != 0) {
        return luaL_error(
            L,
            "Failed closing generated metadata file: %s",
            meta_path
        );
    }

    return 0;
}

#endif /* MAXCORE_EXPERIMENTAL_METADATA */

void MaxCore_Initialize(lua_State *L) {
    int status;
    int module_index;

    /* Load the editable project-root module. */
    status = luaL_loadfile(L, "./max_core.lua");

    if (status != LUA_OK) {
        lua_error(L);
        return;
    }

    status = lua_pcall(L, 0, 1, 0);

    if (status != LUA_OK) {
        lua_error(L);
        return;
    }

    if (!lua_istable(L, -1)) {
        luaL_error(
            L,
            "max_core.lua must return a module table"
        );
        return;
    }

    module_index = lua_gettop(L);

    /*
     * Cache the module so require("max_core") does not execute it again
     * in this Lua state.
     */
    lua_getglobal(L, "package");

    if (!lua_istable(L, -1)) {
        luaL_error(
            L,
            "package library is unavailable during MaxCore startup"
        );
        return;
    }

    lua_getfield(L, -1, "loaded");

    if (!lua_istable(L, -1)) {
        luaL_error(
            L,
            "package.loaded is unavailable during MaxCore startup"
        );
        return;
    }

    lua_pushvalue(L, module_index);
    lua_setfield(L, -2, "max_core");

    lua_pop(L, 2);

    /*
     * Invoke module.call(module), equivalent to:
     * require("max_core").call()
     */
    lua_getfield(L, module_index, "call");

    if (!lua_isfunction(L, -1)) {
        luaL_error(
            L,
            "max_core.lua must return a table with a call function"
        );
        return;
    }

    lua_pushvalue(L, module_index);

    status = lua_pcall(L, 1, 1, 0);

    if (status != LUA_OK) {
        lua_error(L);
        return;
    }

    if (!lua_istable(L, -1)) {
        luaL_error(
            L,
            "max_core.lua call() must return the EngineCore table"
        );
        return;
    }

    /* Expose the initialized engine globally. */
    lua_setglobal(L, "EngineCore");

    /* Remove the original module table. */
    lua_pop(L, 1);

#ifdef MAXCORE_EXPERIMENTAL_METADATA
    /*
     * Optional experimental feature:
     * generate LuaLS metadata from the live EngineCore table.
     *
     * Disabled in ordinary builds.
     */
    if (generate_engine_meta(L) != 0) {
        lua_error(L);
        return;
    }
#endif
}