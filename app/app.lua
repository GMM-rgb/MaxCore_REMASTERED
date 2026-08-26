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

coroutine.wrap(function(...)
    if app ~= nil and core.typeof(app) == "WindowObject" then
        local args = { ... }
        local dc <const> = 255
        local ct <const> = { dc, dc, dc }
        local cx <const>, cy <const> = app:GetDimensions()
        local obj = app:CreateRect((cx / 2) - 50, (cy / 2) - 50, 100, 100, table.unpack(ct))
        local ObjectPosChange = core.Event.new("ObjectPositionChanged")
        local noiseGen = core.NoiseClass.new(os.time())
        local tooltip = app:CreateText()
        local textpos = { 10, 10 }
        local TimeCounter = 0

        tooltip:SetPosition(table.unpack(textpos))
        tooltip:SetText("INFO:\tClicking Creates Colored Squares")
        tooltip:SetColor(0, 255, 255)
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
            tooltip:Render(app)
        end)

        input:BindAction("UpdateObjectPosition", "mouse1", function(name, state, key)
            if state ~= nil and obj ~= nil and state == "Pressed" then
                ObjectPosChange:Fire(obj)
            end
        end)
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
