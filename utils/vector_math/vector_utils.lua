local VectorUtils = {}

---@param screenX number
---@param screenY number
---@return number, number
function VectorUtils.toCenterRelative(screenX, screenY)
    local cx = love.graphics.getWidth() / 2
    local cy = love.graphics.getHeight() / 2
    return screenX - cx, screenY - cy
end

return VectorUtils
