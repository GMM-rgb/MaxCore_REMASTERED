local WindowService = EngineCore:LoadService("WindowService")
local RunnerService = EngineCore:LoadService("RunnerService")
local StorageService = EngineCore:LoadService("StorageService")
local <const> ThisDirectory = StorageService:GetBaseDirectory() or "/"
local WindowCanvas = WindowService:CreateWindow("Editor - Blocks") or {}
local BaseInherit = StorageService:GetFile(ThisDirectory .. "/base_block.xml")

local max_aliasing = WindowService:GetMaxAliasingQuality()
WindowCanvas:SetAliasingQuality("2d", max_aliasing)
WindowCanvas:SetRenderer("auto")

---@param color integer[]
---@param scale number
---@return RectObject
local function create_block(color, scale)
    local outline_reduce = 0.2
    local factor = 1 - outline_reduce
    local r, g, b = color[1], color[2], color[3]
    local br, bg, bb = r * factor, g * factor, b * factor
    local BlockCorners = EngineCore.PseudoStylesClass.new("corner", 24)
    local BlockOutline = EngineCore.PseudoStylesClass.new("border", br, bg, bb, 5)
    local CreatedBlock = WindowCanvas:CreateRect(100, 100, 20, 10, r, g, b)
    CreatedBlock = CreatedBlock:ApplyStyle(BlockOutline)
    CreatedBlock = CreatedBlock:ApplyStyle(BlockCorners)
    CreatedBlock:SetScale(scale, scale, scale)
    CreatedBlock:SetColor(r, g, b)
    return CreatedBlock
end

local block = create_block({ 50, 168, 82 }, 10)

RunnerService.Stepped:Connect(function(dt, wait)
    local mx, my = WindowCanvas:GetMousePosition()
    local coordinates_valid = type(mx) ~= "nil" and type(my) ~= "nil"
    if coordinates_valid then block:SetPosition(mx - 20 / 2 * 10, my - 10) end
end, { yieldable = true })

RunnerService.RenderStepped:Connect(function(dt)
    WindowCanvas:IsRunning(); WindowCanvas:SwapBuffers()
    WindowCanvas:ClearCanvas(); WindowCanvas:RenderScene2D()
end)

RunnerService:KeepAlive()
