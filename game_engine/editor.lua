---@alias ButtonCoordinates { x: number, y: number }
local core <const> = require("max_core").call()
local window = core:LoadService("WindowService")
local runtime = core:LoadService("RunnerService")
local storage = core:LoadService("StorageService")
local inputs = core:LoadService("InputService")
local game = window:CreateWindow("Game Editor", 850, 800)
local NoiseGeneration = core.NoiseClass.new(os.time()) or {}
local ExitEvent = core.Event.new("exit")

if not game or not core.IsA(game, "WindowObject") then
    return 0x1 -- raise error
end

-- local InitalizeWindow = coroutine.create(function()
--     local rx, ry = window:GetDisplayResolution(); local px, py = game:GetPosition()
--     game:SetPosition(0, 0); game:SetPosition(math.floor(rx / 2), math.floor(ry / 2))
--     game:SetDimensions(math.ceil(rx / 1.25), math.ceil(ry / 1.25)); game:SwapBuffers()
--     print(tostring(rx), tostring(ry))
-- end)

---@class ButtonObject
---@field new fun(name: string, pos: ButtonCoordinates): ButtonObject
---@field DisplayButton fun(self: ButtonObject): nil
---@field hitbox ButtonCoordinates
---@field position ButtonCoordinates
---@field button PolygonObject
local ButtonInstancer = setmetatable({}, nil)
ButtonInstancer.__index = ButtonInstancer
core.InstanceType.SetType(ButtonInstancer, "ButtonObject")
ButtonInstancer.DefaultPoints = ({
    { 0, 0 }, { 20, 0 },
    { 20, 10 }, { 0, 10 },
});

---@return ButtonObject
function ButtonInstancer.new(name, pos)
    ---@type ButtonObject
    local self = setmetatable({}, ButtonInstancer)
    local defaults = ButtonInstancer.DefaultPoints
    self.button = game:CreatePolygon(defaults, true)
    self.button:SetPosition(pos.x, pos.y)
    self.position = pos or { x = 0, y = 0 }
    self.hitbox = { x = 0, y = 0 }
    return self
end

function ButtonInstancer:DisplayButton()
    if not self then return end
    if not self.button then return end
    self.button:Render(game)
    return nil
end

local TestButton = ButtonInstancer.new("test", { x = 100, y = 100 })

local function TickGame()
    if game ~= nil then
        ExitEvent:Fire(game:IsRunning() or false)
        TestButton:DisplayButton()
        inputs:UpdateAll()
        game:SwapBuffers()
    end
end

---@param status boolean
ExitEvent:Connect(function(status)
    if status ~= nil and status == false then
        core.colorPrint("DEBUG", "EXITING...")
        game:Close(); os.exit(0x000, true)
    end
end)

runtime.Stepped:Connect(TickGame)
-- coroutine.resume(InitalizeWindow)
runtime:KeepAlive()
