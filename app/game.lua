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
windows.GameApplication:SetAliasingQuality("3d", 4)
windows.GameApplication:SetPhysicsAutoQuality(true)

if windows.GameApplication then
    game.physics = windows.GameApplication:CreatePhysicsWorld(0, 20.98, 0)
end

-- ---@alias DataValues { scope: string, value: string }
-- ---@param data { [string]: DataValues }
-- local function SaveData(data)
--     ---@type string
--     local FormattedSaveData = ""
--     ---@type CreateFileOptions
--     local config = { path = "./game_data.ini" }
--     ---@type FileObject?
--     local DataFile = StorageService:GetFile(config.path)

--     for property, value in pairs(data or {}) do
--         local combined = property .. " = " .. tostring(value.value) .. "\n"
--         local content = FormattedSaveData .. combined
--         FormattedSaveData = content
--     end

--     if not DataFile or not core.IsA(DataFile, "FileObject") then DataFile = StorageService:CreateFile(config) end
--     if DataFile ~= nil and core.IsA(DataFile, "FileObject") then DataFile:Write(FormattedSaveData) end
-- end

---@param pos table<number>
---@return GameObject?
local function CreateCollectableCoin(pos)
    if core.typeof(windows.GameApplication) ~= "WindowObject" then warn("Game Application NIL!") return nil end
    if pos == nil or type(pos) ~= "table" or #pos ~= 3 then warn("Insufficent Position Amount!") return nil end
    local GameApplication <const> = windows.GameApplication
    if not GameApplication then return nil end

    local CoinMeshConfiguration = {
        vertices = {
            -- Front Face Center & Ring (Z = +0.25)
            {  0.000,  0.000,  0.25 }, -- 1: Front Center
            {  1.000,  0.000,  0.25 }, -- 2: Right
            {  0.707,  0.707,  0.25 }, -- 3: Top-Right
            {  0.000,  1.000,  0.25 }, -- 4: Top
            { -0.707,  0.707,  0.25 }, -- 5: Top-Left
            { -1.000,  0.000,  0.25 }, -- 6: Left
            { -0.707, -0.707,  0.25 }, -- 7: Bottom-Left
            {  0.000, -1.000,  0.25 }, -- 8: Bottom
            {  0.707, -0.707,  0.25 }, -- 9: Bottom-Right

            -- Back Face Center & Ring (Z = -0.25)
            {  0.000,  0.000, -0.25 }, -- 10: Back Center
            {  1.000,  0.000, -0.25 }, -- 11: Right
            {  0.707,  0.707, -0.25 }, -- 12: Top-Right
            {  0.000,  1.000, -0.25 }, -- 13: Top
            { -0.707,  0.707, -0.25 }, -- 14: Top-Left
            { -1.000,  0.000, -0.25 }, -- 15: Left
            { -0.707, -0.707, -0.25 }, -- 16: Bottom-Left
            {  0.000, -1.000, -0.25 }, -- 17: Bottom
            {  0.707, -0.707, -0.25 }, -- 18: Bottom-Right
        },
        faces = {
            -- Front Cap (8 Triangles around front center #1)
            { 1, 2, 3 }, { 1, 3, 4 }, { 1, 4, 5 }, { 1, 5, 6 },
            { 1, 6, 7 }, { 1, 7, 8 }, { 1, 8, 9 }, { 1, 9, 2 },

            -- Back Cap (8 Triangles around back center #10)
            { 10, 12, 11 }, { 10, 13, 12 }, { 10, 14, 13 }, { 10, 15, 14 },
            { 10, 16, 15 }, { 10, 17, 16 }, { 10, 18, 17 }, { 10, 11, 18 },

            -- 8 Outer Sides (16 Triangles, outward normals)
            { 2, 11, 12 }, { 2, 12, 3 },   -- Side 1
            { 3, 12, 13 }, { 3, 13, 4 },   -- Side 2
            { 4, 13, 14 }, { 4, 14, 5 },   -- Side 3
            { 5, 14, 15 }, { 5, 15, 6 },   -- Side 4
            { 6, 15, 16 }, { 6, 16, 7 },   -- Side 5
            { 7, 16, 17 }, { 7, 17, 8 },   -- Side 6
            { 8, 17, 18 }, { 8, 18, 9 },   -- Side 7
            { 9, 18, 11 }, { 9, 11, 2 },   -- Side 8
        },
    };

    local CoinMesh = GameApplication:CreateMesh()
    CoinMesh:SetVertices(CoinMeshConfiguration.vertices)
    CoinMesh:SetFaces(CoinMeshConfiguration.faces)
    CoinMesh:SetPosition(table.unpack(pos))
    CoinMesh:SetScale(0.5, 0.5, 0.25)
    
    for _, v in pairs(CoinMesh.Vertices) do
        for _, vv in ipairs(v) do
            print(vv)
        end
    end

    return CoinMesh
end

local CoinObject = CreateCollectableCoin({5, -2, 0})
if not CoinObject or type(CoinObject) == "nil" then return end
local CoinPhysics = windows.GameApplication:BindPhysics(CoinObject)
if not CoinPhysics or core.typeof(CoinPhysics) ~= "PhysicsBody" then return end
local MainCamera = windows.GameApplication:CreateCamera()
local LightSource = windows.GameApplication:CreateLight()
local Floor = windows.GameApplication:CreateCube(0, 0, 0)
local Object = windows.GameApplication:CreateCube(0, -2, 0)
local PhysicsCube = windows.GameApplication:CreateCube(-5, -5, 0)
local PhysicsObjectWire = windows.GameApplication:BindPhysics(PhysicsCube)
local FloorWire = windows.GameApplication:BindPhysics(Floor)
local ObjWire = windows.GameApplication:BindPhysics(Object)
local CoinCount = 0

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
    windows.GameApplication:SetActiveCamera(MainCamera); MainCamera:SetFOV(115)
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

Floor:SetFillMode("solid")
Object:SetFillMode("wireframe")
PhysicsCube:SetFillMode("solid")
ObjWire:SetDamping(0.2)
PhysicsObjectWire:SetMass(20)
PhysicsObjectWire:SetRestitution(0)
PhysicsObjectWire:SetFriction(1)
Object:SetColor(100, 100, 100)
Floor:SetColor(200, 200, 200)
LightSource:SetIntensity(0.5)
FloorWire:SetFriction(1.0)
ObjWire:SetFriction(1.0)
Floor:SetScale(20, 1, 5)
Object:SetScale(1, 1, 1)
ObjWire:SetFriction(1)

ObjWire:SetRotationLocked(false)
-- PhysicsObjectWire:SetRotationLocked(true)

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

---@alias SlopeFetchOutput { sx: number, sy: number, sz: number }

---@class RaycastObject
---@field new fun(opts: RaycastParameters)
---@field origin RaycastCoordinates
---@field target RaycastCoordinates
---@field tester MeshObject
---@field GetSlopeValue fun(selfObj: RaycastObject): SlopeFetchOutput
---@field Synthesis fun(selfObj: RaycastObject): nil

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

--- fetches slope increment value for _`RaycastObject`_ pointing direction.
---@param self RaycastObject
---@return SlopeFetchOutput
function RaycastUtility:GetSlopeValue()
    if not self then return {} end

    local distance = VectorUtility.VectorMath.GetDistance3D({
        x1 = self.origin.x, x2 = self.target.x,
        y1 = self.origin.y, y2 = self.target.y,
        z1 = self.origin.z, z2 = self.target.z,
    });

    local distance_x = self.target.x - self.origin.x
    local distance_y = self.target.y - self.origin.y
    local distance_z = self.target.z - self.origin.z

    local slope_x <const> = distance_x / distance
    local slope_y <const> = distance_y / distance
    local slope_z <const> = distance_z / distance

    return { sx = slope_x, sy = slope_y, sz = slope_z }
end

---@param self RaycastObject
function RaycastUtility:Synthesis()
    for i, OriginCoordinateValue in pairs(self.origin) do
        if self.target[i] < OriginCoordinateValue then goto continue else return end
        if self.target[i] > OriginCoordinateValue then goto continue else return end
        goto unsafe ::continue:: local TesterPositions = table.pack(self.tester:GetPosition())
        -- for PosIndex = 0, self
    end ::unsafe:: return nil
end

---@param self RaycastObject
function RaycastUtility:GetOrigin()
    return self.origin or { x = 0, y = 0, z = 0 }
end

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

---@param object GameObject
CoinObject.Collided:Connect(function(object)
    core.task.seed(function()
        CoinPhysics:SetEnabled(false)
        CoinObject.Visible = false
        CoinCount = CoinCount + 1
        print("COINS:", CoinCount)
    end)
end)

if not CoinPhysics:IsStatic() then
    CoinPhysics:SetStatic(true)
end

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
        PhysicsObjectWire:PredictImpact(ObjWire, dt)
        ObjWire:PredictPosition(dt)

        PhysicsObjectWire:ApplyTorque(0, 0, 0)
        print(PhysicsObjectWire:GetAngularVelocity())

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
        CoinObject:Render(windows.GameApplication)
        CoinObject:SetRotation(0, CoinObject.Rotation.y + (20 * dt), 0)

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
end, { priority = 115, safe = true, maxFails = math.huge, maxCatchUp = 0.5 });

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

local ProgramExit = core.Event.new("ExitProgramCleanup")

---@param dt number
local function tick(dt)
    if windows["GameApplication"] ~= nil then
        if not windows.GameApplication:IsRunning() then
            ProgramExit:Fire()
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
ProgramExit:Connect(function()
    -- "%[([^%]]+)%]"
    windows.GameApplication:Close()
    core.colorPrint("DEBUG", "EXITING...")
    os.exit(0x00000, true); return nil
end); RuntimeService:KeepAlive()
