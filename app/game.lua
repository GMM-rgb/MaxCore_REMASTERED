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
WindowService:CreateWindow()

local FACE_AMOUNT = 1
local TARGET_FACES = 2
local WorkspaceFaces = {}

-- inserts the faces required for the workplane, etc.
for i = 1, TARGET_FACES do
    table.insert(WorkspaceFaces, FACE_AMOUNT)
end

if windows.GameApplication then
    game.physics = windows.GameApplication:CreatePhysicsWorld()
end

function InstanceWorkplane()
    if windows.GameApplication == nil then return nil end
    local WorkplaneMesh = windows.GameApplication:CreateMesh()
    -- windows.GameApplication:BindPhysics(WorkplaneMesh)
    WorkplaneMesh:SetFaces(WorkspaceFaces)
    WorkplaneMesh:SetFillMode("point")
    WorkplaneMesh:SetVertices({
        { -100, 0, 100 }, { 100, 0, 100 },
        { 100, 0, -100 }, { -100, 0 -100 },
    }); return WorkplaneMesh or nil
end

local function tick()
    if windows["GameApplication"] ~= nil and windows["GameApplication"]:IsRunning() then
        windows.GameApplication:SwapBuffers()
    end
end

local ok, obj = pcall(InstanceWorkplane)
if not ok or obj == nil then return end
obj:Render(windows.GameApplication)

---@type JobOptions
local TickConfiguration = { safe = true, maxFails = 1 }
RuntimeService.Stepped:Connect(tick, TickConfiguration)
RuntimeService:KeepAlive()
