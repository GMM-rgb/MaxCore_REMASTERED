#define NOMINMAX

// =========================================================================
// MINIAUDIO STRIPPING CONFIGURATION
// =========================================================================
#define MA_NO_ENCODING                     
#define MA_NO_GENERATION                 
#define MA_NO_DSOUND                    
#define MA_NO_WINMM                     
#define MA_NO_JACK                       
#define MA_NO_NULL                       

#define MINIAUDIO_IMPLEMENTATION
#include "inline_headers/mini_audio.hpp"

#ifdef max
    #undef max
#endif

#ifdef min
    #undef min
#endif

#include <lua.hpp>
#include <string>
#include <unordered_map>
#include <mutex>
#include <algorithm>
#include <filesystem>
#include <iostream>

#include "inline_headers/youtube_extractor.hpp"

namespace fs = std::filesystem;

struct SoundHandle {
    ma_sound sound;
    bool isLoaded = false;
    float volume = 1.0f;
    float pitch = 1.0f;
    float pan = 0.0f;
    bool loop = false;
};

static ma_engine g_engine;
static bool g_engineInitialized = false;
static std::unordered_map<std::string, SoundHandle*> g_sounds;
static std::mutex g_soundMutex;

// Default directly to project-relative "TempAudio" if not configured
static fs::path g_customCacheDir = "TempAudio"; 
static float g_masterVolume = 1.0f;

// Consolidated cache directory resolver that properly respects absolute and relative roots
static fs::path getCacheDirectory() {
    fs::path target = g_customCacheDir.empty() ? fs::path("TempAudio") : g_customCacheDir;
    fs::path absoluteTarget = fs::absolute(target);
    std::error_code ec;
    if (!fs::exists(absoluteTarget)) {
        fs::create_directories(absoluteTarget, ec);
    }
    return absoluteTarget;
}

static bool ensureEngine() {
    if (!g_engineInitialized) {
        if (ma_engine_init(NULL, &g_engine) == MA_SUCCESS) {
            g_engineInitialized = true;
        }
    }
    return g_engineInitialized;
}

// Unload & release file handles to prevent OS file locks during cleanup
static void unloadSoundHandle(SoundHandle* handle) {
    if (handle) {
        if (handle->isLoaded) {
            ma_sound_stop(&handle->sound);
            ma_sound_uninit(&handle->sound);
            handle->isLoaded = false;
        }
        delete handle;
    }
}

static SoundHandle* getOrCreateSound(const std::string& path) {
    std::cout << "[Engine Debug] Requested sound load: " << path << std::endl;

    if (!ensureEngine()) {
        std::cout << "[Engine Debug] ERROR: miniaudio engine failed to initialize!" << std::endl;
        return nullptr;
    }

    auto it = g_sounds.find(path);
    if (it != g_sounds.end()) {
        std::cout << "[Engine Debug] Sound handle found in memory cache." << std::endl;
        return it->second;
    }

    std::string localFilePath = YouTubeExtractor::ResolveToLocalFile(path, getCacheDirectory());
    if (localFilePath.empty()) {
        std::cout << "[Engine Debug] ERROR: Resolving local file path failed!" << std::endl;
        return nullptr;
    }

    std::cout << "[Engine Debug] Passing local file to miniaudio: " << localFilePath << std::endl;

    SoundHandle* handle = new SoundHandle();
    ma_result result = ma_sound_init_from_file(
        &g_engine, 
        localFilePath.c_str(), 
        MA_SOUND_FLAG_DECODE, 
        NULL, 
        NULL, 
        &handle->sound
    );

    if (result != MA_SUCCESS) {
        std::cout << "[Engine Debug] ERROR: ma_sound_init_from_file failed with error code: " << result << std::endl;
        delete handle;
        return nullptr;
    }

    std::cout << "[Engine Debug] Sound loaded and ready for playback." << std::endl;
    handle->isLoaded = true;
    g_sounds[path] = handle;
    return handle;
}

// =========================================================================
// LUA BINDINGS
// =========================================================================

static int l_play(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    float volume = (float)luaL_optnumber(L, 2, 1.0);
    float pitch = (float)luaL_optnumber(L, 3, 1.0);
    bool loop = lua_toboolean(L, 4);
    float pan = (float)luaL_optnumber(L, 5, 0.0);
    float startTime = (float)luaL_optnumber(L, 6, -1.0);

    std::lock_guard<std::mutex> lock(g_soundMutex);
    SoundHandle* handle = getOrCreateSound(path);
    if (!handle) return 0;

    handle->volume = volume;
    handle->pitch = pitch;
    handle->loop = loop;
    handle->pan = pan;

    ma_sound_set_volume(&handle->sound, volume * g_masterVolume);
    ma_sound_set_pitch(&handle->sound, pitch);
    ma_sound_set_looping(&handle->sound, loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_pan(&handle->sound, pan);

    if (startTime >= 0.0f) {
        ma_uint32 sampleRate = 0;
        ma_sound_get_data_format(&handle->sound, NULL, NULL, &sampleRate, NULL, 0);
        if (sampleRate == 0) sampleRate = ma_engine_get_sample_rate(&g_engine);

        ma_uint64 targetFrame = (ma_uint64)(startTime * sampleRate);
        ma_sound_seek_to_pcm_frame(&handle->sound, targetFrame);
    }

    ma_sound_start(&handle->sound);
    return 0;
}

static int l_stop(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    std::lock_guard<std::mutex> lock(g_soundMutex);

    auto it = g_sounds.find(path);
    if (it != g_sounds.end()) {
        ma_sound_stop(&it->second->sound);
        ma_sound_seek_to_pcm_frame(&it->second->sound, 0);
    }
    return 0;
}

static int l_pause(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    std::lock_guard<std::mutex> lock(g_soundMutex);

    auto it = g_sounds.find(path);
    if (it != g_sounds.end()) {
        ma_sound_stop(&it->second->sound);
    }
    return 0;
}

static int l_resume(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    std::lock_guard<std::mutex> lock(g_soundMutex);

    auto it = g_sounds.find(path);
    if (it != g_sounds.end()) {
        ma_sound_start(&it->second->sound);
    }
    return 0;
}

static int l_set_volume(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    float volume = (float)luaL_checknumber(L, 2);

    std::lock_guard<std::mutex> lock(g_soundMutex);
    auto it = g_sounds.find(path);
    if (it != g_sounds.end()) {
        it->second->volume = volume;
        ma_sound_set_volume(&it->second->sound, volume * g_masterVolume);
    }
    return 0;
}

static int l_set_pitch(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    float pitch = (float)luaL_checknumber(L, 2);

    std::lock_guard<std::mutex> lock(g_soundMutex);
    auto it = g_sounds.find(path);
    if (it != g_sounds.end()) {
        it->second->pitch = pitch;
        ma_sound_set_pitch(&it->second->sound, pitch);
    }
    return 0;
}

static int l_set_looping(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    bool shouldLoop = lua_toboolean(L, 2);

    std::lock_guard<std::mutex> lock(g_soundMutex);
    auto it = g_sounds.find(path);
    if (it != g_sounds.end()) {
        it->second->loop = shouldLoop;
        ma_sound_set_looping(&it->second->sound, shouldLoop ? MA_TRUE : MA_FALSE);
    }
    return 0;
}

static int l_set_pan(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    float pan = (float)luaL_checknumber(L, 2);

    std::lock_guard<std::mutex> lock(g_soundMutex);
    auto it = g_sounds.find(path);
    if (it != g_sounds.end()) {
        it->second->pan = (std::max)(-1.0f, (std::min)(1.0f, pan));
        ma_sound_set_pan(&it->second->sound, it->second->pan);
    }
    return 0;
}

static int l_set_master_volume(lua_State* L) {
    float vol = (float)luaL_checknumber(L, 1);
    std::lock_guard<std::mutex> lock(g_soundMutex);
    g_masterVolume = (std::max)(0.0f, vol);

    for (auto& pair : g_sounds) {
        ma_sound_set_volume(&pair.second->sound, pair.second->volume * g_masterVolume);
    }
    return 0;
}

static int l_is_playing(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    std::lock_guard<std::mutex> lock(g_soundMutex);

    auto it = g_sounds.find(path);
    if (it != g_sounds.end()) {
        lua_pushboolean(L, ma_sound_is_playing(&it->second->sound) == MA_TRUE);
        return 1;
    }
    lua_pushboolean(L, false);
    return 1;
}

static int l_stop_all(lua_State* L) {
    std::lock_guard<std::mutex> lock(g_soundMutex);
    for (auto& pair : g_sounds) {
        unloadSoundHandle(pair.second);
    }
    g_sounds.clear();
    return 0;
}

static int l_unload_sound(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    std::lock_guard<std::mutex> lock(g_soundMutex);

    auto it = g_sounds.find(path);
    if (it != g_sounds.end()) {
        unloadSoundHandle(it->second);
        g_sounds.erase(it);
    }
    return 0;
}

static int l_set_time_position(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    float seconds = (float)luaL_checknumber(L, 2);

    std::lock_guard<std::mutex> lock(g_soundMutex);
    SoundHandle* handle = getOrCreateSound(path);
    if (handle) {
        ma_uint32 sampleRate = 0;
        ma_sound_get_data_format(&handle->sound, NULL, NULL, &sampleRate, NULL, 0);
        if (sampleRate == 0) sampleRate = ma_engine_get_sample_rate(&g_engine);

        ma_uint64 targetFrame = (ma_uint64)(seconds * sampleRate);
        ma_sound_seek_to_pcm_frame(&handle->sound, targetFrame);
    }
    return 0;
}

static int l_get_time_position(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);

    std::lock_guard<std::mutex> lock(g_soundMutex);
    SoundHandle* handle = getOrCreateSound(path);
    if (handle) {
        float cursorInSeconds = 0.0f;
        if (ma_sound_get_cursor_in_seconds(&handle->sound, &cursorInSeconds) == MA_SUCCESS) {
            lua_pushnumber(L, (double)cursorInSeconds);
            return 1;
        }
    }
    lua_pushnumber(L, 0.0);
    return 1;
}

static int l_get_duration(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);

    std::lock_guard<std::mutex> lock(g_soundMutex);
    SoundHandle* handle = getOrCreateSound(path);
    if (handle) {
        float lengthInSeconds = 0.0f;
        if (ma_sound_get_length_in_seconds(&handle->sound, &lengthInSeconds) == MA_SUCCESS) {
            lua_pushnumber(L, (double)lengthInSeconds);
            return 1;
        }
    }
    lua_pushnumber(L, 0.0);
    return 1;
}

static int l_set_cache_dir(lua_State* L) {
    const char* dirPath = luaL_checkstring(L, 1);
    std::lock_guard<std::mutex> lock(g_soundMutex);
    g_customCacheDir = fs::path(dirPath);

    std::error_code ec;
    fs::create_directories(fs::absolute(g_customCacheDir), ec);
    return 0;
}

static int l_clear_cache(lua_State* L) {
    std::lock_guard<std::mutex> lock(g_soundMutex);

    // Unload all active handles to release file locks on Windows
    for (auto& pair : g_sounds) {
        unloadSoundHandle(pair.second);
    }
    g_sounds.clear();

    fs::path targetDir = getCacheDirectory();
    std::error_code ec;
    if (fs::exists(targetDir)) {
        for (auto& entry : fs::directory_iterator(targetDir, ec)) {
            fs::remove_all(entry.path(), ec);
        }
    }

    lua_pushboolean(L, true);
    return 1;
}

#if defined(_WIN32) || defined(_WIN64)
    #define EXPORT_FN __declspec(dllexport)
#else
    #define EXPORT_FN
#endif

extern "C" EXPORT_FN int luaopen_sound_native(lua_State* L) {
    static const luaL_Reg soundFuncs[] = {
        {"play", l_play},
        {"stop", l_stop},
        {"pause", l_pause},
        {"resume", l_resume},
        {"set_volume", l_set_volume},
        {"set_pitch", l_set_pitch},
        {"set_looping", l_set_looping},
        {"set_pan", l_set_pan},
        {"set_master_volume", l_set_master_volume},
        {"set_time_position", l_set_time_position},
        {"get_time_position", l_get_time_position},
        {"get_duration", l_get_duration},
        {"is_playing", l_is_playing},
        {"stop_all", l_stop_all},
        {"unload", l_unload_sound},
        {"set_cache_dir", l_set_cache_dir},
        {"clear_cache", l_clear_cache},
        {nullptr, nullptr}
    };

#if LUA_VERSION_NUM >= 502
    luaL_newlib(L, soundFuncs);
#else
    luaL_register(L, "sound_native", soundFuncs);
#endif
    return 1;
}
