---@alias ButtonSourceCode fun(name: string, action: InputActionState, key: KeyName): nil
---@alias ButtonCoordinates { x: number, y: number }
local core <const> = require("max_core").call()
local StorageService = core:LoadService("StorageService")
local InputService = core:LoadService("InputService")
local window = core:LoadService("WindowService")
local runtime = core:LoadService("RunnerService")
local game = window:CreateWindow("Game Editor", 850, 800)
local NoiseGeneration = core.NoiseClass.new(os.time())
local ExitEvent = core.Event.new("exit")

if not game or not core.IsA(game, "WindowObject") then
    return 0x1 -- raise error
end

---@class ButtonObject
---@field new fun(name: string, pos: ButtonCoordinates, source: ButtonSourceCode): ButtonObject
---@field DisplayButton fun(self: ButtonObject): nil
---@field _events {[integer]: Event}
---@field hitbox ButtonCoordinates
---@field position ButtonCoordinates
---@field mouse ButtonCoordinates
---@field button PolygonObject
local ButtonInstancer = setmetatable({}, nil)
ButtonInstancer.__index = ButtonInstancer
core.InstanceType.SetType(ButtonInstancer, "ButtonObject")
ButtonInstancer.DefaultPoints = ({
    { 0, 0 }, { 20, 0 },
    { 20, 10 }, { 0, 10 },
});

---@return ButtonObject
function ButtonInstancer.new(name, pos, source)
    local mx, my = InputService:GetMousePosition()
    ---@type boolean
    local is_touching_button = false
    ---@type ButtonObject
    local self = setmetatable({}, ButtonInstancer)
    local defaults = ButtonInstancer.DefaultPoints
    self.button = game:CreatePolygon(defaults, true)
    self.button:SetPosition(pos.x, pos.y)
    self.position = pos or { x = 0, y = 0 }
    self.hitbox = { x = 0, y = 0 }
    self.mouse = { x = mx, y = my }
    self._events = table.create(0, 2)

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

    TouchingEvent:Connect(function(...)
        
    end)

    return self
end

function ButtonInstancer:DisplayButton()
    if not self then return end
    if not self.button then return end
    if not self._events then return end

    local cmx, cmy = InputService:GetMousePosition()

    if self._events ~= nil and type(self._events) == "table" then
        for _, event in ipairs(self._events or {}) do
            if core.IsA(event, "Event") and event._name == "TouchingEvent" then
                if self.mouse.x > cmx or self.mouse.x < cmx then
                    if self.mouse.y > cmy or self.mouse.y < cmy then
                        self.mouse.x, self.mouse.y = cmx, cmy; event:Fire()
                    end
                end
            end
        end
    end

    self.button:Render(game)

    return nil
end

local TestButton = ButtonInstancer.new("test", { x = 100, y = 100 }, function(name, action, key)
    
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
