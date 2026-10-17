local <const> platform = require("utils.platform")
local <const> core = require("max_core").call(); core:Load()
local StorageService = core:LoadService("StorageService")
local WindowService <const> = core:LoadService("WindowService")
local RuntimeService <const> = core:LoadService("RunnerService")
local InputService <const> = core:LoadService("InputService")
local SoundService <const> = core:LoadService("SoundService")
local RandomValue = core.RandomClass.new(os.time())
local RandomColor = core.RandomClass.new(os.time())
local window = WindowService:CreateWindow("window")
local exit = core.Event.new("exit")

local StringModify = StorageService:GetFile("./services/sub_modules/lua_string_modify.lua"):Read()
if type(StringModify) ~= "string" then return end
load(StringModify, "string-modify", "bt")()

---@param x number
---@param y number
---@return number, number
local function calculate_mouse_position(x, y)
    if not window or not core.IsA(window, "WindowObject") then return 0, 0 end
    local window_x <const>, window_y <const> = window:GetPosition()
    local window_notch_height = platform:get_title_bar_height()
    local converted_x, converted_y = x - window_x - 7, y - window_y - window_notch_height - 7
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

        -- local object = window:CreateText("HI", x, y, 2)
        -- object:SetColor(RandomColor:NextColor())
        local object = window:CreateRect(x, y, 20, 20)

        window:ClearCanvas(0, 0, 0)
        window:RenderObject(object)
    end)
end

---@param dt number
RuntimeService.Heartbeat:Connect(function(dt)
    if window == nil or not window:IsRunning() then
        exit:Fire(); return
    end

    window:SwapBuffers()
    window:StepPhysics(dt)
    InputService:Update()
end)

RuntimeService:Spawn("Heartbeat", function(wait, join)
    while true and type(wait) == "function" do
        -- io.stdout:write("OK\n"); local dt = wait()
        -- io.stdout:write("DELTA:\t", dt, "\n")
        local dt = wait()
    end
end)

exit:Connect(function()
    core.colorPrint("INFO", "CLOSING...")
    os.exit(nil, true)
end)

io.stdout:setvbuf("line")
RuntimeService:Pump(0.02)
RuntimeService:KeepAlive()
