local CoreLib <const> = require("max_core").call(); CoreLib.MainKit:Load()
local StorageManager = CoreLib:LoadService("StorageService")
local RuntimeService = CoreLib:LoadService("RunnerService")
local InputListener = CoreLib:LoadService("InputService")
local QeuryingPlayer = CoreLib.Event.new("QeuryingPlayerDataModelEvent")
local PlayerInstanced = CoreLib.Event.new("PlayerInstancedEvent")
local MappingName <const>, MappingExtension <const> =
StorageManager.SplitFileName("data_mappings.txt")

---@class PlayerDataModel
---@field DataListKeys fun(selfObj: PlayerDataModel): table<string>
---@field new fun(): PlayerDataModel
---@field _src {[string]: any}

---@type PlayerDataModel
local PlayerDataModel = setmetatable({}, nil)

function PlayerDataModel.new()
    local self = setmetatable(PlayerDataModel, PlayerDataModel)
    return self
end

---@return table<string>
---@param self PlayerDataModel
function PlayerDataModel:DataListKeys()
    local DataKeys = table.create(#self._src)
    if self._src and type(self._src) == "table" then
        for key, _ in pairs(self._src) do
            table.insert(DataKeys, key or nil)
        end
    end return table.create(0)
end

---@class game
---@field _objects table<any>
---@field _events table<Event>

---@type game
local game = {
    _objects = {},
    _events = {},
};

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
    local FormatedPosition = string.format("X:\t%G\nY:\t%G", InputListener:GetMousePosition()); print(FormatedPosition)
    StorageManager:CreateFile({ name = "mouse_logging", path = "./logs", extension = "log", contents = FormatedPosition })
    for _ = 1, 2 do io.stdout:write("\27[1A\27[2K"); collectgarbage("collect") end
end

InputListener:BindAction("binding", "mouse3", function(name, state, key)
    print(state)
end)

InputListener:SetGlobalInput(true)
RuntimeService.Stepped:Connect(tick)
RuntimeService:KeepAlive()

-- local x, y = InputListener:GetMousePosition(); StorageManager:CreateFile({
--     name = "mouse_logging",
--     directory = "./logs",
--     extension = "log",
-- }):Write(string.format("X:\t%G\nY:\t%G", x, y));
