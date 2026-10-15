local core = require("max_core").call()
local storage = core:LoadService("StorageService")
local runtime = core:LoadService("RunnerService")
local input = core:LoadService("InputService")

input:BindAction("restore-system", "down", function(name, state, key)
    if type(state) ~= "string" or state ~= "Pressed" then return end

    local absolute_script_path = storage:GetFile("recover_script.ps1"):GetPath() -- Full absolute execution path targeting the script file
    local command = string.format('powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%s"', absolute_script_path) -- Invoke native PowerShell file execution channel, bypassing CMD string parsing limits
    local success = os.execute(command); io.stdout:write("SYSTEM RECOVERY INITIALIZED:\t", tostring(success) .. "\n")
end)

---@param dt number
runtime.Heartbeat:Connect(function(dt)
    input:Update()
end)

runtime:KeepAlive()
