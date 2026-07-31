#define NOMINMAX

// =========================================================================
// MINIAUDIO STRIPPING CONFIGURATION
// =========================================================================
// #define MA_NO_DEVICE_IO              // Disable raw device callbacks
#define MA_NO_ENCODING                  // Disables audio recording/saving capabilities
#define MA_NO_GENERATION                // Disables synth generators (sine wave, noise, etc.)
#define MA_NO_DSOUND                    // Disable legacy DirectSound (forces modern WASAPI on Windows)
#define MA_NO_WINMM                     // Disable legacy WinMM backend
#define MA_NO_JACK                      // Disable JACK audio backend
#define MA_NO_NULL                      // Disable Null audio backend

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
static float g_masterVolume = 1.0f;

static bool ensureEngine() {
    if (!g_engineInitialized) {
        if (ma_engine_init(NULL, &g_engine) == MA_SUCCESS) {
            g_engineInitialized = true;
        }
    }
    return g_engineInitialized;
}

static SoundHandle* getOrCreateSound(const std::string& path) {
    if (!ensureEngine()) return nullptr;

    auto it = g_sounds.find(path);
    if (it != g_sounds.end()) {
        return it->second;
    }

    SoundHandle* handle = new SoundHandle();
    ma_result result = ma_sound_init_from_file(
        &g_engine, 
        path.c_str(), // Fixed: using path.c_str()
        MA_SOUND_FLAG_DECODE, // Decodes into memory instantly for immediate duration access
        NULL, 
        NULL, 
        &handle->sound
    );

    if (result != MA_SUCCESS) {
        delete handle;
        return nullptr;
    }

    handle->isLoaded = true;
    g_sounds[path] = handle;
    return handle;
}

// sound_native.play(path, volume, pitch, loop, pan, startTime)
static int l_play(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    float volume = (float)luaL_optnumber(L, 2, 1.0);
    float pitch = (float)luaL_optnumber(L, 3, 1.0);
    bool loop = lua_toboolean(L, 4);
    float pan = (float)luaL_optnumber(L, 5, 0.0);
    float startTime = (float)luaL_optnumber(L, 6, -1.0); // Default to -1.0 (do not overwrite position)

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

    // Only seek if a valid timestamp (>= 0.0) was explicitly provided
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
        // Wrapped in extra parens to bypass any macro expansion
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
        ma_sound_stop(&pair.second->sound);
        ma_sound_uninit(&pair.second->sound);
        delete pair.second;
    }
    g_sounds.clear();
    return 0;
}

// sound_native.set_time_position(path, seconds)
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

// sound_native.get_time_position(path) -> number
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

// sound_native.get_duration(path) -> number
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
        {nullptr, nullptr}
    };

#if LUA_VERSION_NUM >= 502
    luaL_newlib(L, soundFuncs);
#else
    luaL_register(L, "sound_native", soundFuncs);
#endif
    return 1;
}
