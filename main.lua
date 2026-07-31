local utflib = require("utf8")
local core = require("max_core").call()
local runner = core:LoadService("RunnerService")
local storage = core:LoadService("StorageService")

---@diagnostic disable-next-line
local ext = (package.config:sub(1,1) == "\\") and "dll" or (jit and jit.os == "OSX" or package.cpath:find("%.dylib") and "dylib" or "so")
package.cpath = package.cpath .. ";./build/?." .. ext .. ";./build/Release/?." .. ext .. ";./build/Debug/?." .. ext .. ";./build/bin/?." .. ext .. ";./build/bin/Release/?." .. ext .. ";./build/*/?/?." .. ext

local ImportSucess, StorageInterface = pcall(require, "storage_interface")


local MainThread = runner.Stepped:Connect(function(deltaTime)
    io.stdout:write("\27[A\27[K"); io.stdout:setvbuf("line"); io.stdout:flush()
    print(string.format("%s %g", "RUNTIME DT:\t", deltaTime)) collectgarbage("step")
end, { maxFails = 3, maxCatchUp = 0.1, safe = true });

print(utflib.char(8))

runner:KeepAlive(); local success = os.execute("clear")
local exited = core.MainKit:WaitForCondition(function ()
    return success == true
end, math.huge) print("EXITED:", tostring(exited))
