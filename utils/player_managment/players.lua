local runner = require("services.runner_service")
local InstanceType = require("instance_type")
local PlayerManagment = setmetatable({}, nil)
---@type table<PlayerObject>
PlayerManagment.ConnectedPlayers = {}
---@type MaxCore?
PlayerManagment.core = nil

-- ====================================================
--               PLAYER OBJECT TYPE STUBS
-- ====================================================

---@class PlayerPermissions 

---@class PlayerObject 
---@field GetName fun(selfObj: PlayerObject): string
---@field GetSessionTime fun(selfObj: PlayerObject, FormatParams): integer
---@field _name string 

-- ====================================================
--               PLAYER MANAGMENT METHODS
-- ====================================================

local ValidPlayerPermissions <const> = {
    ---@type Event
    ["_IdleEvent"] = nil,
    ["PlayerProcessTerminated"] = false,
    ["PlayerConnectionIdle"] = false,
    ["PlayerDataWrite"] = true,
    ["PlayerDataRead"] = true,
};

---@param name string
---@param permissions table?
---@param sandboxed boolean?
---@return PlayerObject
function PlayerManagment.new(name, permissions, sandboxed)
    if sandboxed == nil or type(sandboxed) ~= "boolean" then sandboxed = false end
    if not permissions then permissions = ValidPlayerPermissions end

    ---@type PlayerObject
    local self = setmetatable(PlayerManagment, {})
    self["_name"] = name or "PlayerInstance"
    InstanceType.SetType(self, "PlayerObject")

    -- print("INSTANCED PLAYER:\t" .. tostring(self._name or "<unkown>"))
    -- print("PLAYER CLASSNAME:\t" .. tostring(InstanceType.GetType(self)))

    coroutine.wrap(function()
        
    end)()

    return self
end

---@param self PlayerObject
function PlayerManagment:GetName()
    if not self._name or type(self._name) ~= "string" then return string.char(0) end
end

---@param self PlayerObject
---@param FormatParams table
function PlayerManagment:GetSessionTime(FormatParams)

end

--- ====================================================
---          PLAYER MANAGMENT OUTBOUND METHODS
--- ====================================================

---@param player PlayerObject|string
---@return boolean
function PlayerManagment.PlayerExists(player)
    if not player or not InstanceType.IsA(player, "PlayerObject") or type(player) ~= "string" then return false end

    ---@return boolean
    local function ValidateConnectedPlayers()
        for _, player in ipairs(PlayerManagment.ConnectedPlayers) do
            if player ~= nil and InstanceType.IsA(player, "PlayerObject") then return true end
        end return false
    end

    if PlayerManagment.ConnectedPlayers ~= nil and type(PlayerManagment.ConnectedPlayers) == "table" then
        if ValidateConnectedPlayers and type(ValidateConnectedPlayers) == "function" and ValidateConnectedPlayers() then
            
        end
    end

    return false
end

return PlayerManagment
