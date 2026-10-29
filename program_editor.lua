local InputUpdateEvent = EngineCore.Event.new("input-update")
local RunnerService = EngineCore:LoadService("RunnerService")
local InputService = EngineCore:LoadService("InputService")
local WindowService = EngineCore:LoadService("WindowService")
local EditorWindow = WindowService:CreateWindow()

RunnerService.RenderStepped:Connect(function (dt, wait, join)
    if EditorWindow and EditorWindow:IsRunning() then
        EditorWindow:SwapBuffers()
    elseif EditorWindow then
        EditorWindow:Close()
    end
end, { priority = 0, yieldable = true, whenRunning = "allow" })

RunnerService:KeepAlive()
