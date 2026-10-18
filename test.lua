local <const> platform = require("utils.platform")
local <const> VectorMath = require("utils.vector_math.vector_utils")
local <const> core = require("max_core").call(); core:Load()
local <const> StorageService = core:LoadService("StorageService")
local <const> WindowService = core:LoadService("WindowService")
local <const> RuntimeService = core:LoadService("RunnerService")
local <const> InputService = core:LoadService("InputService")
local <const> SoundService = core:LoadService("SoundService")
local WindowElement = WindowService:CreateWindow("window", 1280, 1080)
local RandomValues = core.RandomClass.new(os.time())
if not WindowElement then return end
local exit = core.Event.new("exit")

local PhysicsWorld = WindowElement:CreatePhysicsWorld(0, 9.81, 0)
local AliasingQaulityEffective = WindowElement:SetAliasingQuality("3d", 4.0)
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
WorldGameFloor:SetScale(-10, 0.5, 10)
WorldGameFloor:SetColor(255, 255, 255)

---@type {[1]: number, [2]: number, [3]: number}[]
local PlayerModelConfiguration = {
    { 0, 0, 0 }, { -5, -5, 0 }, { -10, 0, 0 },
};

local PlayerModel = WindowElement:CreateMesh()
PlayerModel:SetVertices(PlayerModelConfiguration)
PlayerModel:SetFaces({{1}, {1}, {1}})
PlayerModel:SetPosition(0, -10, 0)
PlayerModel:SetFillMode("point")

local GameCamera = WindowElement:CreateCamera(0, -10, 20, 90)
if GameCamera ~= nil then WindowElement:SetActiveCamera(GameCamera) end
local MainLight = WindowElement:CreateLight(nil, 10, nil, 0.25, 1.0)
if MainLight ~= nil then WindowElement:SetActiveLight(MainLight) end

RuntimeService.RenderStepped:Connect(function(dt, wait, _)
    if WindowElement == nil or not WindowElement:IsRunning() then
        exit:Fire(); return
    end

    local UpdateDelta = wait(dt)

    WindowElement:SwapBuffers()
    WindowElement:ClearCanvas()
    WindowElement:RenderObject(WorldGameFloor)
    WindowElement:RenderObject(PlayerModel)
    WindowElement:StepPhysics(UpdateDelta)

    if WindowElement:IsFocused() ~= InputService:GetGlobalInput() then
        InputService:SetGlobalInput(WindowElement:IsFocused() or false)
    end

    InputService:Update(); collectgarbage("collect")
end, { yieldable = true, maxFails = math.huge, tag = "main" })

RuntimeService:Spawn("Heartbeat", function(wait, join)
    while true and type(wait) == "function" do
        local DeltaTime = wait()
        local memory = platform:get_memory_info()
        local in_use = memory["total_mb"] - memory["available_mb"]
        -- print(in_use .. " MB / " .. memory["total_mb"] .. " MB")
        -- print("DELTA:\t", DeltaTime)
    end
end, { safe = true, reentrant = true, key = "inline-main" })

exit:Connect(function()
    core.colorPrint("INFO", "CLOSING...")
    if PhysicsWorld then PhysicsWorld:destroy() end
    RuntimeService:destroy(); os.exit(true, true)
end)

-- for i, v in pairs(RuntimeService._jobs) do
--     for ii, vv in ipairs(v) do
--         vv = vv --- @type RunnerJob

--         for key, job_value in pairs(vv) do
--             print(key, job_value)
--         end
--     end
-- end

io.stdout:setvbuf("line")
RuntimeService:Pump(0.02)
RuntimeService:KeepAlive()
