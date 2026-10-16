local <const> core = require("max_core").call(); core:Load()
local WindowService <const> = core:LoadService("WindowService")
local RuntimeService <const> = core:LoadService("RunnerService")
local InputService <const> = core:LoadService("InputService")
local SoundService <const> = core:LoadService("SoundService")
local RandomValue = core.RandomClass.new(os.time())
local RandomColor = core.RandomClass.new(os.time())
local window = WindowService:CreateWindow("window")
local exit = core.Event.new("exit")

---@param s string
---@return table<string>
function string.split(s)
    local splitted = {}; for i = 1, #s do
        local ss = s:sub(i, i) or string.char(0)
        table.insert(splitted, ss)
    end; return splitted
end

---@param x number
---@param y number
---@return number, number
local function calculate_mouse_position(x, y)
    if not window or not core.IsA(window, "WindowObject") then return 0, 0 end
    local window_x <const>, window_y <const> = window:GetPosition()
    local converted_x, converted_y = x - window_x - 5, y - window_y - 30
    return converted_x, converted_y
end

if window and core.typeof(window) == "WindowObject" then
    if window:IsVisible() then
        window:ClearCanvas(0,0,0)
    end

    InputService:BindAction("draw-line", "mouse1", function(name, state, key)
        if type(state) ~= "string" or state ~= "Pressed" then return end
        local x, y = InputService:GetMousePosition()
        x, y = calculate_mouse_position(x, y)

        local message_length <const> = 10
        local created_message = string.char(0)

        for index = 1, message_length do
            RandomValue:SetSeed(os.time())
            created_message = created_message .. utf8.char(RandomValue:NextNumber(32, 64))
        end

        local object = window:CreateText("", x, y, 2)
        object:SetColor(RandomColor:NextColor())

        window:ClearCanvas(0, 0, 0)
        window:RenderObject(object)
    end)
end

---@param dt number
local connection = RuntimeService.Heartbeat:Connect(function(dt)
    if window == nil or not window:IsRunning() then
        exit:Fire()
        return
    end

    window:SwapBuffers()
    window:StepPhysics(dt)
    InputService:Update()
    RuntimeService:Pump(dt)
end)

RuntimeService:Spawn("Stepped", function(wait, join)
    RuntimeService.RenderStepped:Connect(function(dt)
        
    end)
end, { reentrant = true })

exit:Connect(function()
    core.colorPrint("INFO", "CLOSING...")
    os.exit(nil, true)
end)

io.stdout:setvbuf("line")
RuntimeService:KeepAlive()
