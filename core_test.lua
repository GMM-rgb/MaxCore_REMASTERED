if _G.io and type(io) == "table" then
    collectgarbage("restart")
    io.stdout:setvbuf("line")
    collectgarbage("collect")
end

local MaxCore = require("max_core").call()
local SoundService = MaxCore:LoadService("SoundService")
local InputService = MaxCore:LoadService("InputService")
local RunnerService = MaxCore:LoadService("RunnerService")
local VirtualCursorClicked = MaxCore and MaxCore.Event.new("CursorClicked")
local BackgroundMusicToggle = MaxCore and MaxCore.Event.new("MusicToggle")
local MusicNextTrackTrigger = MaxCore and MaxCore.Event.new("TrackSwitch")

---@type integer
local CurrentTrackIndex = 1
---@type table<SoundObject>
local LoadedMusicTracks = {
    SoundService:LoadSound("../audio/saxobeat.mp3"),
    SoundService:LoadSound("../audio/bang_bang_bang.mp3"),
    SoundService:LoadSound("../audio/error_music.wav"),
    SoundService:LoadSound("../audio/oof_song.wav"),
};

if InputService._actions["VirtualClick"] == nil then
    InputService:BindAction("VirtualClick", "space")
end

---@type JobOptions
local GameRuntimeConfiguration = {
    maxFails = 2, safe = true, priority = 82,
    tag = "Main-Application-Thread",
};

---@type SoundObject
local BackgroundMusic =
LoadedMusicTracks[CurrentTrackIndex]
local HasFiredCompletion = false
local SkipFirstClear = false
local MusicToggled = false

BackgroundMusicToggle:Connect(function(state)
    if not BackgroundMusic or not MaxCore.IsA(BackgroundMusic, "SoundObject") then return end
    MusicToggled = (state ~= nil) and state or not MusicToggled
    local operation = MusicToggled and "Play" or "Stop"
    getmetatable(BackgroundMusic)[operation](BackgroundMusic)
end)

MusicNextTrackTrigger:Connect(function()
    BackgroundMusicToggle:Fire(false)
    CurrentTrackIndex = CurrentTrackIndex + 1

    if CurrentTrackIndex > #LoadedMusicTracks then
        CurrentTrackIndex = 1
    end

    BackgroundMusic = LoadedMusicTracks[CurrentTrackIndex]
    BackgroundMusic:SetTimePosition(0)
    HasFiredCompletion = false
    SkipFirstClear = true

    BackgroundMusicToggle:Fire(true)
end)

---@param deltaTime number
local BackgroundMusicListener = RunnerService.Stepped:Connect(function(deltaTime)
    if BackgroundMusic and MaxCore.IsA(BackgroundMusic, "SoundObject") then
        local pos = BackgroundMusic:GetTimePosition()
        local duration = BackgroundMusic:GetDuration()

        if not HasFiredCompletion and pos >= (duration - 0.02) then
            HasFiredCompletion = true
            print("MUSIC COMPLETED... INDEX:", tostring(CurrentTrackIndex))
            MusicNextTrackTrigger:Fire()
            return
        end

        if BackgroundMusic:IsPlaying() and not HasFiredCompletion then
            if SkipFirstClear then SkipFirstClear = false else
                io.write("\27[A\27[2K")
            end print(string.format("%.1f / %.1f", pos, duration))
        end
    end
end, { tag = "Background-Music-Listener" })

RunnerService.Stepped:Connect(function(dt)
    if InputService and MaxCore.typeof(InputService) == "InputService" then
        if InputService:IsKeyDown(InputService._actions["VirtualClick"]) then
            VirtualCursorClicked:Fire()
        end InputService:Update()
    end collectgarbage("step")
end, GameRuntimeConfiguration)

print() BackgroundMusicToggle:Fire(true)
RunnerService:KeepAlive()
