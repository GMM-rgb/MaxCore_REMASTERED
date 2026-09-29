---@alias ButtonSourceCode fun(name: string, action: InputActionState, key: KeyName): nil
---@alias HitboxCoordinates { ["one"]: { ["x1"]: number, ["y1"]: number }, ["two"]: { ["x2"]: number, ["y2"]: number }}
---@alias ButtonCoordinates { x: number, y: number }
local os <const> = require("os")
local io <const> = require("io")
local math <const> = require("math")
local table <const> = require("table")
local string <const> = require("string")
local core <const> = require("max_core").call()
local platform <const> = require("utils.platform")
local StorageService = core:LoadService("StorageService")
local InputService = core:LoadService("InputService")
local window = core:LoadService("WindowService")
local runtime = core:LoadService("RunnerService")
local sound = core:LoadService("SoundService")
local game = window:CreateWindow("Game Editor", 850, 800)
local logs = StorageService:CreateFile({ path = "./logs.log" })
local NoiseGeneration = core.NoiseClass.new(os.time())
local ExitEventObject = core.Event.new("exit")
local MachineName = platform:get_os_name()
local MachineArch = platform:get_arch()
local MachineInfo = platform:get_info()

sound:SetStorageService(StorageService)
sound:SetCacheFolder("audio_cache")

local TargetPath = "https://music.youtube.com/watch?v=8LShXs7yAC0&si=ZkQv-PFN1cy2MZ8p"
-- local TargetPath = "https://music.youtube.com/watch?v=IOym7Md8Hcw&si=pC83L1ssgOTbEUwW"
-- local TargetPath = "https://music.youtube.com/watch?v=e3OBPOKtMgA&si=X4oEGel88G-mHgLR"
local MusicAudio = sound:LoadSound(tostring(TargetPath) or "$PATH")
MusicAudio:SetLooping(true)
MusicAudio:SetVolume(0.85)
MusicAudio:SetPitch(1.0)

local AudioControls <const> = {
    [true] = MusicAudio.Pause,
    [false] = MusicAudio.Resume,
};

InputService:BindAction("toggle_music", "k", function(name, state, key)
    if state ~= nil and key ~= nil and state == "Pressed" and key == "k" then
        AudioControls[MusicAudio:IsPlaying()](MusicAudio)
    end
end)

local MachineDebugInfo = { MachineName, MachineArch, MachineInfo.is_64bit }
io.stdout:write(string.format("[MACHINE: %s] | [ARCH: %s] | [64-BIT: %s]", table.unpack(MachineDebugInfo)) .. "\n")
if not game or not core.IsA(game, "WindowObject") then return 0x1, error() end -- raise error
local choosen = game:ShowAlert("Play Music?", "Do you wanna play the music?", "question", "yesno")
if type(choosen) == "string" and choosen == "yes" then MusicAudio:Play() end

local DebuggerRuntime = runtime.Heartbeat:Connect(function()
    for _, file in ipairs(StorageService:ListDirectory("./")) do
        local DebugContents <const> = file:GetContents()

        local DumpedContents = string.dump(function(...)
            return DebugContents
        end, true)

        if logs ~= nil then
            local formatted = logs:Read()
            .. tostring(DumpedContents)
            logs:Write(formatted)
        end
    end
end, { safe = true, maxFails = 2, priority = 115 }); DebuggerRuntime:Pause()

---@class ButtonObject
---@field new fun(name: string, pos: ButtonCoordinates, text: string, source: ButtonSourceCode): ButtonObject
---@field DisplayButton fun(self: ButtonObject): nil
---@field _events {[integer]: Event}
---@field contains_cursor boolean
---@field hitbox HitboxCoordinates
---@field position ButtonCoordinates
---@field mouse ButtonCoordinates
---@field button PolygonObject
---@field label TextObject
---@field text string
local ButtonInstancer = setmetatable({}, nil)
core.InstanceType.SetType(ButtonInstancer, "ButtonObject")
ButtonInstancer.__index = ButtonInstancer or {}
ButtonInstancer.DefaultPoints = ({
    { 0, 0 }, { 20, 0 },
    { 20, 10 }, { 0, 10 },
});

---@return ButtonObject
function ButtonInstancer.new(name, pos, text, source)
    local mx, my = InputService:GetMousePosition()
    ---@type boolean
    local is_touching_button = false
    ---@type ButtonObject
    local self = setmetatable({}, ButtonInstancer)
    local defaults = ButtonInstancer.DefaultPoints
    self.button = game:CreatePolygon(defaults, true)
    self.button:SetPosition(pos.x, pos.y)
    self.button:SetScale(6, 4, 0)
    self.position = pos or { x = 0, y = 0 }
    self.hitbox = { one = { x1 = 0, y1 = 0}, two = { x2 = 0, y2 = 0} }
    self.mouse = { x = mx, y = my }
    self._events = table.create(0, 2)
    self.contains_cursor = false
    self.text = text

    ---@return nil
    local function SetupHitbox()
        local px, py = self.button:GetPosition()
        local ButtonScaleX, ButtonScaleY = self.button:GetScale()

        local minX, minY = math.huge, math.huge
        local maxX, maxY = -math.huge, -math.huge

        for _, point in ipairs(self.button.Points) do
            if type(point) == "table" and point[1] and point[2] then
                local scaledX = point[1] * ButtonScaleX
                local scaledY = point[2] * ButtonScaleY

                minX = math.min(minX, scaledX)
                minY = math.min(minY, scaledY)
                maxX = math.max(maxX, scaledX)
                maxY = math.max(maxY, scaledY)
            end
        end

        -- Account for negative offsets from the position origin
        self.hitbox["one"]["x1"] = px + minX
        self.hitbox["one"]["y1"] = py + minY
        self.hitbox["two"]["x2"] = px + maxX
        self.hitbox["two"]["y2"] = py + maxY
    end

    local TargetTextPosX <const> = self.button.Position.x * 1.25
    local TargetTextPosY <const> = self.button.Position.y * 1.15
    self.label = game:CreateText(self.text, TargetTextPosX, TargetTextPosY, 2, 0, 0, 0, 1)

    ---@param action string
    ---@param state InputActionState
    ---@param key KeyName
    ---@return nil
    local function TriggerAction(action, state, key)
        if is_touching_button ~= nil and is_touching_button then
            local success = xpcall(source, function(...)
                core.colorPrint("ERROR", ...); error()
            end, action, state, key); print(success)
        end
    end

    if type(name) ~= "string" then name = "ButtonObject" end
    local binding, target = name .. "_click", "mouse1"
    local ClickEvent = core.Event.new("ButtonClicked")
    local TouchingEvent = core.Event.new("TouchingEvent")
    InputService:BindAction(binding, target, TriggerAction)
    table.insert(self._events, TouchingEvent)
    table.insert(self._events, ClickEvent)
    SetupHitbox()

    ---@param ... any
    TouchingEvent:Connect(function(...)
        is_touching_button = ({...})[1]
    end)

    return self
end

function ButtonInstancer:DisplayButton()
    if not self then return end
    if not self.button then return end
    if not self._events then return end

    local comapare_functions = {
        less_than = function(a, b) return a < b end,
        greater_than = function(a, b) return a > b end,
    };

    ---@return integer?, integer?
    local function GetApplicationMouse()
        local cmx, cmy = InputService:GetMousePosition()
        local wx, wy = game:GetPosition()
        local cwx, cwy = cmx - wx, cmy - wy
        return math.tointeger(cwx), math.tointeger(cwy)
    end

    game["MouseOffsetX"] = 8   -- Standard Windows side border padding
    game["MouseOffsetY"] = 31  -- Standard Windows title bar height

    ---Returns mouse coordinates shifted into canvas space
    ---@return number, number
    function GetCanvasMouse()
        local rawX, rawY = GetApplicationMouse()
        return rawX - game.MouseOffsetX, rawY - game.MouseOffsetY
    end

    local function InBox(box, mx, my)
        if not box or not box.one or not box.two then return false end
        return mx >= box.one.x1 and mx <= box.two.x2
        and my >= box.one.y1 and my <= box.two.y2
    end

    local mx, my = GetCanvasMouse()
    self.contains_cursor = InBox(self.hitbox, mx, my)
    self.button:Render(game)
    self.label:Render(game)

    if self.contains_cursor then
        self.button:SetColor(180, 180, 180)
    else
        self.button:SetColor(255, 255, 255)
    end

    return nil
end

local TestButton = ButtonInstancer.new("test", { x = 100, y = 100 }, "TEST", function(name, action, key)
    
end)

local function TickGame()
    if game ~= nil then
        io.stdout:setvbuf("line")

        if MusicAudio:IsPlaying() then
            local PositionLabel = "AUDIO POSITION:\t"
            local MusicAudioTime = MusicAudio:GetTimePosition()
            local AudioPosition = string.format(PositionLabel .. "%g", MusicAudioTime)
            print(AudioPosition .. " / " .. string.format("%g", MusicAudio:GetDuration()))
        end

        InputService:SetGlobalInput(game:IsFocused())
        ExitEventObject:Fire(game:IsRunning())
        TestButton:DisplayButton()
        InputService:UpdateAll()
        game:SwapBuffers()
    end
end

---@param status boolean
ExitEventObject:Connect(function(status)
    if status ~= nil and status == false then
        core.colorPrint("DEBUG", "EXITING...")
        game:Close(); os.exit(0x000, true)
    end
end)

if not game:IsFocused() then game:Focus() end
runtime.Stepped:Connect(TickGame)
DebuggerRuntime:Resume()
runtime:KeepAlive()
