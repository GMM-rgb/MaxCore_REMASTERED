local core = require("max_core").call()
local sound = core:LoadService("SoundService")
local runtime = core:LoadService("RunnerService")
local window = core:LoadService("WindowService")

window.ShowAlert("MESSAGE", "hello", "info", "ok")

runtime:Spawn("Heartbeat", function(wait, join)
    while true do
        local <const> dt = wait()
        io.stdout:write(dt .. "\n")
    end
end, { yieldable = true })

runtime:Pump(0.01)
runtime:KeepAlive()
