-- #service
local InstanceType = require("instance_type")

package.cpath = package.cpath 
    .. ";./build/?.dylib" 
    .. ";./build/Release/?.dylib" 
    .. ";./build/Debug/?.dylib" 
    .. ";./build/bin/?.dylib" 
    .. ";./build/bin/Release/?.dylib"
    .. ";./build/?/?.dylib"
    .. ";./build/?.so"
    .. ";./build/?.dll"

local native_ok, sound_native = pcall(require, "sound_native")

if not native_ok then
    print("\27[33m[SoundService Warning]\27[0m Failed to load sound_native: " .. tostring(sound_native))
end

local IS_LOVE = (_G.love ~= nil) and (_G.love.audio ~= nil)

---@class SoundObject
---@field _path string
---@field _volume number
---@field _pitch number
---@field _loop boolean
---@field _panning number
---@field _isPaused boolean
---@field _loveSource any | nil
---@field _service SoundService | nil
local SoundObject = {}
SoundObject.__index = SoundObject
InstanceType.SetType(SoundObject, "SoundObject")

function SoundObject.new(filePath, serviceOwner)
    local self = setmetatable({}, SoundObject)
    self._path = filePath
    self._service = serviceOwner
    self._volume = 1.0
    self._pitch = 1.0
    self._loop = false
    self._panning = 0.0
    self._isPaused = false
    self._loveSource = nil

    if IS_LOVE then
        local success, sourceOrErr = pcall(love.audio.newSource, filePath, "static")
        if success then
            self._loveSource = sourceOrErr
        end
    end

    return self
end

function SoundObject:_GetEffectiveVolume()
    local masterVol = self._service and self._service:GetMasterVolume() or 1.0
    return self._volume * masterVol
end

--- Starts or resumes playback of the sound.
---@param startTime number|nil Optional timestamp in seconds to seek to before playing.
function SoundObject:Play(startTime)
    self._isPaused = false

    if IS_LOVE and self._loveSource then
        self._loveSource:stop()
        self._loveSource:setVolume(self:_GetEffectiveVolume())
        self._loveSource:setPitch(self._pitch)
        self._loveSource:setLooping(self._loop)
        if self._loveSource.setPosition then
            self._loveSource:setPosition(self._panning, 0, 0)
        end
        if startTime then
            self._loveSource:seek(math.max(0, startTime), "seconds")
        end
        self._loveSource:play()
    elseif native_ok and type(sound_native.play) == "function" then
        -- Pass startTime if specified, or -1.0 so C++ keeps the current cursor
        local targetTime = startTime and math.max(0, startTime) or -1.0
        sound_native.play(self._path, self._volume, self._pitch, self._loop, self._panning, targetTime)
    end
end

--- Temporarily pauses audio playback without resetting the track position.
function SoundObject:Pause()
    self._isPaused = true
    if IS_LOVE and self._loveSource then
        self._loveSource:pause()
    elseif native_ok and type(sound_native.pause) == "function" then
        sound_native.pause(self._path)
    end
end

--- Resumes audio playback from the exact position it was paused at.
function SoundObject:Resume()
    if not self._isPaused then return end
    self._isPaused = false

    if IS_LOVE and self._loveSource then
        self._loveSource:play()
    elseif native_ok then
        if type(sound_native.resume) == "function" then
            sound_native.resume(self._path)
        else
            self:Play()
        end
    end
end

--- Stops audio playback entirely and resets playback position back to frame 0.
function SoundObject:Stop()
    self._isPaused = false
    if IS_LOVE and self._loveSource then
        self._loveSource:stop()
    elseif native_ok and type(sound_native.stop) == "function" then
        sound_native.stop(self._path)
    end
end

--- Sets the individual playback volume multiplier for this track.
---@param vol number Volume level (>= 0.0, where 1.0 is default volume).
function SoundObject:SetVolume(vol)
    self._volume = math.max(0, vol)

    if IS_LOVE and self._loveSource then
        self._loveSource:setVolume(self:_GetEffectiveVolume())
    elseif native_ok and type(sound_native.set_volume) == "function" then
        sound_native.set_volume(self._path, self._volume)
    end
end

--- Gets the current local volume setting for this sound.
---@return number
function SoundObject:GetVolume()
    return self._volume
end

--- Sets the playback speed and pitch multiplier for this track.
---@param pitch number Pitch multiplier (>= 0.01, where 1.0 is normal, 2.0 is double speed/octave up).
function SoundObject:SetPitch(pitch)
    self._pitch = math.max(0.01, pitch)

    if IS_LOVE and self._loveSource then
        self._loveSource:setPitch(self._pitch)
    elseif native_ok and type(sound_native.set_pitch) == "function" then
        sound_native.set_pitch(self._path, self._pitch)
    end
end

--- Gets the current pitch multiplier setting.
---@return number
function SoundObject:GetPitch()
    return self._pitch
end

--- Configures whether the audio should automatically restart when it reaches the end.
---@param shouldLoop boolean True to loop indefinitely, false to play once.
function SoundObject:SetLooping(shouldLoop)
    self._loop = not not shouldLoop

    if IS_LOVE and self._loveSource then
        self._loveSource:setLooping(self._loop)
    elseif native_ok and type(sound_native.set_looping) == "function" then
        sound_native.set_looping(self._path, self._loop)
    end
end

--- Checks whether the track is set to loop.
---@return boolean
function SoundObject:IsLooping()
    return self._loop
end

--- Sets the stereo balance (panning) of the audio track.
--- Clamps the value between -1.0 (100% left) and 1.0 (100% right).
---@param pan number Stereo position ranging from -1.0 (left) to 0.0 (center) to 1.0 (right).
function SoundObject:SetPan(pan)
    self._panning = math.max(-1.0, math.min(1.0, pan))

    if IS_LOVE and self._loveSource then
        if self._loveSource.setPosition then
            self._loveSource:setPosition(self._panning, 0, 0)
        end
    elseif native_ok and type(sound_native.set_pan) == "function" then
        sound_native.set_pan(self._path, self._panning)
    end
end

--- Checks if the audio track is currently actively playing sound output.
---@return boolean
function SoundObject:IsPlaying()
    if IS_LOVE and self._loveSource then
        return self._loveSource:isPlaying()
    elseif native_ok and type(sound_native.is_playing) == "function" then
        return sound_native.is_playing(self._path)
    end
    return false
end

--- Duplicates this SoundObject with identical path, volume, pitch, loop, and panning settings.
---@return SoundObject
function SoundObject:Clone()
    local clone = SoundObject.new(self._path, self._service)
    clone:SetVolume(self._volume)
    clone:SetPitch(self._pitch)
    clone:SetLooping(self._loop)
    clone:SetPan(self._panning)
    return clone
end

-- =========================================================================
-- MAIN SERVICE CLASS
-- =========================================================================

---@class SoundService
---@field _masterVolume number
---@field _cachedSounds table<string, SoundObject>
local SoundService = {}
SoundService.__index = SoundService
InstanceType.SetType(SoundService, "SoundService")

function SoundService.new()
    local self = setmetatable({}, SoundService)
    self._masterVolume = 1.0
    self._cachedSounds = {}
    return self
end

--- Retrieves a cached SoundObject for the given path, or loads and caches a new one if missing.
---@param filePath string Audio file path.
---@return SoundObject
function SoundService:LoadSound(filePath)
    if not self._cachedSounds[filePath] then
        self._cachedSounds[filePath] = SoundObject.new(filePath, self)
    end
    return self._cachedSounds[filePath]
end

--- Sets the global master volume multiplier for all sounds.
--- **NOTE:** ONLY FOR LOVE2D
---@param vol number Master volume level (>= 0.0).
function SoundService:SetMasterVolume(vol)
    self._masterVolume = math.max(0, vol)

    if IS_LOVE then
        love.audio.setVolume(self._masterVolume)
    elseif native_ok and type(sound_native.set_master_volume) == "function" then
        sound_native.set_master_volume(self._masterVolume)
    end
end

function SoundService:GetMasterVolume()
    return self._masterVolume
end

--- Stops all playing sounds across the entire audio backend immediately.
--- **NOTE:** ONLY FOR LOVE2D
function SoundService:StopAll()
    if IS_LOVE then
        love.audio.stop()
    elseif native_ok and type(sound_native.stop_all) == "function" then
        sound_native.stop_all()
    end
end

--- Sets the current playback position of the sound in seconds.
---@param seconds number Target timestamp in seconds to seek to.
function SoundObject:SetTimePosition(seconds)
    local targetTime = math.max(0, seconds)

    if IS_LOVE and self._loveSource then
        if self._loveSource.seek then
            self._loveSource:seek(targetTime, "seconds")
        end
    elseif native_ok and type(sound_native.set_time_position) == "function" then
        sound_native.set_time_position(self._path, targetTime)
    end
end

--- Gets the current playback timestamp of the sound in seconds.
---@return number Current position in seconds.
function SoundObject:GetTimePosition()
    if IS_LOVE and self._loveSource then
        if self._loveSource.tell then
            return self._loveSource:tell("seconds")
        end
    elseif native_ok and type(sound_native.get_time_position) == "function" then
        return sound_native.get_time_position(self._path)
    end
    return 0.0
end

--- Gets the total duration/length of the audio track in seconds.
---@return number Length in seconds.
function SoundObject:GetDuration()
    if IS_LOVE and self._loveSource then
        if self._loveSource.getDuration then
            return self._loveSource:getDuration("seconds")
        end
    elseif native_ok and type(sound_native.get_duration) == "function" then
        return sound_native.get_duration(self._path)
    end
    return 0.0
end

return SoundService
