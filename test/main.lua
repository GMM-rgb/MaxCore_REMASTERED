local utflib = require("utf8")
local core = require("max_core").call()
local runner = core:LoadService("RunnerService")
local storage = core:LoadService("StorageService")

local contents, msg = storage:CreateFile({
    extension = "json",
    name = "data",
});

local MainThread = runner.Stepped:Connect(function(deltaTime)
    io.stdout:write("\27[A\27[K"); io.stdout:setvbuf("line"); io.stdout:flush()
    print(string.format("%s %g", "RUNTIME DT:\t", deltaTime)) collectgarbage("step")
end, { maxFails = 3, maxCatchUp = 0.1, safe = true }); print(msg) runner:KeepAlive()

local success = os and os.execute("clear")
local exited = core.MainKit:WaitForCondition(function ()
    return success == true
end, math.huge) print("EXITED:", tostring(exited))
