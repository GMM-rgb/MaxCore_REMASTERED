local StorageService = EngineCore:LoadService("StorageService")
local RuntimeService = EngineCore:LoadService("RunnerService")
local WindowService = EngineCore:LoadService("WindowService")
local ListWindow = WindowService:CreateWindow("Commit History") or {}
local ManagementWindow = WindowService:CreateWindow("Main Configuration") or {}
local IconSucess = ManagementWindow:SetIcon("E:/gear_icon.png", { default = true })
local MainHeartbeat = nil --- @type RunnerConnection?

MainHeartbeat = RuntimeService.Heartbeat:Connect(function(dt)
    RuntimeService:Spawn("RenderStepped", function(wait, join)
        for _, window in ipairs({ListWindow, ManagementWindow}) do
            if not EngineCore.IsA(window, "WindowObject") and MainHeartbeat then
                MainHeartbeat:disconnect(); MainHeartbeat = nil; return
            elseif window:IsVisible() then
                window:SwapBuffers()
            end; wait()
        end
    end, { maxConcurrent = 4, once = true })

    local WindowsRunning = ListWindow:IsRunning() and ManagementWindow:IsRunning()
    if not WindowsRunning then RuntimeService._native:shutdown(); os.exit(true, true) return end
end, { whenRunning = "allow", safe = true })

EngineCore.ColorPrint("INFO", "STARTED...", RuntimeService._running)

RuntimeService:KeepAlive()
