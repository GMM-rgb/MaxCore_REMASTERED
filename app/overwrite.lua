local CoreLib = require("max_core").call()
local Input = CoreLib:LoadService("InputService")
local Sound = CoreLib:LoadService("SoundService")
local Runtime = CoreLib:LoadService("RunnerService")
Sound:SetStorageService(CoreLib:LoadService("StorageService")); Sound:SetCacheFolder("audio")
local audio = Sound:LoadSound("https://www.youtube.com/watch?v=dQw4w9WgXcQ")

local PlayerConverter = {
    ["Pressed"] = true,
    ["Released"] = false,
};

local PlayerToggle = {
    [false] = audio.Pause,
    [true] = audio.Resume,
};

Input:BindAction("PlayAudio", "space", function(name, state, key)
    if state ~= nil and state == "Pressed" then
        if not audio:IsPlaying() then
            audio:Play()
        end

        if audio ~= nil and CoreLib.typeof(audio) == "SoundObject" then
            local AudioCurrentlyPlaying = audio:IsPlaying()
            if AudioCurrentlyPlaying == nil then return end

            io.stdout:write(tostring(AudioCurrentlyPlaying), "\n")

            xpcall(PlayerToggle[
            PlayerConverter[AudioCurrentlyPlaying]
            ], print, audio)
        end
    end
end)

-- Input:BindAction("SimulateWindowsKey", "w", function (name, state, key)
--     if not state or type(state) ~= "string" or state == "Released" then return end
--     Input:SimulateKey("win", state == "Pressed" and true or false)
--     io.stdout:write("Simulating Windows Input" .. "\n")
-- end)

Runtime.Stepped:Connect(function ()
    Input:UpdateAll()
end); Runtime:KeepAlive()
