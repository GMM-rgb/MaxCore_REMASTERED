local core <const> = require("max_core").call()
local InputService = core:LoadService("InputService")
local WindowService = core:LoadService("WindowService")
local RuntimeService = core:LoadService("RunnerService")

---@class GameDataModel
---@field physics PhysicsWorld?
---@field objects { PhysicBodies: PhysicsBody, StaticObjects: GameObject }

---@type { string: WindowObject }
local windows = {
    GameApplication = nil,
};

---@type GameDataModel
local game = {
    physics = nil,
    objects = {},
};

windows["GameApplication"] =
WindowService:CreateWindow("Engine", 900, 800)

windows.GameApplication:SetAliasingQuality("2d", 4)
windows.GameApplication:SetAliasingQuality("3d", 3)
windows.GameApplication:SetPhysicsAutoQuality(true)

if windows.GameApplication then
    game.physics = windows.GameApplication:CreatePhysicsWorld(nil, 19.81, nil)
end

local MainCamera = windows.GameApplication:CreateCamera()
local LightSource = windows.GameApplication:CreateLight()
local Floor = windows.GameApplication:CreateCube(0, 2, -5)
local Object = windows.GameApplication:CreateCube(0, -5, -5)
local FloorWire = windows.GameApplication:BindPhysics(Floor)
local ObjWire = windows.GameApplication:BindPhysics(Object)

if MainCamera ~= nil and core.typeof(MainCamera) == "CameraObject" then
    windows.GameApplication:SetActiveCamera(MainCamera); MainCamera:SetFOV(110)
    MainCamera:SetClipPlanes(1, 50)
    MainCamera:SetPosition(0, -2, 0)
    MainCamera:SetRotation(-135, 0, 0)
end

if not FloorWire then return end
if not ObjWire then return end

Object:SetColor(100, 100, 100)
Object:SetFillMode("solid")
Floor:SetFillMode("solid")
Floor:SetScale(5, 0, 5)
Object:SetScale(1, 1, 1)
FloorWire:SetMass(0)
ObjWire:SetMass(100)

local mx, my = InputService:GetMousePosition()
local dx, dy = InputService:GetMouseDelta()

RuntimeService.RenderStepped:Connect(function(dt)
    if ObjWire ~= nil and core.typeof(ObjWire) == "PhysicsBody" then
        -- ObjWire:ApplyForce(mx + (dx * dt), my - (dy * dt))
        local fx, fy, fz = Floor:GetScale()
        local ox, oy, oz = Object:GetScale()
        FloorWire:SetShapeBox(fx / 2, fy / 2, fz / 2)
        ObjWire:SetShapeBox(ox / 2, oy / 2, oz / 2)
        ObjWire:PredictPosition(dt)
    end
end)

if LightSource ~= nil then
    LightSource:SetAmbient(0.65); LightSource:SetDirection(-35, 0, -10)
    windows.GameApplication:SetActiveLight(LightSource)
end

if FloorWire ~= nil then
    if not FloorWire:IsStatic() then
        FloorWire:SetStatic(true)
    end
end

InputService:BindAction("RotatePlaneLeft", "a", function (name, state, key)
    
end)

InputService:BindAction("RotatePlaneRight", "d", function (name, state, key)
    if FloorWire and Floor and core.typeof(FloorWire) == "PhysicsBody" and core.typeof(Floor) == "CubeOject" then
        local FPR = table.pack(FloorWire:GetRotation())
        local FRR = table.pack(Floor:GetRotation())
        FloorWire:SetRotation(FPR[1] + 1, FPR[2], FPR[3])
        Floor:SetRotation(FRR[1] + 1, FRR[2], FRR[3])
    end
end)

---@param dt number
local function tick(dt)
    if windows["GameApplication"] ~= nil then
        if not windows.GameApplication:IsRunning() then
            return
        end

        windows.GameApplication:StepPhysics(dt)
        windows.GameApplication:SwapBuffers()
        windows.GameApplication:ClearCanvas()
        windows.GameApplication:RenderAll()
    end
end

-- local ok, obj = pcall(InstanceWorkplane)
-- if not ok or obj == nil then return end
-- obj:Render(windows.GameApplication)

---@type JobOptions
local TickConfiguration = { safe = true, maxFails = 1 }
RuntimeService.Stepped:Connect(tick, TickConfiguration)
RuntimeService:KeepAlive()
