---@alias ButtonSourceCode fun(name: string, action: InputActionState, key: KeyName): nil
---@alias HitboxCoordinates { ["one"]: { x1: number, y1: number }, ["two"]: { x2: number, y2: number }}
---@alias ButtonCoordinates { x: number, y: number }
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
local _, TypeError = io.popen("../_services.d.lua", "r")
local NoiseGeneration = core.NoiseClass.new(os.time())
local ExitEventObject = core.Event.new("exit")
local MachineName = platform:get_os_name()
local MachineArch = platform:get_arch()
local MachineInfo = platform:get_info()

if type(TypeError) == "string" then
    io.stdout:write(TypeError)
    io.stdout:close()
end

sound:SetStorageService(StorageService)
sound:SetCacheFolder("audio_cache")

-- local TargetPath = "https://music.youtube.com/watch?v=8LShXs7yAC0&si=ZkQv-PFN1cy2MZ8p"
local TargetPath = "https://music.youtube.com/watch?v=IOym7Md8Hcw&si=pC83L1ssgOTbEUwW"
local MusicAudio = sound:LoadSound(tostring(TargetPath) or "$PATH")
MusicAudio:SetLooping(true)
MusicAudio:SetVolume(1.0)
MusicAudio:SetPitch(1.125)
MusicAudio:Play()

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
    self.text = text

    local TargetTextPosX <const> = self.button.Position.x * 1.25
    local TargetTextPosY <const> = self.button.Position.y * 1.15
    self.label = game:CreateText(self.text, TargetTextPosX, TargetTextPosY, 2, 0, 0, 0, 1)

    ---@param action string
    ---@param state InputActionState
    ---@param key KeyName
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

    local cmx, cmy = InputService:GetMousePosition()

    ---@return number, number
    local function GetApplicationMouse()
        local wx, wy = game:GetPosition()
        local cwx, cwy = cmx - wx, cmy - wy
        return cwx, cwy
    end

    if logs ~= nil and core.typeof(logs) == "FileObject" then
        local x, y = GetApplicationMouse()
        local message = string.format("%g, %g", x, y)
        if message and logs:Read() ~= message then logs:Write(message) end
        self.mouse.x, self.mouse.y = x, y
    end

    ---@return boolean
    local function MouseMatchesHitbox()
        local ContainsCursor = false

        for group, coordinates in pairs(self.hitbox) do
            if type(group) == "string" and group:len() > 0 then
                for _, coordinate in ipairs(coordinates or {}) do
                    if coordinate ~= nil and type(coordinate) == "number" then
                        if (InputService:GetMousePosition()) > coordinate then
                        
                        elseif (InputService:GetMousePosition()) < coordinate then
                            
                        end
                    end
                end
            end
        end

        return ContainsCursor
    end

    if self._events ~= nil and type(self._events) == "table" then
        for _, SelectedEvent in ipairs(self._events or {}) do
            if SelectedEvent and MouseMatchesHitbox() then
                SelectedEvent:Fire(false)
            end
        end
    end

    self.button:Render(game)
    self.label:Render(game)

    return nil
end

local TestButton = ButtonInstancer.new("test", { x = 100, y = 100 }, "TEST", function(name, action, key)
    
end)

local function TickGame()
    if game ~= nil then
        ExitEventObject:Fire(game:IsRunning() or false)
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

InputService:SetGlobalInput(true)
runtime.Stepped:Connect(TickGame)
DebuggerRuntime:Resume()
runtime:KeepAlive()
