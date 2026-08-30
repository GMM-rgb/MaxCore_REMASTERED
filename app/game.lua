local core <const> = require("max_core").call()
local InputService = core:LoadService("InputService")
local WindowService = core:LoadService("WindowService")
local RuntimeService = core:LoadService("RunnerService")

---@class GameDataModel
---@field physics PhysicsWorld?
---@field objects { PhysicBodies: PhysicsBody, StaticObjects: GameObject }

local windows = {
    GameApplication = nil,
};

---@type GameDataModel
local game = {
    physics = nil,
    objects = {},
};

windows["GameApplication"] =
WindowService:CreateWindow("Engine", 850, 800)
windows.GameApplication:SetAliasingQuality("2d", 4)
windows.GameApplication:SetAliasingQuality("3d", 2)
windows.GameApplication:SetPhysicsAutoQuality(true)

if windows.GameApplication then
    game.physics = windows.GameApplication:CreatePhysicsWorld(0, 28.5, 0)
end

local MainCamera = windows.GameApplication:CreateCamera()
local LightSource = windows.GameApplication:CreateLight()
local Floor = windows.GameApplication:CreateCube(0, 0, -5)
local Object = windows.GameApplication:CreateCube(0, -5, -5)
local PhysicsCube = windows.GameApplication:CreateCube(-5, -2, -5)
local PhysicsObjectWire = windows.GameApplication:BindPhysics(PhysicsCube)
local FloorWire = windows.GameApplication:BindPhysics(Floor)
local ObjWire = windows.GameApplication:BindPhysics(Object)

if MainCamera ~= nil and core.typeof(MainCamera) == "CameraObject" then
    windows.GameApplication:SetActiveCamera(MainCamera); MainCamera:SetFOV(70)
    MainCamera:SetClipPlanes(1, 50)
    MainCamera:SetPosition(0, -5, 5)
    MainCamera:SetRotation(-135, 0, 0)
end

if not PhysicsObjectWire then return end
if not PhysicsCube then return end
if not LightSource then return end
if not MainCamera then return end
if not FloorWire then return end
if not ObjWire then return end

PhysicsCube:SetFillMode("solid")
PhysicsObjectWire:SetFriction(0.5)
Object:SetColor(100, 100, 100)
LightSource:SetIntensity(0.5)
Object:SetFillMode("solid")
Floor:SetFillMode("solid")
FloorWire:SetFriction(1.0)
ObjWire:SetFriction(1.0)
Floor:SetScale(20, 0, 5)
Object:SetScale(1, 1, 1)

local IsPlayerDead = false
local LightIntensityInitial = LightSource and LightSource:GetIntensity()
local wx, wy = windows.GameApplication:GetDimensions() or 0, 0
---@type table<integer, PhysicsBody>
local objects = table.pack(FloorWire, ObjWire)
local GameOverText = windows.GameApplication:CreateText()
GameOverText:SetText("DANG - YOU'RE DEAD!")
GameOverText:SetPosition(wx / 7, 15, 0)
GameOverText:SetColor(255, 0, 0)
GameOverText:SetScale(4, 4, 4)
GameOverText:SetQuality(4)

RuntimeService.RenderStepped:Connect(function(dt)
    if ObjWire ~= nil and core.typeof(ObjWire) == "PhysicsBody" then
        local fx <const>, fy <const>, fz <const> = Floor:GetScale()
        local osx <const>, osy <const>, osz <const> = Object:GetScale()
        local ox <const>, oy <const>, oz <const> = ObjWire:GetPosition()
        local cax, cay, caz = MainCamera:GetPosition()
        local mx, my = InputService:GetMousePosition()
        local dx, dy = InputService:GetMouseDelta()
        FloorWire:SetShapeBox(fx / 2, fy / 2, fz / 2)
        ObjWire:SetShapeBox(osx / 2, osy / 2, osz / 2)
        ObjWire:PredictImpact(FloorWire, dt, 16)
        local PhysicsCubeSize = table.pack(PhysicsCube:GetScale())
        PhysicsObjectWire:SetShapeBox(PhysicsCubeSize[1] / 2, PhysicsCubeSize[2] / 2, PhysicsCubeSize[3] / 2)

        -- for i = 1, math.ceil(math.abs(#objects)) do
        --     io.stdout:write("OBJECT_ID:\t" .. tostring(objects[i]:GetId()) .. "\n")
        -- end

        if LightSource:GetIntensity() ~= LightIntensityInitial then
            if LightIntensityInitial and type(LightIntensityInitial) == "number" then
                LightSource:SetIntensity(LightIntensityInitial)
            end
        end

        MainCamera:SetPosition(ox, oy - 2.75, caz)
        Floor:Render(windows.GameApplication)
        PhysicsCube:Render(windows.GameApplication)
        Object:Render(windows.GameApplication)

        if InputService:IsKeyDown("a") or InputService:IsKeyDown("left") then
            ObjWire:ApplyImpulse(-1, 0, 0)
        elseif InputService:IsKeyDown("d") or InputService:IsKeyDown("right") then
            ObjWire:ApplyImpulse(1, 0, 0)
        end

        -- if type(oy) == "number" and oy >= -1 then
        --     GameOverText:Render(windows.GameApplication)
        --     LightSource:SetIntensity(LightSource:GetIntensity() / 2)
        --     IsPlayerDead = true
        -- end
    end
end, { priority = 107, safe = true, maxFails = math.huge, maxCatchUp = 0.2 });

---@param state InputActionState
local function JumpObject(_, state, _)
    if state and state == "Pressed" then
        ObjWire:ApplyImpulse(0, -20, 0)
    end
end

InputService:BindAction("JumpObjectStandard", "space", JumpObject)
InputService:BindAction("JumpObjectArrow", "up", JumpObject)

if LightSource ~= nil then
    LightSource:SetAmbient(0.65); LightSource:SetDirection(20, 20, 20)
    windows.GameApplication:SetActiveLight(LightSource)
end

if FloorWire ~= nil then
    if not FloorWire:IsStatic() then
        FloorWire:SetStatic(true)
    end
end

---@param dt number
local function tick(dt)
    if windows["GameApplication"] ~= nil then
        if not windows.GameApplication:IsRunning() then
            return
        end

        -- if type(IsPlayerDead) == "boolean" and not IsPlayerDead then
            windows.GameApplication:StepPhysics(dt)
        -- end

        windows.GameApplication:SetDimensions(windows.GameApplication:GetDimensions())
        windows.GameApplication:SwapBuffers()
        windows.GameApplication:ClearCanvas()
        InputService:UpdateAll()
    end
end

-- local ok, obj = pcall(InstanceWorkplane)
-- if not ok or obj == nil then return end
-- obj:Render(windows.GameApplication)

InputService:SetGlobalInput(true)

---@type JobOptions
local TickConfiguration = { safe = true, maxFails = 1 }
RuntimeService.Stepped:Connect(tick, TickConfiguration)
RuntimeService:KeepAlive()
