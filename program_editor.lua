local InputUpdateEvent = EngineCore.Event.new("input-update")
local ProgramEditorExit = EngineCore.Event.new("program-exit")
local RunnerService = EngineCore:LoadService("RunnerService")
local InputService = EngineCore:LoadService("InputService")
local WindowService = EngineCore:LoadService("WindowService")
local EditorWindow = WindowService:CreateWindow()

local CoreConnection, CoreJob = RunnerService.RenderStepped:Connect(function(dt, wait, join)
    if EditorWindow and EditorWindow:IsRunning() then EditorWindow:SwapBuffers()
    elseif EditorWindow then EditorWindow:Close(); ProgramEditorExit:Fire(true) end
end, { priority = 0, yieldable = true, whenRunning = "allow" })

RunnerService:KeepAlive()
