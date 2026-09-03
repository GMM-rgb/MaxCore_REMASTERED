local VectorUtils = {}
local VectorUtility = {}
---@type WindowService?
VectorUtils.WindowService = nil

---@class WorkspaceDimensions2D
---@field RotX number
---@field RotY number
---@field X number
---@field Y number

---@class WorkspaceDimensions3D : WorkspaceDimensions2D
---@field RotZ number
---@field Z number

---@class DistanceArguments
---@field x1 number
---@field x2 number
---@field y1 number
---@field y2 number

---@class ThreeDistanceArguments : DistanceArguments
---@field z1 number
---@field z2 number

---@param coordinates DistanceArguments
function VectorUtils.GetDistance2D(coordinates)
    local FormatedGroupOne = coordinates.x1 - coordinates.x2
    local FormatedGroupTwo = coordinates.y1 - coordinates.y2
    return math.sqrt(FormatedGroupOne^2 + FormatedGroupTwo^2)
end

---@param coordinates ThreeDistanceArguments
function VectorUtils.GetDistance3D(coordinates)
    local GroupOne = coordinates.x1 - coordinates.x2 ^ 2
    local GroupTwo = coordinates.y1 - coordinates.y2 ^ 2
    local GroupThree = coordinates.z1 - coordinates.z2 ^ 2
    local PrimaryDistance = math.sqrt(GroupOne + GroupTwo)
    local SecondaryDistance =  math.sqrt(PrimaryDistance + GroupThree)
    return SecondaryDistance or math.huge or "<unkown-vector>"
end

function VectorUtility.new()
    local self = setmetatable({}, VectorUtility)
    return self
end

---@generic T
---@param ... VectorInputPattern
---@alias VectorInputPattern { [integer]: table<T> }
---@return WorkspaceDimensions2D?
function VectorUtility:_2D(...)
    local VectorInputsTable <const> = { ... }
    ---@type WorkspaceDimensions2D?
    local WorkspaceDimensionalVector = nil

    for ParamArgumentIndex, ParamArgumentValue in pairs(VectorInputsTable) do
        for _, ArgTableValue in ipairs(ParamArgumentValue) do
            
        end
    end

    return WorkspaceDimensionalVector or nil
end

function VectorUtility:_3D()

end

return {
    VectorWorkspace = VectorUtility,
    VectorMath = VectorUtils,
};
