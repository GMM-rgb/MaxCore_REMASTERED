local CoreLib <const> = require("max_core").call(); CoreLib.MainKit:Load()
local InputListener = CoreLib and CoreLib:LoadService("InputService")
local RuntimeService = CoreLib and CoreLib:LoadService("RunnerService")
local StorageManager = CoreLib and CoreLib:LoadService("StorageService")
local name, ext = StorageManager.SplitFileName("data_mappings.txt")
local QeuryingPlayer = CoreLib.Event.new("QeuryingPlayerDataModelEvent")
local PlayerInstanced = CoreLib.Event.new("PlayerInstancedEvent")

---@class game
---@field _objects table<any>
---@field _events table<Event>

---@type game
local game = {
    _objects = {},
    _events = {}
};

---@param AppendingData table
---@param TargetPlayerObject PlayerObject
local function ApplyPlayerDataField(TargetPlayerObject, AppendingData)

end

local QueryPlayerThread = coroutine.create(function()
    if CoreLib.IsA(QeuryingPlayer, "Event") and not QeuryingPlayer:IsConnected() then
        ---@param TargetName string
        ---@param RecievedData table
        ---@param ActivePermissions PlayerPermissions
        local QueryPlayerConnection = QeuryingPlayer:Connect(function(TargetName, RecievedData, ActivePermissions)
            local NewPlayer = CoreLib.PlayerDataModel.new(TargetName, ActivePermissions, false)
            pcall(ApplyPlayerDataField, NewPlayer, RecievedData)
        end); io.write(tostring(QeuryingPlayer:IsConnected()))
    end
end); coroutine.resume(QueryPlayerThread)
