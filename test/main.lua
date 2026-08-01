---@param name string
---@return string, string
local function SplitFileName(name)
    if not name or type(name) ~= "string" then
        return "<unkown>", "<unkown>"
    end; return name:match("^(.+)%.([^.]+)$")
end

local DataFileFullName <const> = "data.xml"
local CoreLib = require("max_core").call()
local RunnerService = CoreLib:LoadService("RunnerService")
local StorageService = CoreLib:LoadService("StorageService")
local DataFileName, DataFileExtension = SplitFileName(DataFileFullName)



local contents, msg = StorageService:CreateFile({
    path = "./" .. tostring(DataFileFullName),
    extension = DataFileExtension,
    name = DataFileName,
    contents = [[
<?xml version="1.0" encoding="UTF-8"?>

<data xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="data.xsd">
    <player-object>

    </player-object>
</data>
]],
});

local MainThread = RunnerService.Stepped:Connect(function(deltaTime)
    io.stdout:write("\27[A\27[K"); io.stdout:setvbuf("line"); io.stdout:flush()
    print(string.format("%s %g", "RUNTIME DT:\t", deltaTime)); collectgarbage("step")
end, { maxFails = 3, maxCatchUp = 0.1, safe = true }); print(msg); RunnerService:KeepAlive()

local success = os and os.execute("clear")
local exited = CoreLib.MainKit:WaitForCondition(function ()
    return success == true
end, math.huge) print("EXITED:", tostring(exited))
