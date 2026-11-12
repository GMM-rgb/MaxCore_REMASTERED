---@alias ChildrenEventDictionary { ["event"]: Event, ["connection"]: EventConnection }
local GraphicService = EngineCore:LoadService("WindowService")
local RuntimeService = EngineCore:LoadService("RunnerService")
local <const> GameWindow = GraphicService:CreateWindow("my world", 500, 400, { renderer = "opengl" })
assert(GameWindow, "game window failed to be instanced, insufficent perms!")
assert(RuntimeService:HasNative(), "native runtime scheduler is not available!")
local ToolbarBorder = EngineCore.PseudoStylesClass.new("border", 150, 150, 150, 3)
local ToolbarRound = EngineCore.PseudoStylesClass.new("corner", 8)
local ShutdownEvent = EngineCore.Event.new("shutdown-program")
local renderer = GameWindow:GetRenderer() or {}
local wdx, wdy = GameWindow:GetDimensions()
local toolbar_padding = 10

GameWindow:SetAliasingQuality("2d", 3)
EngineCore.ColorPrint("INFO", renderer.description, "NAME:\t" .. renderer.name:upper())

local ToolbarInterface = GameWindow:CreateRect(0, 0, 200, 50, 210, 210, 210)
ToolbarInterface = ToolbarInterface:ApplyStyle(ToolbarBorder)
ToolbarInterface = ToolbarInterface:ApplyStyle(ToolbarRound)
ToolbarInterface["items"] = {} --- @type RectObject[]
local InventoryButton = GameWindow:CreateRect(0, 0)

local function update_inventory_button()
    local ToolbarX, ToolbarY = ToolbarInterface:GetPosition()
    InventoryButton:SetPosition(ToolbarX + toolbar_padding, ToolbarY + toolbar_padding)
    InventoryButton.Size.h = ToolbarInterface.Size.h - toolbar_padding * 2
    InventoryButton.Size.w = 50 % ToolbarInterface.Size.w
end

---@return nil
local function update_toolbar_position()
    local toolbar_pos_x = (wdx / 2) - (ToolbarInterface.Size.w / 2) -- calculate based on width and window x dimension
    ToolbarInterface:SetPosition(toolbar_pos_x, wdy - ToolbarInterface.Size.h - 10)
    ToolbarInterface.Size.w = wdx - 20
    update_inventory_button()
end

local InterfaceConnection, InterfaceJob = RuntimeService.RenderStepped:Connect(function(dt)
    if not GameWindow:IsRunning() then ShutdownEvent:Fire(); return end
    wdx, wdy = GameWindow:GetDimensions()
    GameWindow:RenderScene2D()
    update_toolbar_position()
end, { key = "interface-rendering", safe = true, whenRunning = "replace" })

RuntimeService.RenderStepped:Connect(function(dt, wait, join)
    GameWindow:SwapBuffers(); GameWindow:ClearCanvas();
    local <const> mem_info = EngineCore.PlatformUtils.get_memory_info()
    -- print("USED MEMORY:\t", mem_info["total_mb"] - mem_info["available_mb"] .. " MB")
end, { key = "main-rendering", whenRunning = "skip", yieldable = true, safe = true })



ShutdownEvent:Connect(function()
    RuntimeService._native:shutdown()
    io:flush(); os.exit(true)
end); RuntimeService:KeepAlive()
