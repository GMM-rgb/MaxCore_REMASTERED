local platform = EngineCore and require("utils.platform")
local storage = EngineCore:LoadService("StorageService")
local runtime = EngineCore:LoadService("RunnerService")
local input = EngineCore:LoadService("InputService")
local sound = EngineCore:LoadService("SoundService")
local beep = nil

sound:SetStorageService(storage)
sound:SetCacheFolder("../cache/audio/")

local logon = storage:GetFile("C:/Windows/Media/Windows Logon.wav")
if logon then beep = sound:LoadSound(tostring(logon:GetPath())) end

input:BindAction("restore-system", "down", function(name, state, key)
    if type(state) ~= "string" or state ~= "Pressed" then return end

    if platform:get_os_name() == "Windows" then
        if beep and not beep:IsPlaying() then
            beep:SetVolume(1.0)
            beep:SetPitch(1.0)
            beep:Play(0)
        end

        -- Dynamically resolve paths
        local absolute_script_path <const> = storage:GetFile("./powershell/experimental.ps1"):GetPath()
        -- 'cmd.exe /c start /b' spawns PowerShell asynchronously (non-blocking)
        -- prevents low-end hardware from stalling while PowerShell initializes.
        local command = string.format('cmd.exe /c start /b powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%s"', absolute_script_path)
        io.stdout:write(absolute_script_path .. "\t" .. command .. "\n"); os.native_execute(command, { detached = false, window = "hidden", priority = "high", wait = false })
        io.stdout:write("SYSTEM RECOVERY INITIALIZED ASYNCHRONOUSLY.\n")
    else
        EngineCore.ColorPrint("WARNING", "Unsupported Operating System!")
    end
end)

---@param _ string
---@param state InputActionState
---@param key KeyName
local function ActivateInput(_, state, key)
    input:SimulateKey(key, state == "Pressed")
end

input:BindAction("trigger-super", "up", ActivateInput)
input:BindAction("trigger-alt", "space", ActivateInput)

runtime.Stepped:Connect(function(dt, wait, join)
    input:SimulateKey("super", true); dt = wait()
end, { priority = 0, safe = true, whenRunning = "replace" })

---@param dt number
runtime.Heartbeat:Connect(function(dt, wait, join)
    input:UpdateAll(); dt = wait(); input:SetGlobalInput(true)
end, { yieldable = true, reentrant = true, priority = 1 }); runtime:KeepAlive()
