cmake_minimum_required(VERSION 3.15)
project(lua_native_engine CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =========================================================================
# LUA DYNAMIC DISCOVERY & BUILD
# =========================================================================
file(GLOB LUA_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/lua-*/src")

if(LUA_DIRS)
    list(GET LUA_DIRS 0 LUA_SRC_DIR)
else()
    set(LUA_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lua-5.7.2/src")
endif()

# MaxCore bootstrap files
set(MAXCORE_BOOTSTRAP_C
    "${LUA_SRC_DIR}/maxcore_bootstrap.c"
)

set(MAXCORE_BOOTSTRAP_H
    "${LUA_SRC_DIR}/maxcore_bootstrap.h"
)

# Fail early if the bootstrap files are missing
if(NOT EXISTS "${MAXCORE_BOOTSTRAP_C}")
    message(FATAL_ERROR
        "[Engine Error] Missing MaxCore bootstrap source: ${MAXCORE_BOOTSTRAP_C}"
    )
endif()

if(NOT EXISTS "${MAXCORE_BOOTSTRAP_H}")
    message(FATAL_ERROR
        "[Engine Error] Missing MaxCore bootstrap header: ${MAXCORE_BOOTSTRAP_H}"
    )
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# Enforce output directories for multi-config generators
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${OUTPUTCONFIG}" CONFIG)

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG} "${CMAKE_BINARY_DIR}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG} "${CMAKE_BINARY_DIR}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG} "${CMAKE_BINARY_DIR}")
endforeach()

message(STATUS "[Engine] Using Lua source directory: ${LUA_SRC_DIR}")
message(STATUS "[Engine] Using MaxCore bootstrap: ${MAXCORE_BOOTSTRAP_C}")

include_directories("${LUA_SRC_DIR}")

file(GLOB LUA_SOURCES "${LUA_SRC_DIR}/*.c")

set(LUA_LIB_SOURCES ${LUA_SOURCES})

# These sources belong to executables or the MaxCore bootstrap.
# Do NOT compile maxcore_bootstrap.c into lua_shared.
list(REMOVE_ITEM LUA_LIB_SOURCES
    "${LUA_SRC_DIR}/lua.c"
    "${LUA_SRC_DIR}/luac.c"
    "${MAXCORE_BOOTSTRAP_C}"
)

# =========================================================================
# PLATFORM LIBS & FRAMEWORKS DEFINITION
# =========================================================================
find_package(Threads REQUIRED)

set(PLATFORM_LIBS Threads::Threads)

if(WIN32)
    message(STATUS "[Engine] Target OS: Windows")

    list(APPEND PLATFORM_LIBS
        winmm
        user32
        urlmon
    )

elseif(APPLE)
    message(STATUS "[Engine] Target OS: macOS")

    list(APPEND PLATFORM_LIBS
        "-framework CoreAudio"
        "-framework CoreFoundation"
        "-framework AudioToolbox"
        "-framework ApplicationServices"
        "-framework Cocoa"
        "-lobjc"
    )

elseif(UNIX)
    message(STATUS "[Engine] Target OS: Linux / Unix")

    set(SEARCH_INCLUDES "")
    set(SEARCH_LIBS "")

    if(DEFINED ENV{CPATH})
        string(REPLACE ":" ";" SEARCH_INCLUDES "$ENV{CPATH}")
    endif()

    if(DEFINED ENV{LIBRARY_PATH})
        string(REPLACE ":" ";" SEARCH_LIBS "$ENV{LIBRARY_PATH}")
    endif()

    find_path(X11_INCLUDE_DIR
        NAMES X11/Xlib.h
        PATHS ${SEARCH_INCLUDES}
    )

    find_library(X11_LIB
        NAMES X11
        PATHS ${SEARCH_LIBS}
    )

    find_library(XTST_LIB
        NAMES Xtst
        PATHS ${SEARCH_LIBS}
    )

    if(X11_INCLUDE_DIR AND X11_LIB)
        message(STATUS "[Engine] Found X11 headers at: ${X11_INCLUDE_DIR}")
        message(STATUS "[Engine] Found libX11 at: ${X11_LIB}")

        include_directories("${X11_INCLUDE_DIR}")

        list(APPEND PLATFORM_LIBS
            m
            dl
            "${X11_LIB}"
        )

        if(XTST_LIB)
            message(STATUS "[Engine] Found libXtst at: ${XTST_LIB}")
            list(APPEND PLATFORM_LIBS "${XTST_LIB}")
        endif()

    else()
        message(FATAL_ERROR
            "\n[Engine Error] Could not locate X11 headers or libX11.so!\n"
        )
    endif()
endif()

# =========================================================================
# BUILD SHARED LUA LIBRARY
# =========================================================================
add_library(lua_shared SHARED
    ${LUA_LIB_SOURCES}
    backend/platform_module.cpp
)

set_target_properties(lua_shared PROPERTIES
    PREFIX ""
    OUTPUT_NAME "lua55"
)

target_link_libraries(lua_shared PRIVATE
    ${PLATFORM_LIBS}
)

# Windows Lua DLL Export Configuration
if(WIN32)
    set_source_files_properties(
        ${LUA_LIB_SOURCES}
        PROPERTIES COMPILE_DEFINITIONS LUA_BUILD_AS_DLL
    )
endif()

# =========================================================================
# BUILD LUA RUNNER + MAXCORE BOOTSTRAP
# =========================================================================
# Compile maxcore_bootstrap.c directly into the runner so that
# MaxCore_Initialize() is defined when lua.c references it.
add_executable(lua_runner
    "${LUA_SRC_DIR}/lua.c"
    "${MAXCORE_BOOTSTRAP_C}"
)

target_include_directories(lua_runner PRIVATE
    "${LUA_SRC_DIR}"
)

target_link_libraries(lua_runner PRIVATE
    lua_shared
)

set_target_properties(lua_runner PROPERTIES
    OUTPUT_NAME "max-core-lua"
)

# =========================================================================
# PLATFORM COMPILE DEFINITIONS FOR LUA
# =========================================================================
if(WIN32)
    message(STATUS "[Engine] Target OS: Windows")

elseif(APPLE)
    message(STATUS "[Engine] Enabling LUA_USE_MACOSX for io.popen support")

    target_compile_definitions(lua_shared PRIVATE LUA_USE_MACOSX)
    target_compile_definitions(lua_runner PRIVATE LUA_USE_MACOSX)

elseif(UNIX)
    message(STATUS "[Engine] Enabling LUA_USE_LINUX for io.popen support")

    target_compile_definitions(lua_shared PRIVATE LUA_USE_LINUX)
    target_compile_definitions(lua_runner PRIVATE LUA_USE_LINUX)
endif()

# =========================================================================
# NATIVE C++ MODULES CONFIGURATION
# =========================================================================
set(NATIVE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/backend/sound_service_backend.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/backend/input_native.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/backend/storage_interface.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/backend/window_management.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/backend/physics_management.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/backend/native_utils.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/backend/platform_module.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/backend/runtime_backend/scheduler_native.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/backend/runtime_backend/sleep_native.cpp"
)

# All native modules need lua_shared + platform libs
list(APPEND PLATFORM_LIBS lua_shared)

# =========================================================================
# MODULE TARGET GENERATION
# =========================================================================
foreach(SOURCE_FILE IN LISTS NATIVE_SOURCES)
    get_filename_component(TARGET_NAME "${SOURCE_FILE}" NAME_WE)
    add_library("${TARGET_NAME}" SHARED "${SOURCE_FILE}")
    target_link_libraries("${TARGET_NAME}" PRIVATE ${PLATFORM_LIBS})

    if(WIN32)
        target_compile_definitions("${TARGET_NAME}" PRIVATE LUA_BUILD_AS_DLL)
    endif()

    if(TARGET_NAME STREQUAL "sound_service_backend")
        set_target_properties("${TARGET_NAME}" PROPERTIES PREFIX "" OUTPUT_NAME "sound_native")
    else()
        set_target_properties("${TARGET_NAME}" PROPERTIES PREFIX "" OUTPUT_NAME "${TARGET_NAME}")
    endif()
endforeach()
