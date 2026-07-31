---@class InstanceType
local InstanceType = {}
InstanceType.__index = InstanceType

-- Internal symbol key to prevent type spoofing
local TYPE_KEY = "__max_type"

--- Assigns a strict instance type identifier to a target object or class metatable
---@generic T
---@param target T
---@param typeName string
---@return T
function InstanceType.SetType(target, typeName)
    if type(target) ~= "table" then
        error("[InstanceType] Target must be a table or metatable.", 2)
    end
    
    local meta = getmetatable(target) or target
    meta[TYPE_KEY] = typeName
    return target
end

--- Returns the registered type name of an object, falling back to primitive lua type()
---@param object any
---@return string
function InstanceType.GetType(object)
    if type(object) ~= "table" then
        return type(object)
    end

    local meta = getmetatable(object)
    if meta and meta[TYPE_KEY] then
        return meta[TYPE_KEY]
    end

    if object[TYPE_KEY] then
        return object[TYPE_KEY]
    end

    return "table"
end

--- Checks whether an object matches a target type or class name
---@param object any
---@param targetType string
---@return boolean
function InstanceType.IsA(object, targetType)
    return InstanceType.GetType(object) == targetType
end

-- Export module
return {
    SetType = InstanceType.SetType,
    GetType = InstanceType.GetType,
    IsA = InstanceType.IsA,
    -- Metatable Tag Key
    TYPE_KEY = TYPE_KEY,
};
