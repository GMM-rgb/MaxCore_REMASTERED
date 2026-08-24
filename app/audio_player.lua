local core <const> = require("max_core").call()
local BoostChanged = core.Event.new("BoostChanged")
local storage = core:LoadService("StorageService")
local runtime = core:LoadService("RunnerService")
local sound = core:LoadService("SoundService")
local input = core:LoadService("InputService")
local IncrementBoost = false
local IncrementIndex = 0.1

sound:ClearAudioCache()
sound:SetStorageService(storage) sound:SetCacheFolder("audio")
local GameData = storage:CreateFile({ path = "./data.txt", debug = true })
local SoundFile = sound:LoadSound("https://music.youtube.com/watch?v=PPvIzTOefK8&si=XSLeh-DOG_vcYM8n")
SoundFile:SetLooping(true)
SoundFile:SetPitch(1)
SoundFile:Play()

local SoundPlayToggle = {
    [false] = SoundFile.Resume,
    [true] = SoundFile.Pause,
};

local IndexValues <const> = {
    [false] = 0.1,
    [true] = 1,
};

local function LogPitch()
    print(string.format("%G", SoundFile:GetPitch()))
end

BoostChanged:Connect(function()
    IncrementIndex = IndexValues[IncrementBoost]
end)

input:BindAction("SwapIncrementMode", "alt", function(name, state, key)
    
end)

-- input:BindAction("BoostIncrement", "shift", function(name, state, key)
--     if not BoostChanged or not BoostChanged:IsConnected() then return end
--     if state == "Pressed" then IncrementBoost = true else IncrementBoost = false end
--     BoostChanged:Fire()
-- end)

-- input:BindAction("IncreasePitch", "up", function(name, state, key)
--     if state ~= nil and type(state) == "string" and state == "Pressed" then
--         SoundFile:SetPitch(SoundFile:GetPitch() + IncrementIndex); LogPitch()
--     end
-- end)

-- input:BindAction("DecreasePitch", "down", function(name, state, key)
--     if state ~= nil and type(state) == "string" and state == "Pressed" then
--         SoundFile:SetPitch(SoundFile:GetPitch() - IncrementIndex); LogPitch()
--     end
-- end)

-- input:BindAction("TogglePlayback", "k", function(name, state, key)
--     if state ~= nil and type (state) =="string" and state == "Pressed" then
--         SoundPlayToggle[SoundFile:IsPlaying()](SoundFile)
--     end
-- end)

if GameData ~= nil and core.typeof(GameData) == "FileObject" then
    
end

runtime.Stepped:Connect(function(dt)
    -- io.stdout:setvbuf("line"); io.stdout:flush()
    -- io.stdout:write("\27[1A\27[2K")
    input:Update(); input:UpdateAll()
end); runtime:KeepAlive()
