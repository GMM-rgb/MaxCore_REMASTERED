local core <const> = require("max_core").call()
local window = core:LoadService("WindowService")
local runtime = core:LoadService("RunnerService")
local inputs = core:LoadService("InputService")
local game = window:CreateWindow("Game Editor", 850, 800)
local NoiseGeneration = core.NoiseClass.new(os.time()) or {}
local ExitEvent = core.Event.new("exit")
---@alias ButtonCoordinates { x: number, y: number }

if not game then
    return 0x1 -- raise error
end

local InitalizeWindow = coroutine.create(function()
    local rx, ry = window:GetDisplayResolution(); local px, py = game:GetPosition()
    game:SetPosition(0, 0); game:SetPosition(math.floor(rx / 2), math.floor(ry / 2))
    game:SetDimensions(math.ceil(rx / 1.25), math.ceil(ry / 1.25)); game:SwapBuffers()
    print(rx, ry)
end)

---@class ButtonObject
---@field new fun(name: string, pos: ButtonCoordinates): ButtonObject
---@field UpdateHitbox fun(self: ButtonObject): nil
---@field hitbox ButtonCoordinates
---@field position ButtonCoordinates
---@field button PolygonObject
local ButtonInstancer = setmetatable({}, nil)
ButtonInstancer.DefaultPoints = {
    { 0, 0}, { 20, 0 },
    { 20, 10 }, { 0, 10 },
};

---@return ButtonObject
function ButtonInstancer.new(name, pos)
    ---@type ButtonObject
    local self = setmetatable({}, ButtonInstancer)
    local defaults = ButtonInstancer.DefaultPoints
    self.button = game:CreatePolygon(defaults)
    self.position = pos or { x = 0, y = 0 }
    self.hitbox = { x = 0, y = 0 }

    coroutine.wrap(function()
        self.button:SetPosition(table.unpack(pos))
        self.button.Fill = not self.button.Fill
    end)()

    return self
end

---@return nil
function ButtonInstancer:UpdateHitbox()
    
end

function ButtonInstancer:DisplayButton()
    if not self then return end
    if not self.button then return end
    self.button:Render(game)
end

local TestButton = ButtonInstancer.new("test", { x = 50, y = 50 })
io.stdout:write(tostring(TestButton.button.Fill) .. "\n")

local function TickGame()
    if game ~= nil then
        ExitEvent:Fire(game:IsRunning() or false)
        game:SwapBuffers(); inputs:UpdateAll()
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
coroutine.resume(InitalizeWindow)
runtime:KeepAlive()
