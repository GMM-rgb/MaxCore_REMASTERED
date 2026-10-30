local InputUpdateEvent = EngineCore.Event.new("input-update")
local ProgramEditorExit = EngineCore.Event.new("program-exit")
local <const> RunnerService = EngineCore:LoadService("RunnerService")
local <const> InputService = EngineCore:LoadService("InputService")
local <const> WindowService = EngineCore:LoadService("WindowService")
local <const> StorageService = EngineCore:LoadService("StorageService")
local EditorWindow = WindowService:CreateWindow("Program Editor Mode")
if EditorWindow == nil then error("editor window unavailable") return end
EditorWindow:SetAliasingQuality("2d", WindowService:GetMaxAliasingQuality())

---@class EditorInterface
---@field private _objects table<integer, GameObject>
---@field private _window WindowObject?
local EditorInterface = {}
EditorInterface._objects = {}

---@class configuration_values
---@field shape string
---@field size string|table<number|"inherit">|number
---@field position table<integer, number>|"inherit"
---@field parent GameObject|string?

---@class EditorConfiguration
---@field public theme "dark" | "light"
---@field public object_configuration table<string, configuration_values>
local EditorConfiguration = {
    ["theme"] = "dark",
    ["object_configuration"] = {
        ["container"] = {
            ["shape"] = "Rect",
            ["size"] = "auto",
            ["position"] = { 0, 0 },
        },
        ["toolbar"] = {
            ["shape"] = "Rect",
            ["parent"] = "container",
            ["size"] = { "inherit", 25 },
            ["position"] = { 0, 0 },
        },
    },
};

---@return table
local function ExtractObjectArguments()
    return {} -- TODO
end

---@param name string
---@return string
local function capitalize_name(name)
    local words = {}

    for word in name:gmatch("%w+") do
        -- Capitalize the first letter (%u) and keep the rest lowercase (%l)
        local capitalized = word:sub(1,1):upper() .. word:sub(2):lower()
        table.insert(words, capitalized)
    end
    
    return table.concat(words, utf8.char(32))
end

---@param TargetWindow WindowObject
---@return EditorInterface
function EditorInterface.new(TargetWindow)
    local self = setmetatable({}, EditorInterface) --- @type EditorInterface
    if type(self._window) == "nil" then self._window = TargetWindow end
    return self
end

---@param self EditorInterface
---@return nil
function EditorInterface:RenderInterface()
    for _, obj in ipairs(self._objects) do
        self._window:RenderObject(obj)
    end
end

---@param self EditorInterface
---@return table
function EditorInterface:GenerateLayout()
    local ObjectConfiguration = EditorConfiguration["object_configuration"]

    for name, config in pairs(ObjectConfiguration) do
        self._window["Create" .. capitalize_name(config.shape)]()
    end

    return {}
end

-- local EditorGroup = EditorInterface.new(EditorWindow)
-- EditorGroup:RenderInterface()

RunnerService:Spawn("Stepped", function(wait, join)
    for _, name in ipairs(type(InputService.InputKeys) == "table" and InputService.InputKeys or {}) do
        InputService:BindAction("listen-" .. name, name, function(_, state, key)
            InputUpdateEvent:Fire(state, key)
        end)
    end
end, { reentrant = false, once = true })

local CoreConnection = RunnerService.RenderStepped:Connect(function(dt, wait, join)
    if EditorWindow and EditorWindow:IsRunning() then EditorWindow:SwapBuffers()
    elseif EditorWindow then ProgramEditorExit:Fire(true, true) end
end, { priority = 0, yieldable = true, whenRunning = "allow" })

ProgramEditorExit:Connect(os.exit)
print(tostring(RunnerService:_jobOf(CoreConnection).id))
RunnerService:KeepAlive()
