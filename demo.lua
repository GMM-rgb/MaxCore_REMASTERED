local <const> WindowService = EngineCore:LoadService("WindowService")
local <const> RunnerService = EngineCore:LoadService("RunnerService")
local DemoGraphics = WindowService:CreateWindow("DEMO") or {}
local DemoImage = DemoGraphics:CreateImageObject(0, 10, 10)
local DemoCube = DemoGraphics:CreateCube()

DemoCube:SetScale(0.5, 0.85, 0.5)

local GraphicRuntime = RunnerService.RenderStepped:Connect(function(dt, wait, join)
    if not DemoGraphics:IsRunning() then os.exit(true, true); return else DemoGraphics:SwapBuffers() end
    DemoGraphics:ClearCanvas(0, 0, 0); DemoGraphics:RenderScene3D()
end, { yieldable = true, id = "main-graphic-process" });

RunnerService:KeepAlive()
