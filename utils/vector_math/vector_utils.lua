local VectorUtils = {}
---@type WindowService?
VectorUtils.WindowService = nil

---@class DistanceArguments
---@field x1 number
---@field x2 number
---@field y1 number
---@field y2 number

---@param coordinates DistanceArguments
function VectorUtils.getDistance(coordinates)
    local FormatedGroupOne = coordinates.x1 - coordinates.x2
    local FormatedGroupTwo = coordinates.y1 - coordinates.y2
    local distance = math.sqrt(FormatedGroupOne^2+FormatedGroupTwo^2)
end

local VectorUtility = {}

function VectorUtility.new()
    local self = setmetatable({}, VectorUtility)
    return self
end

function VectorUtility:_2D()

end

function VectorUtility:_3D()

end

return VectorUtils, VectorUtility
