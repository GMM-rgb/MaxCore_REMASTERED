local core = require("max_core").call()
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

    -- Dynamically resolve paths and execute the win32 recovery script
    local absolute_script_path <const> = storage:GetFile("recover_script.ps1"):GetPath()
    local command = string.format('powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%s"', absolute_script_path)
    io.stdout:write(absolute_script_path .. "\t" .. command .. "\n")
    local success = os.execute(command); io.stdout:write("SYSTEM RECOVERY INITIALIZED:\t", tostring(success) .. "\n")
end)

---@param dt number
runtime.Heartbeat:Connect(function(dt)
    input:UpdateAll()
    input:SetGlobalInput(true)
    -- input:SimulateKey("meta", true)
end); runtime:KeepAlive()
