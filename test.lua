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
if not WindowElement then return end
local exit = core.Event.new("exit")

local PhysicsWorld = WindowElement:CreatePhysicsWorld(0, 9.81, 0)

local StringModify = StorageService:GetFile("./services/sub_modules/lua_string_modify.lua"):Read()
if type(StringModify) ~= "string" then return end
load(StringModify, "string-modify", "bt")()

---@param x number
---@param y number
---@return number, number
local function calculate_mouse_position(x, y)
    local window_x <const>, window_y <const> = WindowElement:GetPosition()
    local window_notch_height = platform and platform:get_title_bar_height()
    local converted_x, converted_y = x - window_x - 7, y - window_y - window_notch_height - 7
    return converted_x, converted_y
end

WorldGameFloor = WindowElement:CreateCube(0, 0, 0, 1)
WorldGameFloor:SetFillMode("solid")
WorldGameFloor:SetScale(10, 0.5, 10)
WorldGameFloor:SetColor(255, 255, 255)
WorldGameFloor:Render(WindowElement)

GameCamera = WindowElement:CreateCamera(0, 0, 10, 70)
if GameCamera ~= nil then WindowElement:SetActiveCamera(GameCamera) end

-- if WindowElement and core.typeof(WindowElement) == "WindowObject" then
--     WindowElement:SetAliasingQuality("2d", 3)
--     WindowElement:ClearCanvas(0, 0, 0)

--     InputService:BindAction("draw-line", "mouse1", function(name, state, key)
--         if type(state) ~= "string" or state ~= "Pressed" then return end
--         local x, y = InputService:GetMousePosition()
--         x, y = calculate_mouse_position(x, y)
--         RandomColor:SetSeed(os.time())

--         -- local LaserLine = WindowElement:CreateLine(0, 0)
--         -- LaserLine.EndPosition = { x = x, y = y }
--         -- LaserLine:SetColor(RandomColor:NextColor())

--         -- WindowElement:RenderObject(LaserLine)

--         RuntimeService:Spawn("Stepped", function(wait, join)
--             local CubeObject = WindowElement:CreateCube(x, y, -10, 2.0, RandomColor:NextColor())
--             local ROTATION_SPEED = math.rad(20.0); print "OK"

--             while true and CubeObject do
--                 local <const> dt = wait()
--                 CubeObject:SetFillMode("solid")
--                 CubeObject:SetRotation(CubeObject.Rotation.x, CubeObject.Rotation.y + (dt * ROTATION_SPEED), CubeObject.Rotation.z)
--                 WindowElement:RenderObject(CubeObject)
--             end
--         end)
--     end)
-- end

RuntimeService.Stepped:Connect(function(dt, wait, join)
    if WindowElement == nil or not WindowElement:IsRunning() then
        exit:Fire(); return
    end

    print(dt, wait, join)

    WindowElement:ClearCanvas()
    WindowElement:SwapBuffers()
    WorldGameFloor:Render(WindowElement)

    if WindowElement:IsFocused() ~= InputService:GetGlobalInput() then
        InputService:SetGlobalInput(WindowElement:IsFocused() or false)
    end

    InputService:Update(); collectgarbage("collect")
end, { yieldable = true, maxFails = math.huge, tag = "main" })

RuntimeService:Spawn("Heartbeat", function(wait, join)
    while true and type(wait) == "function" do
        local DeltaTime = wait()
        print("DELTA:\t", DeltaTime)
        WindowElement:StepPhysics(DeltaTime)
    end
end, { safe = true })

exit:Connect(function()
    core.colorPrint("INFO", "CLOSING...")
    os.exit(nil, true)
end)

io.stdout:setvbuf("line")
RuntimeService:Pump(0.02)
RuntimeService:KeepAlive()
