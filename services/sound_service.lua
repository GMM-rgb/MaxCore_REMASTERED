-- #service sound_service.lua
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

local function isUrl(path)
    if type(path) ~= "string" then return false end
    return path:find("^https?://") ~= nil 
        or path:find("youtube%.com/") ~= nil 
        or path:find("youtu%.be/") ~= nil
end

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

    if IS_LOVE and not isUrl(filePath) then
        local success, sourceOrErr = pcall(love.audio.newSource, filePath, "static")
        if success then
            self._loveSource = sourceOrErr
        end
    end

    return self
end

---@param self SoundObject
---@return number
function SoundObject:_GetEffectiveVolume()
    local masterVol = self._service and self._service:GetMasterVolume() or 1.0
    return self._volume * masterVol
end

---@param self SoundObject
---@param startTime number?
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
        local targetTime = startTime and math.max(0, startTime) or -1.0
        sound_native.play(self._path, self:_GetEffectiveVolume(), self._pitch, self._loop, self._panning, targetTime)
    end
end

---@param self SoundObject
function SoundObject:Pause()
    self._isPaused = true
    if IS_LOVE and self._loveSource then
        self._loveSource:pause()
    elseif native_ok and type(sound_native.pause) == "function" then
        sound_native.pause(self._path)
    end
end

---@param self SoundObject
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

---@param self SoundObject
function SoundObject:Stop()
    self._isPaused = false
    if IS_LOVE and self._loveSource then
        self._loveSource:stop()
    elseif native_ok and type(sound_native.stop) == "function" then
        sound_native.stop(self._path)
    end
end

---@param self SoundObject
---@param vol number
function SoundObject:SetVolume(vol)
    self._volume = math.max(0, vol)

    if IS_LOVE and self._loveSource then
        self._loveSource:setVolume(self:_GetEffectiveVolume())
    elseif native_ok and type(sound_native.set_volume) == "function" then
        sound_native.set_volume(self._path, self:_GetEffectiveVolume())
    end
end

---@param self SoundObject
---@return number
function SoundObject:GetVolume()
    return self._volume
end

---@param self SoundObject
---@param pitch number
function SoundObject:SetPitch(pitch)
    self._pitch = math.max(0.01, pitch)

    if IS_LOVE and self._loveSource then
        self._loveSource:setPitch(self._pitch)
    elseif native_ok and type(sound_native.set_pitch) == "function" then
        sound_native.set_pitch(self._path, self._pitch)
    end
end

---@param self SoundObject
---@return number
function SoundObject:GetPitch()
    return self._pitch
end

---@param self SoundObject
---@param shouldLoop boolean
function SoundObject:SetLooping(shouldLoop)
    self._loop = not not shouldLoop

    if IS_LOVE and self._loveSource then
        self._loveSource:setLooping(self._loop)
    elseif native_ok and type(sound_native.set_looping) == "function" then
        sound_native.set_looping(self._path, self._loop)
    end
end

---@param self SoundObject
---@return boolean
function SoundObject:IsLooping()
    return self._loop
end

---@param self SoundObject
---@param pan number
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

---@param self SoundObject
---@return boolean
function SoundObject:IsPlaying()
    if IS_LOVE and self._loveSource then
        return self._loveSource:isPlaying()
    elseif native_ok and type(sound_native.is_playing) == "function" then
        return sound_native.is_playing(self._path)
    end
    return false
end

---@param self SoundObject
---@return SoundObject
function SoundObject:Clone()
    local clone = SoundObject.new(self._path, self._service)
    clone:SetVolume(self._volume)
    clone:SetPitch(self._pitch)
    clone:SetLooping(self._loop)
    clone:SetPan(self._panning)
    return clone
end

---@param self SoundObject
---@param seconds number
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

---@param self SoundObject
---@return number|unknown
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

---@param self SoundObject
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

-- =========================================================================
-- MAIN SERVICE CLASS (Zero parameters in .new() to comply with framework)
-- =========================================================================

---@class SoundService
---@field _masterVolume number
---@field _cachedSounds table<string, SoundObject>
---@field _storageStorage StorageService|nil
---@field _cacheSubFolder string
local SoundService = {}
SoundService.__index = SoundService
InstanceType.SetType(SoundService, "SoundService")

function SoundService.new()
    local self = setmetatable({}, SoundService)
    self._masterVolume = 1.0
    self._cachedSounds = {}
    self._cacheSubFolder = "audio_cache"
    self._storageStorage = nil
    return self
end

--- Manually maps a StorageService instance independently post-init
---@param storageService StorageService
function SoundService:SetStorageService(storageService)
    assert(storageService, "StorageService instance cannot be nil")
    self._storageStorage = storageService
end

--- Manually updates the cache folder path and pushes it directly to the C++ backend
---@param self SoundService
---@param subfolder string
function SoundService:SetCacheFolder(subfolder)
    assert(type(subfolder) == "string" and subfolder ~= "", "Cache folder path must be a non-empty string")
    self._cacheSubFolder = subfolder

    if native_ok and type(sound_native.set_cache_dir) == "function" then
        local fullCachePath = subfolder
        
        if self._storageStorage then
            if type(self._storageStorage.CreateDirectory) == "function" then
                self._storageStorage:CreateDirectory(self._cacheSubFolder)
            end
            local base = type(self._storageStorage.GetBaseDirectory) == "function" 
                and self._storageStorage:GetBaseDirectory() 
                or "./"
            local sep = (base:sub(-1) == "/" or base:sub(-1) == "\\") and "" or "/"
            fullCachePath = base .. sep .. self._cacheSubFolder
        end

        sound_native.set_cache_dir(fullCachePath)
    end
end

---@return string
function SoundService:GetCachePath()
    if self._storageStorage then
        local base = type(self._storageStorage.GetBaseDirectory) == "function"
            and self._storageStorage:GetBaseDirectory()
            or "./"
        local sep = (base:sub(-1) == "/" or base:sub(-1) == "\\") and "" or "/"
        return base .. sep .. self._cacheSubFolder
    end
    return self._cacheSubFolder
end

---@return integer
function SoundService:GetCacheSize()
    if not self._storageStorage or type(self._storageStorage.ListDirectory) ~= "function" then return 0 end

    local entries = self._storageStorage:ListDirectory(self._cacheSubFolder)
    local totalSize = 0

    for _, file in ipairs(entries) do
        if not file:IsDirectory() then
            totalSize = totalSize + file:GetSize()
        end
    end

    return totalSize
end

--[=[
    Import audio files for program playback.  
    **VALID:** local-file-path / youtube URL.
]=]
---@param self SoundService
---@param filePath string
function SoundService:LoadSound(filePath)
    if not self._cachedSounds[filePath] then
        self._cachedSounds[filePath] = SoundObject.new(filePath, self)
    end
    return self._cachedSounds[filePath]
end

---@param self SoundService
---@param vol number
function SoundService:SetMasterVolume(vol)
    self._masterVolume = math.max(0, vol)

    if IS_LOVE then
        love.audio.setVolume(self._masterVolume)
    elseif native_ok and type(sound_native.set_master_volume) == "function" then
        sound_native.set_master_volume(self._masterVolume)
    end

    for _, sound in pairs(self._cachedSounds) do
        sound:SetVolume(sound:GetVolume())
    end
end

---@return number
function SoundService:GetMasterVolume()
    return self._masterVolume
end

function SoundService:StopAll()
    if IS_LOVE then
        love.audio.stop()
    elseif native_ok and type(sound_native.stop_all) == "function" then
        sound_native.stop_all()
    end
end

function SoundService:ClearAudioCache()
    self:StopAll()

    for key, _ in pairs(self._cachedSounds) do
        if isUrl(key) then
            self._cachedSounds[key] = nil
        end
    end

    collectgarbage("collect")

    if native_ok and type(sound_native.clear_cache) == "function" then
        sound_native.clear_cache()
    end

    if self._storageStorage and type(self._storageStorage.ListDirectory) == "function" then
        local entries = self._storageStorage:ListDirectory(self._cacheSubFolder)
        for _, file in ipairs(entries) do
            if not file:IsDirectory() then
                self._storageStorage:DeleteFile(self._cacheSubFolder .. "/" .. file:GetName())
            end
        end
    end

    return true
end

return SoundService
