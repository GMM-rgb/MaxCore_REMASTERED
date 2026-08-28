local core <const> = require("max_core").call()
local storage = core:LoadService("StorageService")
local runtime = core:LoadService("RunnerService")
local window = core:LoadService("WindowService")
local sound = core:LoadService("SoundService")
local input = core:LoadService("InputService")
local click = core.Event.new("PlayerClickEvent")
local app = window:CreateWindow("Engine", 800, 800)
local logs = window:CreateWindow("Logger", 400, 500)
local mpw = app and app:CreatePhysicsWorld()

sound:SetStorageService(storage)
sound:SetCacheFolder("../audio")

local GameData <const> = storage:CreateFile({
    path = "./data.txt", debug = true,
});

---@class AppData
---@field objects table<Instance, unknown>
---@field events table<Event>
local data = {
    objects = {
        ---@type table<SoundObject>
        sound = {},
        ---@type table<GameObject>
        shapes = {},
    },
    events = {},
};

if GameData ~= nil and core.typeof(GameData) == "FileObject" then

end

local function ChecksumGameData()
    local ErrorReason = string.char(0)
    for DataIndex, DataValue in pairs(data) do
        return -- TODO
    end
    io.stderr:write(string.format("[Game Debug]: fatal, checksum failed. \n [Reason]:\t%s", ErrorReason))
end

local BorderElementPoints = {
    { 0, 0 }, { 550, 0 },
    { 520, 50 }, { 0, 50 },
};

coroutine.wrap(function(...)
    if app ~= nil and core.typeof(app) == "WindowObject" then
        local args = { ... }
        local dc <const> = 255
        local ct <const> = { dc, dc, dc }
        local cx <const>, cy <const> = app:GetDimensions()
        local toolbar = app:CreatePolygon(BorderElementPoints, true, 171, 212, 161)
        local ObjectPosChange = core.Event.new("ObjectPositionChanged")
        local noiseGen = core.NoiseClass.new(os.time())
        local tooltip = app:CreateText()
        local textpos = { 10, 17.5 }
        local MeshVertices = {
            { 0, 0, -20 },
            { 0, 5, -20 },
            { 5, 5, -20 },
            { 5, 0, -20 },
        };

        app:SetAliasingQuality("3d", window:GetMaxAliasingQuality() - 2)

        if logs ~= nil and core.IsA(logs, "WindowObject") then
            local NewLogsPositionX <const>, NewLogsPositionY <const> = app:GetPosition()
            logs:SetPosition(NewLogsPositionX + table.pack(app:GetDimensions())[1], NewLogsPositionY)
        end

        local OriginPointMesh = app:CreateMesh()
        OriginPointMesh:SetFaces({{1}, {1}, {1}, {1}})
        OriginPointMesh:SetVertices(MeshVertices)
        OriginPointMesh:SetFillMode("point")
        OriginPointMesh:Render(app)

        local BindingPointMesh = app:BindPhysics(OriginPointMesh)

        runtime.RenderStepped:Connect(function(dt)
            if not BindingPointMesh then return end
            -- BindingPointMesh:PredictPosition(dt)
            BindingPointMesh:SetEnabled(true)
            BindingPointMesh:SetStatic(true)
        end)

        tooltip:SetPosition(table.unpack(textpos))
        tooltip:SetText("INFO:   3D Workspace Testing")
        tooltip:SetColor(54, 54, 59)
        tooltip:SetScale(2, 2)
        tooltip:SetQuality(4)
        tooltip:Render(app)

        ---@param changed GameObject
        ObjectPosChange:Connect(function(changed)
            TimeCounter = math.abs(TimeCounter) + 0.1

            local rawNoiseR = noiseGen:Sample2D(TimeCounter, 0)
            local rawNoiseG = noiseGen:Sample2D(0, TimeCounter)
            local rawNoiseB = noiseGen:Sample2D(TimeCounter, TimeCounter)
            local color_1 = math.floor(((rawNoiseR + 1) / 2) * 255)
            local color_2 = math.floor(((rawNoiseG + 1) / 2) * 255)
            local color_3 = math.floor(((rawNoiseB + 1) / 2) * 255)
            local packed <const> = { color_1, color_2, color_3 }
            local wx <const>, wy <const> = app:GetDimensions()
            local x = math.random(0, wx)
            local y = math.random(0, wy)

            changed:SetColor(table.unpack(packed))
            changed:SetPosition(x - 50, y - 50)
            changed:Render(app)
            toolbar:Render(app)
            tooltip:Render(app)
        end)

        -- input:BindAction("UpdateObjectPosition", "mouse1", function(name, state, key)
        --     if state ~= nil and obj ~= nil and state == "Pressed" then
        --         ObjectPosChange:Fire(obj)
        --     end
        -- end)

        ---@type integer[][]
        local WorkspacePlaneFaces = {}

        for i = 1, 8 do
            table.insert(WorkspacePlaneFaces, 1)
        end

        local CubeConfiguration <const> = {
            positions = {
                { 0, 0, -20 }, { -10, 0, -20 },
                { 15, 0, -2 }, { 10, 5, -12.2 },
                { 20, 10, -5 },
            },
            sizes = {
                4, 7,
                2, 4,
                8,
            },
        };

        local ROTATION_SPEED = 1
        ---@type table<CubeObject>
        local ProjectedCubes = {}
        ---@type { integer: PhysicsBody }
        local CubePhysicBodies = {}
        local MainLightSource = app:CreateLight(27, 20, -10, 0.45, 1)
        if MainLightSource == nil or type(MainLightSource) ~= "table" then return end
        if app then app:SetActiveLight(MainLightSource) end

        for CubeInstanceIndex = 1, #CubeConfiguration.positions do
            local InstancedCube = app:CreateCube(nil, nil, nil, CubeConfiguration.sizes[CubeInstanceIndex], nil, nil, nil, "solid")
            InstancedCube:SetPosition(table.unpack(CubeConfiguration.positions[CubeInstanceIndex]))
            InstancedCube:SetColor(core.RandomClass.new(os.time() + math.random(100)):NextColor())
            CubePhysicBodies[core.RandomClass.new():NextInteger(0, os.time())] = app:BindPhysics(InstancedCube)
            table.insert(ProjectedCubes, InstancedCube)
        end

        ---@param dt number
        local CubeRuntimeConnection = runtime.RenderStepped:Connect(function(dt)
            if app and type(app.ClearCanvas) == "function" then
                if app then app:ClearCanvas(0, 0, 0) end
            end

            for i = 1, #ProjectedCubes do
                ---@type CubeObject
                local cube = ProjectedCubes[i]
                local NewRotY = cube.Rotation.y + (ROTATION_SPEED * dt)
                -- local NewRotX = cube.Rotation.x + (ROTATION_SPEED * dt)
                -- local NewRotZ = cube.Rotation.z - (ROTATION_SPEED * dt)
                cube:SetRotation(0, NewRotY, 0)
                pcall(cube.Render, cube, app)
            end

            if toolbar then toolbar:Render(app) end
            if tooltip then tooltip:Render(app) end
        end)

        input:BindAction("ChangeCubeColors", "f", function(name, state, key)
            if state == "Pressed" then
                for _, cube in pairs(ProjectedCubes) do
                    local RandomGeneratorInstance = core.RandomClass.new()
                    cube:SetColor(RandomGeneratorInstance:NextColor())
                end
            end
        end)

        local DefaultCamera = app:CreateCamera(0, 0, 0, 115)
        if not DefaultCamera then return end
        DefaultCamera:SetClipPlanes(-20, 100)
        DefaultCamera:SetRotation(0, 0, 0)
        app:SetActiveCamera(DefaultCamera)
    end
end)(nil)

input:SetGlobalInput(true)

---@param deltaTime number
runtime.RenderStepped:Connect(function(deltaTime)
    if mpw ~= nil and core.typeof(mpw) == "PhysicsWorld" then
        mpw:Step(deltaTime)
    end
end)

runtime.Stepped:Connect(function(dt)
    if logs and core.typeof(logs) == "WindowObject" then
        if app ~= nil and core.typeof(app) == "WindowObject" then
            if not logs:IsRunning() then
                return
            else
                logs:SwapBuffers()
            end

            if not app:IsRunning() then
                return
            else
                app:SwapBuffers()
            end

            ChecksumGameData()
            input:UpdateAll()
        end
    end
end)

if app ~= nil then
    runtime:KeepAlive()
    app:Close()
end
