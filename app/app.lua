local core <const> = require("max_core").call()
local storage = core:LoadService("StorageService")
local runtime = core:LoadService("RunnerService")
local window = core:LoadService("WindowService")
local sound = core:LoadService("SoundService")
local input = core:LoadService("InputService")
local click = core.Event.new("PlayerClickEvent")
local app = window:CreateWindow("APP - GAME", 800, 800)

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
    { 0,   0 }, { 550, 0 },
    { 520, 50 }, { 0, 50 },
};

coroutine.wrap(function(...)
    if app ~= nil and core.typeof(app) == "WindowObject" then
        local args = { ... }
        local dc <const> = 255
        local ct <const> = { dc, dc, dc }
        local cx <const>, cy <const> = app:GetDimensions()
        local obj = app:CreateCircle((cx / 2) - 50, (cy / 2) - 50, 100, true, table.unpack(ct))
        local toolbar = app:CreatePolygon(BorderElementPoints, true, 171, 212, 161)
        local ObjectPosChange = core.Event.new("ObjectPositionChanged")
        local noiseGen = core.NoiseClass.new(os.time())
        local tooltip = app:CreateText()
        local textpos = { 10, 17.5 }
        local TimeCounter = 0

        -- local cube = app:CreateCube(0, 0, -20)
        -- cube:SetScale(5, 5, 5)
        -- cube:Render(app)
        -- toolbar:Render(app)

        tooltip:SetPosition(table.unpack(textpos))
        tooltip:SetText("INFO:\tSpinning Cube YES :D")
        tooltip:SetColor(54, 54, 59)
        tooltip:SetScale(2, 2)
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

        local CubeConfiguration <const> = {
            positions = {
                { 0, 0, -15 }, { -10, 0, -35 },
                { 15, 0, -25 }, { 10, 5, -30 },
            },
            sizes = {
                3.25, 5.65,
                5, 2,
            },
        };

        local ROTATION_SPEED = 1
        local ProjectedCubes = {}

        for CubeInstanceIndex = 1, #CubeConfiguration.positions do
            local InstancedCube = app and app:CreateCube(nil, nil, nil, CubeConfiguration.sizes[CubeInstanceIndex])
            InstancedCube:SetPosition(table.unpack(CubeConfiguration.positions[CubeInstanceIndex]))
            InstancedCube:SetColor(core.RandomClass.new(os.time() + math.random(100)):NextColor())
            table.insert(ProjectedCubes, InstancedCube)
        end

        -- local MAXIMUM_COLOR_VALUE <const> = 255

        local CubeRuntimeConnection = runtime.RenderStepped:Connect(function(dt)
            if app and type(app.ClearCanvas) == "function" then
                app:ClearCanvas()
            end

            local SelectedColorValue = {}

            for i = 1, #ProjectedCubes do
                ---@type CubeObject
                local cube = ProjectedCubes[i]
                local NewRotY = cube.Rotation.y + (ROTATION_SPEED * dt)
                local NewRotX = cube.Rotation.x + (ROTATION_SPEED * dt)
                local NewRotZ = cube.Rotation.z - (ROTATION_SPEED * dt)
                SelectedColorValue = table.pack(cube:GetColor())
                cube.Color.r = cube.Color.r + core.RandomClass.new():NextInteger(-5, 5)
                cube.Color.g = cube.Color.g + core.RandomClass.new():NextInteger(-5, 5)
                cube:SetRotation(NewRotX, NewRotY, NewRotZ)
                pcall(cube.Render, cube, app)
            end

            if toolbar then toolbar:Render(app) end
            if tooltip then tooltip:Render(app) end
        end)

        -- runtime.RenderStepped:Connect(function()
        --     local currentTime = os and os.clock()
        --     local deltaTime = currentTime - LAST_TIME
        --     LAST_TIME = currentTime

        --     if type(app.ClearCanvas) == "function" then
        --         if app then app:ClearCanvas() end
        --     end

        --     local NewRotY = cube.Rotation.y + (ROTATION_SPEED * deltaTime)
        --     local NewRotX = cube.Rotation.x - (ROTATION_SPEED * deltaTime)
        --     cube:SetRotation(NewRotX, NewRotY, 0)

        --     toolbar:Render(app)
        --     tooltip:Render(app)
        --     cube:Render(app)
        -- end)
    end
end)(nil)

runtime.Stepped:Connect(function(dt)
    if app ~= nil and core.typeof(app) == "WindowObject" then
        if not app:IsRunning() then
            return
        else
            app:SwapBuffers()
        end

        if input:GetGlobalInput() ~= true then
            input:SetGlobalInput(true)
        else
            ChecksumGameData()
            input:UpdateAll()
        end
    end
end)

if app ~= nil then
    runtime:KeepAlive()
    app:Close()
end
