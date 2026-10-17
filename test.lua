local <const> platform = require("utils.platform")
local <const> core = require("max_core").call(); core:Load()
local <const> StorageService = core:LoadService("StorageService")
local <const> WindowService = core:LoadService("WindowService")
local <const> RuntimeService = core:LoadService("RunnerService")
local <const> InputService = core:LoadService("InputService")
local <const> SoundService = core:LoadService("SoundService")
local WindowElement = WindowService:CreateWindow("window")
local RandomValue = core.RandomClass.new(os.time())
local RandomColor = core.RandomClass.new(os.time())
local exit = core.Event.new("exit")

local StringModify = StorageService:GetFile("./services/sub_modules/lua_string_modify.lua"):Read()
if type(StringModify) ~= "string" then return end
load(StringModify, "string-modify", "bt")()

---@param x number
---@param y number
---@return number, number
local function calculate_mouse_position(x, y)
    if not WindowElement or not core.IsA(WindowElement, "WindowObject") then return 0, 0 end
    local window_x <const>, window_y <const> = WindowElement:GetPosition()
    local window_notch_height = platform and platform:get_title_bar_height() or 0
    local converted_x, converted_y = x - window_x - 7, y - window_y - window_notch_height - 7
    return converted_x, converted_y
end

if WindowElement and core.typeof(WindowElement) == "WindowObject" then
    WindowElement:SetAliasingQuality("2d", 3)
    WindowElement:ClearCanvas(0, 0, 0)

    InputService:BindAction("draw-line", "mouse1", function(name, state, key)
        if type(state) ~= "string" or state ~= "Pressed" then return end
        local x, y = InputService:GetMousePosition()
        x, y = calculate_mouse_position(x, y)

        local LaserLine = WindowElement:CreateLine(0, 0)
        LaserLine.EndPosition = { x = x, y = y }
        LaserLine:SetColor(RandomColor:NextColor())

        WindowElement:RenderObject(LaserLine)

        collectgarbage("step", 64)
    end)
end

---@param dt number
RuntimeService.Heartbeat:Connect(function(dt)
    if WindowElement == nil or not WindowElement:IsRunning() then
        exit:Fire(); return
    end

    WindowElement:SwapBuffers()
    WindowElement:StepPhysics(dt)

    if WindowElement:IsFocused() ~= InputService:GetGlobalInput() then
        InputService:SetGlobalInput(WindowElement:IsFocused())
    end

    InputService:Update()
end, { yieldable = true, maxFails = math.huge, tag = "main" })

RuntimeService:Spawn("Heartbeat", function(wait, join)
    while true and type(wait) == "function" do
        local DeltaTime = wait()
        collectgarbage("collect")
    end
end, { safe = true })

-- for i, v in pairs(RuntimeService._ready) do
--     print(i, v)
-- end

exit:Connect(function()
    core.colorPrint("INFO", "CLOSING...")
    os.exit(nil, true)
end)

io.stdout:setvbuf("line")
RuntimeService:Pump(0.02)
RuntimeService:KeepAlive()
