local <const> core = require("max_core").call(); core:Load()
local WindowService <const> = core:LoadService("WindowService")
local RuntimeService <const> = core:LoadService("RunnerService")
local InputService <const> = core:LoadService("InputService")
local SoundService <const> = core:LoadService("SoundService")
local window = WindowService:CreateWindow()
local exit = core.Event.new("exit")

---@param s string
---@return table<string>
function string.split(s)
    local splitted = {}; for i = 1, #s do
        local ss = s:sub(i, i) or string.char(0)
        table.insert(splitted, ss)
    end; return splitted
end

if window and core.typeof(window) == "WindowObject" then
    
end

---@param dt number
RuntimeService.Heartbeat:Connect(function(dt)
    if window == nil or not window:IsRunning() then exit:Fire(); return end
    window:SwapBuffers(); window:StepPhysics(dt); InputService:Update()
end)

exit:Connect(function()
    core.colorPrint("INFO", "CLOSING...")
    os.exit(nil, true)
end)

RuntimeService:KeepAlive()
