local core = require("lua-5.7.2.src.max_core").call()
local storage = core:LoadService("StorageService")
local runtime = core:LoadService("RunnerService")
local input = core:LoadService("InputService")
local sound = core:LoadService("SoundService")
local window = core:LoadService("WindowService")

sound:SetStorageService(storage); sound:SetCacheFolder("../cache/audio/")
local beep = sound:LoadSound("C:/Windows/Media/Windows Logon.wav")

input:BindAction("restore-system", "down", function(name, state, key)
    if type(state) ~= "string" or state ~= "Pressed" then return end

    if beep and not beep:IsPlaying() then
        beep:SetVolume(1.0)
        beep:SetPitch(1.0)
        beep:Play(0)
    end

    -- Dynamically resolve paths
    local absolute_script_path <const> = storage:GetFile("./powershell/experimental.ps1"):GetPath()
    -- Use 'cmd.exe /c start /b' to spawn PowerShell asynchronously (non-blocking)
    -- This prevents low-end hardware from stalling while PowerShell initializes.
    local command = string.format('cmd.exe /c start /b powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%s"', absolute_script_path)
    io.stdout:write(absolute_script_path .. "\t" .. command .. "\n"); os.native_execute(command, true)
    io.stdout:write("SYSTEM RECOVERY INITIALIZED ASYNCHRONOUSLY.\n")
end)

input:BindAction("trigger-meta", "up", function(name, state, key)
    input:SimulateKey("meta", state == "Pressed")
end)

input:BindAction("trigger-alt", "space", function(name, state, key)
    input:SimulateKey("alt", state == "Pressed")
end)

---@param dt number
runtime.Heartbeat:Connect(function(dt, wait, join)
    input:UpdateAll(); dt = wait(); input:SetGlobalInput(true)
end, { yieldable = true, reentrant = true, priority = 1 }); runtime:KeepAlive()
