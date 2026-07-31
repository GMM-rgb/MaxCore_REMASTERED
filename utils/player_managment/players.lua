local runner = require("services.runner_service")
local InstanceType = require("instance_type")
local PlayerManagment = setmetatable({}, nil)
PlayerManagment.core = nil

-- ====================================================
--               PLAYER OBJECT TYPE STUBS
-- ====================================================

---@class PlayerPermissions 

---@class PlayerObject 
---@field GetName fun(): string
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

    print("INSTANCED PLAYER:\t" .. tostring(self._name or "<unkown>"))
    print("PLAYER CLASSNAME:\t" .. InstanceType.GetType(self)) return self
end

---@param self PlayerObject
function PlayerManagment:GetName()
    if not self._name or type(self._name) ~= "string" then return string.char(0) end
end

return PlayerManagment
