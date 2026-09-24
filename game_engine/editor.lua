---@alias ButtonSourceCode fun(name: string, action: InputActionState, key: KeyName): nil
---@alias ButtonCoordinates { x: number, y: number }
local core <const> = require("max_core").call()
local StorageService = core:LoadService("StorageService")
local InputService = core:LoadService("InputService")
local window = core:LoadService("WindowService")
local runtime = core:LoadService("RunnerService")
local game = window:CreateWindow("Game Editor", 850, 800)
local logs = StorageService:CreateFile({ path = "./logs.log" })
local NoiseGeneration = core.NoiseClass.new(os.time())
local ExitEvent = core.Event.new("exit")

if not game or not core.IsA(game, "WindowObject") then
    return 0x1 -- raise error
end

---@class ButtonObject
---@field new fun(name: string, pos: ButtonCoordinates, text: string, source: ButtonSourceCode): ButtonObject
---@field DisplayButton fun(self: ButtonObject): nil
---@field _events {[integer]: Event}
---@field hitbox ButtonCoordinates
---@field position ButtonCoordinates
---@field mouse ButtonCoordinates
---@field button PolygonObject
---@field label TextObject
---@field text string
local ButtonInstancer = setmetatable({}, nil)
ButtonInstancer.__index = ButtonInstancer
core.InstanceType.SetType(ButtonInstancer, "ButtonObject")
ButtonInstancer.DefaultPoints = ({
    { 0, 0 }, { 20, 0 },
    { 20, 10 }, { 0, 10 },
});

---@return ButtonObject
function ButtonInstancer.new(name, pos, text, source)
    local mx, my = InputService:GetMousePosition()
    ---@type boolean
    local is_touching_button = false
    ---@type ButtonObject
    local self = setmetatable({}, ButtonInstancer)
    local defaults = ButtonInstancer.DefaultPoints
    self.button = game:CreatePolygon(defaults, true)
    self.button:SetPosition(pos.x, pos.y)
    self.button:SetScale(6, 4, 0)
    self.position = pos or { x = 0, y = 0 }
    self.hitbox = { x = 0, y = 0 }
    self.mouse = { x = mx, y = my }
    self._events = table.create(0, 2)
    self.text = text

    local TargetTextPosX <const> = self.button.Position.x * 1.25
    local TargetTextPosY <const> = self.button.Position.y * 1.15
    self.label = game:CreateText(self.text, TargetTextPosX, TargetTextPosY, 2, 0, 0, 0, 1)

    ---@param action string
    ---@param state InputActionState
    ---@param key KeyName
    local function TriggerAction(action, state, key)
        if is_touching_button ~= nil and is_touching_button then
            local success = xpcall(source, function(...)
                core.colorPrint("ERROR", ...); error()
            end, action, state, key); print(success)
        end
    end

    if type(name) ~= "string" then name = "ButtonObject" end
    local binding, target = name .. "_click", "mouse1"
    local ClickEvent = core.Event.new("ButtonClicked")
    local TouchingEvent = core.Event.new("TouchingEvent")
    InputService:BindAction(binding, target, TriggerAction)
    table.insert(self._events, TouchingEvent)
    table.insert(self._events, ClickEvent)

    ---@param ... any
    TouchingEvent:Connect(function(...)
        is_touching_button = ({...})[1]
    end)

    return self
end

function ButtonInstancer:DisplayButton()
    if not self then return end
    if not self.button then return end
    if not self._events then return end

    local cmx, cmy = InputService:GetMousePosition()

    ---@return number, number
    local function GetApplicationMouse()
        local wx, wy = game:GetPosition()
        local cmix, cmiy = math.tointeger(cmx), math.tointeger(cmy)
        local cwx, cwy = cmix - wx, cmiy - wy
        return cwx, cwy
    end

    if logs ~= nil and core.typeof(logs) == "FileObject" then
        local x, y = GetApplicationMouse()
        local message = string.format("%g, %g", x, y)
        if message and logs:Read() ~= message then logs:Write(message) end
        self.mouse.x, self.mouse.y = x, y
    end

    ---@return boolean
    local function MouseMatchesHitbox()
        return false -- default : TODO
    end

    if self._events ~= nil and type(self._events) == "table" then
        for _, SelectedEvent in ipairs(self._events or {}) do
            if SelectedEvent and MouseMatchesHitbox() then
                SelectedEvent:Fire(false)
            end
        end
    end

    self.button:Render(game)
    self.label:Render(game)

    return nil
end

local TestButton = ButtonInstancer.new("test", { x = 100, y = 100 }, "TEST", function(name, action, key)
    
end)

local function TickGame()
    if game ~= nil then
        ExitEvent:Fire(game:IsRunning() or false)
        TestButton:DisplayButton()
        InputService:UpdateAll()
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

InputService:SetGlobalInput(true)
runtime.Stepped:Connect(TickGame)
runtime:KeepAlive()
