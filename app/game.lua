local core <const> = require("max_core").call()
local VectorUtility = require("utils.vector_math.vector_utils")
local SoundService = core:LoadService("SoundService")
local InputService = core:LoadService("InputService")
local WindowService = core:LoadService("WindowService")
local RuntimeService = core:LoadService("RunnerService")
local StorageService = core:LoadService("StorageService")
local RaycastUtility = {}
RaycastUtility._private = {}

SoundService:SetStorageService(StorageService)
SoundService:SetCacheFolder("../audio")

-- if MusicSoundObject and core.typeof(MusicSoundObject) == "SoundObject" then
--     if not MusicSoundObject:IsPlaying() then
--         MusicSoundObject:Play()
--     end
-- end

---@class GameDataModel
---@field physics PhysicsWorld|nil?
---@field objects { PhysicBodies: PhysicsBody, StaticObjects: GameObject }
---@field world table<Instance>
---@field events table<Event>

local windows = {
    GameApplication = nil,
};

---@type GameDataModel
local game = {
    physics = nil,
    objects = {},
    events = {},
    world = {},
};

windows["GameApplication"] =
WindowService:CreateWindow("Engine", 850, 800)
windows.GameApplication:SetAliasingQuality("2d", 2)
windows.GameApplication:SetAliasingQuality("3d", 1)
windows.GameApplication:SetPhysicsAutoQuality(true)

if windows.GameApplication then
    game.physics = windows.GameApplication:CreatePhysicsWorld(0, 9.81, 0)
end

local MainCamera = windows.GameApplication:CreateCamera()
local LightSource = windows.GameApplication:CreateLight()
local Floor = windows.GameApplication:CreateCube(0, 0, 0)
local Object = windows.GameApplication:CreateCube(0, -5, 0)
local PhysicsCube = windows.GameApplication:CreateCube(-5, -2, 0)
local PhysicsObjectWire = windows.GameApplication:BindPhysics(PhysicsCube)
local FloorWire = windows.GameApplication:BindPhysics(Floor)
local ObjWire = windows.GameApplication:BindPhysics(Object)

---@class PlatformConfig
---@field size table<number>
---@field position table<number>
---@type table<PlatformConfig>
local PlatformCoordinates = {
    [1] = {
        position = { -5, -4, 0 },
        size = { 4, 0.5, 2 },
    },
    [2] = {
        position = { -1, -7, 0 },
        size = { 3.5, 0.5, 2 },
    },
    [3] = {
        position = { 6, -15, 0 },
        size = { 4.25, 0.5, 2 },
    },
};

local function CreatePlatforms()
    local rng = core.RandomClass.new(os.time())
    ---@type table<integer, CubeObject>
    local InstancedPlatforms = {}

    for _, PlatformConfiguration in ipairs(PlatformCoordinates) do
        local PlatformInstance = windows.GameApplication:CreateCube(table.unpack(PlatformConfiguration.position))
        local PlatformPhysics = windows.GameApplication:BindPhysics(PlatformInstance, { isStatic = true })
        PlatformInstance:SetScale(table.unpack(PlatformConfiguration.size))
        if not PlatformPhysics then goto skip_platform_config end
        print("OK:\t", PlatformInstance, "\t", PlatformPhysics)
        table.insert(InstancedPlatforms, PlatformInstance)
        local PIS = table.pack(PlatformInstance:GetScale())
        local ShapeBox = { PIS[1] / 2, PIS[2] / 2, PIS[3] / 2 }
        PlatformPhysics:SetShapeBox(table.unpack(ShapeBox))
        PlatformInstance:SetColor(255, 255, 255)
        PlatformInstance:SetFillMode("solid")
        ::skip_platform_config::
    end

    ---@param dt number
    RuntimeService.RenderStepped:Connect(function(dt)
        for _, SelectedPlatform in ipairs(InstancedPlatforms) do
            Floor:Render(windows.GameApplication)
            SelectedPlatform:Render(windows.GameApplication)
            PhysicsCube:Render(windows.GameApplication)
            Object:Render(windows.GameApplication)
        end
    end)
end

if MainCamera ~= nil and core.typeof(MainCamera) == "CameraObject" then
    windows.GameApplication:SetActiveCamera(MainCamera); MainCamera:SetFOV(70)
    -- MainCamera:SetClipPlanes(1, 50)
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
Floor:SetColor(200, 200, 200)
FloorWire:SetFriction(1.0)
ObjWire:SetFriction(1.0)
Floor:SetScale(20, 1, 5)
Object:SetScale(1, 1, 1)
ObjWire:SetFriction(1)

local LightIntensityInitial = LightSource and LightSource:GetIntensity()
local wx, wy = windows.GameApplication:GetDimensions() or 0, 0
---@type table<integer, PhysicsBody>
local objects = table.pack(FloorWire, ObjWire)
game.physics:SetQualityThresholds(60, 50, 45)
game.physics:SetAutoQuality(true)

---@return RaycastTesterObject
function RaycastUtility._private.CreateRaycastTester()
    ---@class RaycastTesterObject : MeshObject
    ---@field TestIntersection fun(): boolean
    local InitialObject = windows.GameApplication:CreateMesh()

    InitialObject.TestIntersection = function()
        return false -- default value for now; TODO!
    end

    return InitialObject
end

---@class RaycastObject
---@field new fun(opts: RaycastParameters)
---@field origin RaycastCoordinates
---@field target RaycastCoordinates
---@field tester MeshObject

---@class RaycastParameters
---@field origin RaycastCoordinates
---@field target RaycastCoordinates
---@alias RaycastCoordinates { x: number, y: number, z: number }
---@param opts RaycastParameters
---@return RaycastObject
function RaycastUtility.new(opts)
    ---@type RaycastObject
    local self = setmetatable({}, RaycastUtility)
    core.InstanceType.SetType(self, "RaycastObject")
    self.tester = windows and windows.GameApplication:CreateMesh()
    local TesterPhysics = windows.GameApplication:BindPhysics(self.tester)
    if not TesterPhysics then return setmetatable({}, RaycastUtility) end
    if not TesterPhysics:IsStatic() then TesterPhysics:SetStatic(true) end

    for ParameterName, ParameterValues in pairs(opts or {}) do
        if ParameterValues ~= nil and type(ParameterValues) == "table" then
            for SubParamName, SubParamValue in pairs(ParameterValues) do
                self[ParameterName] = table and table.create(3) or {}
                self[ParameterName][SubParamName] = SubParamValue
            end
        end
    end

    return self
end

---@param self RaycastObject
function RaycastUtility:Synthesis()
    for i, OriginCoordinateValue in pairs(self.origin) do
        if self.target[i] < OriginCoordinateValue then goto continue else return end
        if self.target[i] > OriginCoordinateValue then goto continue else return end
        goto unsafe ::continue:: local TesterPositions = table.pack(self.tester:GetPosition())
    end ::unsafe:: return nil
end

---@param self RaycastObject
function RaycastUtility:GetOrigin()
    return self.origin or { x = 0, y = 0, z = 0 }
end

---@param v PhysicsBody
Floor.Collided:Connect(function(v)
    print("Floor COLLISION!")
end)

---@param self RaycastObject
function RaycastUtility:GetDistance()
    if self ~= nil and type(self) == "table" then
        return VectorUtility.VectorMath.GetDistance3D({
            x1 = self.origin.x, x2 = self.target.x,
            y1 = self.origin.y, y2 = self.target.y,
            z1 = self.origin.z, z2 = self.target.z,
        });
    end
end

function RaycastUtility:EndingPosition()
    local window <const> = windows.GameApplication
    if not window or core.typeof(window) ~= "WindowObject" then return end
    local physics = window and window:GetPhysicsWorld() or {}
    -- local intersection = window:GetPhysicsBody()
end

local ray = RaycastUtility.new({
    origin = { x = 0, y = 0, z = 0 },
    target = { x = 0, y = 0, z = -5 },
});

RuntimeService.RenderStepped:Connect(function(dt)
    if ObjWire ~= nil and core.typeof(ObjWire) == "PhysicsBody" then
        local fx <const>, fy <const>, fz <const> = Floor:GetScale()
        local osx <const>, osy <const>, osz <const> = Object:GetScale()
        local ox <const>, oy <const>, oz <const> = ObjWire:GetPosition()
        local cax, cay, caz = MainCamera:GetPosition()
        local mx, my = InputService:GetMousePosition()
        local dx, dy = InputService:GetMouseDelta()
        -- FloorWire:SetShapeBox(fx / 2, fy / 2, fz / 2)
        -- ObjWire:SetShapeBox(osx / 2, osy / 2, osz / 2)
        FloorWire:SetShapeBox(fx / 1.8, fy / 1.8, fz / 1.8)
        -- ObjWire:SetShapeBox(osx / 1.8, osy / 1.8, osz / 1.8)

        local PhysicsCubeSize = table.pack(PhysicsCube:GetScale())
        PhysicsObjectWire:SetShapeBox(PhysicsCubeSize[1] / 2, PhysicsCubeSize[2] / 2, PhysicsCubeSize[3] / 2)

        if LightSource:GetIntensity() ~= LightIntensityInitial then
            if LightIntensityInitial and type(LightIntensityInitial) == "number" then
                LightSource:SetIntensity(LightIntensityInitial)
            end
        end

        MainCamera:SetPosition(ox, oy - 2.75, 10)
        Floor:Render(windows.GameApplication)
        PhysicsCube:Render(windows.GameApplication)
        Object:Render(windows.GameApplication)

        if InputService:IsKeyDown("a") or InputService:IsKeyDown("left") then
            ObjWire:ApplyImpulse(-1, 0, 0)
        elseif InputService:IsKeyDown("d") or InputService:IsKeyDown("right") then
            ObjWire:ApplyImpulse(1, 0, 0)
        elseif InputService:IsKeyDown("w") then
            ObjWire:ApplyImpulse(0, 0, -1)
        elseif InputService:IsKeyDown("s") then
            ObjWire:ApplyImpulse(0, 0, 1)
        end
    end
end, { priority = 107, safe = true, maxFails = math.huge, maxCatchUp = 0.1 });

---@param state InputActionState
local function JumpObject(_, state, _)
    if state and state == "Pressed" then
        ObjWire:ApplyImpulse(0, -20, 0)
    end
end

---@param state InputActionState
local function FallObject(_, state, _)
    if state and state == "Pressed" then
        ObjWire:ApplyImpulse(0, 20, 0)
    end
end

InputService:BindAction("FallDownStandard", "c", FallObject)
InputService:BindAction("FallDownArrow", "down", FallObject)
InputService:BindAction("JumpObjectStandard", "space", JumpObject)
InputService:BindAction("JumpObjectArrow", "up", JumpObject)

if LightSource ~= nil then
    LightSource:SetAmbient(0.65); LightSource:SetDirection(-20, 20, 20)
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

        windows.GameApplication:StepPhysics(dt)
        windows.GameApplication:SetDimensions(windows.GameApplication:GetDimensions())
        windows.GameApplication:SwapBuffers()
        windows.GameApplication:ClearCanvas()
        InputService:UpdateAll()
    end
end

InputService:SetGlobalInput(true)
xpcall(CreatePlatforms, print)

---@type JobOptions
local TickConfiguration = { safe = true, maxFails = 1 }
RuntimeService.Stepped:Connect(tick, TickConfiguration)
RuntimeService:KeepAlive()
