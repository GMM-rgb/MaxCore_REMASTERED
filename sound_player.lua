local core = require("max_core").call()
local sound = core:LoadService("SoundService")
local storage = core:LoadService("StorageService")
local runtime = core:LoadService("RunnerService")

sound:SetStorageService(storage)
sound:SetCacheFolder("audio")

local song = sound:LoadSound("https://www.youtube.com/watch?v=IlouAA8mRZo")
song:SetLooping(true)
song:SetPitch(1.25)
song:Play(0)

runtime.Stepped:Connect(function()
    return
end); runtime:KeepAlive()
