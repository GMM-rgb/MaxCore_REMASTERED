local StorageService = EngineCore:LoadService("StorageService")
local RuntimeService = EngineCore:LoadService("RunnerService")
local WindowService = EngineCore:LoadService("WindowService")
local ListWindow = WindowService:CreateWindow("Commit History") or {}
local ManagementWindow = WindowService:CreateWindow("Main Configuration") or {}
local MainHeartbeat = nil --- @type RunnerConnection?

ListWindow:SetIcon("E:/gear_icon.png", { all = false, scope = "window" })

local function create_display_menu()

end

MainHeartbeat = RuntimeService.Heartbeat:Connect(function(dt)
    for _, window in ipairs({ListWindow, ManagementWindow}) do
        if not EngineCore.IsA(window, "WindowObject") and MainHeartbeat then
            MainHeartbeat:disconnect(); MainHeartbeat = nil; return
        else
            window:SwapBuffers()
        end
    end

    local WindowsRunning = ListWindow:IsRunning() and ManagementWindow:IsRunning()
    if not WindowsRunning then os.exit(true, true); return end
end, { whenRunning = "allow", safe = true })

EngineCore.ColorPrint("INFO", "STARTED...", RuntimeService._running)

RuntimeService:KeepAlive()
