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
    set(LUA_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lua-5.6.0/src")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# Enforce output directories for multi-config generators
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} CONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG} "${CMAKE_BINARY_DIR}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG} "${CMAKE_BINARY_DIR}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG} "${CMAKE_BINARY_DIR}")
endforeach()

message(STATUS "[Engine] Using Lua source directory: ${LUA_SRC_DIR}")
include_directories(${LUA_SRC_DIR})

file(GLOB LUA_SOURCES "${LUA_SRC_DIR}/*.c")
set(LUA_LIB_SOURCES ${LUA_SOURCES})
list(REMOVE_ITEM LUA_LIB_SOURCES "${LUA_SRC_DIR}/lua.c" "${LUA_SRC_DIR}/luac.c")

# Build shared Lua library (lua55)
add_library(lua_shared SHARED ${LUA_LIB_SOURCES})
set_target_properties(lua_shared PROPERTIES PREFIX "" OUTPUT_NAME "lua55")

# FIX: Link C math and dynamic loading library explicitly to Lua on Unix
if(UNIX AND NOT APPLE)
    target_link_libraries(lua_shared PRIVATE m dl)
endif()

# Build custom Lua executable linked to lua_shared
add_executable(lua_runner "${LUA_SRC_DIR}/lua.c")
target_link_libraries(lua_runner PRIVATE lua_shared)
set_target_properties(lua_runner PROPERTIES OUTPUT_NAME "max-core-lua")

# =========================================================================
# PLATFORM COMPILE DEFINITIONS FOR LUA
# =========================================================================
if(WIN32)
    target_compile_definitions(lua_shared PRIVATE LUA_BUILD_AS_DLL)
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
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/sound_service_backend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/input_native.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/storage_interface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/window_management.cpp
)

find_package(Threads REQUIRED)

set(PLATFORM_LIBS Threads::Threads lua_shared)

if(WIN32)
    message(STATUS "[Engine] Target OS: Windows")
    list(APPEND PLATFORM_LIBS winmm user32)
elseif(APPLE)
    message(STATUS "[Engine] Target OS: macOS")
    list(APPEND PLATFORM_LIBS "-framework CoreAudio" "-framework CoreFoundation" "-framework AudioToolbox" "-framework ApplicationServices" "-framework Cocoa")
elseif(NOT APPLE AND UNIX)
    message(STATUS "[Engine] Target OS: Linux / Unix")
    find_package(X11)
    if(NOT X11_FOUND)
        message(FATAL_ERROR 
            "\n[Engine Error] X11 development headers missing!\n"
            "Please install them using your system package manager:\n"
            "  Ubuntu/Debian: sudo apt install libx11-dev\n"
            "  Fedora/RHEL:   sudo dnf install libX11-devel\n"
            "  Arch Linux:    sudo pacman -S libx11\n"
        )
    endif()
    list(APPEND PLATFORM_LIBS m dl X11::X11)
endif()

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
