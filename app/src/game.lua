local CoreLib <const> = require("max_core").call(); CoreLib.MainKit:Load()
local SoundService = CoreLib:LoadService("SoundService")
local StorageManager = CoreLib:LoadService("StorageService")
local RuntimeService = CoreLib:LoadService("RunnerService")
local InputListener = CoreLib:LoadService("InputService")
local QeuryingPlayer = CoreLib.Event.new("QeuryingPlayerDataModelEvent")
local PlayerInstanced = CoreLib.Event.new("PlayerInstancedEvent")

---@class game
---@field _objects table<any>
---@field _events table<Event>

---@type game
local game = {
    _objects = {},
    _events = {},
};

CoreLib.Instance.new("../temp")

---@param AppendingData table
---@param TargetPlayerObject PlayerObject
local function ApplyPlayerDataField(TargetPlayerObject, AppendingData)
    TargetPlayerObject["data"] = nil
end

if CoreLib.IsA(QeuryingPlayer, "Event") and not QeuryingPlayer:IsConnected() then
    ---@param TargetName string
    ---@param RecievedData table
    ---@param ActivePermissions PlayerPermissions
    local QueryPlayerConnection = QeuryingPlayer:Connect(function(TargetName, RecievedData, ActivePermissions)
        local NewPlayer = CoreLib.PlayerDataModel.new(TargetName, ActivePermissions, false)
        pcall(ApplyPlayerDataField, NewPlayer, RecievedData)
    end);
end

---@param dt number
local function tick(dt)
    InputListener:Update(); io.stdout:flush(); io.stdout:setvbuf("line")
    local FormatedPosition = string.format("X:\t%G\nY:\t%G", InputListener:GetMousePosition())
    CoreLib.colorPrint("INFO", tostring(FormatedPosition))
    for _ = 1, 2 do io.stdout:write("\27[1A\27[2K"); collectgarbage("collect") end
end

InputListener:BindAction("binding", "enter", function(name, state, key)
    print(name .. ":", state, key)
end)

InputListener:SetGlobalInput(true)
RuntimeService.Stepped:Connect(tick)
RuntimeService:KeepAlive()

local x, y = InputListener:GetMousePosition(); StorageManager:CreateFile({
    name = "mouse_logging",
    directory = "../logs",
    extension = "log",
}):Write(string.format("X:\t%G\nY:\t%G", x, y));
