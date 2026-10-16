local core = require("max_core").call()
local platform = require("utils.platform")
local storage = core:LoadService("StorageService")
local runtime = core:LoadService("RunnerService")
local sound = core:LoadService("SoundService")

runtime:Spawn("Heartbeat", function(wait, join)
    sound:SetStorageService(storage)
    -- local audio = sound:LoadSound("")
end)

print("RUNTIME NATIVE:", runtime:HasNative())
runtime:Pump(0.02); runtime:KeepAlive()
