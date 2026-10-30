local InputUpdateEvent = EngineCore.Event.new("input-update")
local ProgramEditorExit = EngineCore.Event.new("program-exit")
local <const> RunnerService = EngineCore:LoadService("RunnerService")
local <const> InputService = EngineCore:LoadService("InputService")
local <const> WindowService = EngineCore:LoadService("WindowService")
local EditorWindow = WindowService:CreateWindow("Program Editor Mode")
if EditorWindow == nil then error("editor window unavailable") return end

EditorWindow:SetAliasingQuality("2d", 4)

local left_eye = EditorWindow:CreateCircle(100, 100, 40, false)
local right_eye = EditorWindow:CreateCircle(200, 100, 40, false)
local face = { left_eye, right_eye } --- @type table<GameObject>
for _, obj in ipairs(face) do EditorWindow:RenderObject(obj) end

local InputListener = setmetatable({}, nil)

function InputListener.new()
    local self = setmetatable({}, InputListener)
    self.buffer = string.char(0)
    return self or {}
end

function InputListener:_bind()
    ---@param s InputActionState
    ---@param k KeyName
    ---@param ... any
    InputUpdateEvent:Connect(function(s, k, ...)

    end)
end

RunnerService:Spawn("Stepped", function(wait, join)
    for _, name in ipairs(type(InputService.InputKeys) == "table" and InputService.InputKeys or {}) do
        InputService:BindAction("listen-" .. name, name, function(_, state, key)
            InputUpdateEvent:Fire(state, key)
        end)
    end
end, { reentrant = false, once = true })

local CoreConnection, CoreJob = RunnerService.RenderStepped:Connect(function(dt, wait, join)
    if EditorWindow and EditorWindow:IsRunning() then EditorWindow:SwapBuffers()
    elseif EditorWindow then ProgramEditorExit:Fire(true) end
end, { priority = 0, yieldable = true, whenRunning = "allow" })

print(tostring(RunnerService:_jobOf(CoreConnection).id))
RunnerService:KeepAlive(); WindowService:CloseAll()
