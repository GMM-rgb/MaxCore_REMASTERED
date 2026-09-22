local core = require("max_core").call()
local input = core:LoadService("InputService")
local runtime = core:LoadService("RunnerService")
local window = core:LoadService("WindowService")
---@type {[integer]: WindowObject}
local windows = {}

input:BindAction("WindowRepeat", "down", function (name, state, key)
    table.insert(windows, window:CreateWindow("flush", 100, 100))
end)

runtime.Stepped:Connect(function()
    for _, w in ipairs(windows) do
        w:IsRunning()
        w:SwapBuffers()
    end

    input:UpdateAll()
end); runtime:KeepAlive()
