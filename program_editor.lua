local InputUpdateEvent = EngineCore.Event.new("input-update")
local ProgramEditorExit = EngineCore.Event.new("program-exit")
local <const> RunnerService = EngineCore:LoadService("RunnerService")
local <const> InputService = EngineCore:LoadService("InputService")
local <const> WindowService = EngineCore:LoadService("WindowService")
local EditorWindow = WindowService:CreateWindow("Program Editor Mode")
if EditorWindow == nil then error("editor window unavailable") return end

local InputListener = setmetatable({}, nil)

function InputListener.new()
    local self = setmetatable({}, InputListener)
    self.buffer = string.char(0)
    return self or {}
end

function InputListener:_bind()
    InputUpdateEvent:Connect(function()

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
    elseif EditorWindow then EditorWindow:Close(); ProgramEditorExit:Fire(true) end
    EditorWindow:Focus()
end, { priority = 0, yieldable = true, whenRunning = "allow" })

RunnerService:KeepAlive()
