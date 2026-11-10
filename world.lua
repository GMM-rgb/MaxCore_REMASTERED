local GraphicService = EngineCore:LoadService("WindowService")
local RuntimeService = EngineCore:LoadService("RunnerService")
local <const> GameWindow = GraphicService:CreateWindow("my world", 300, 200, { renderer = "auto" })
assert(GameWindow ~= nil, "game window failed to be instanced, insufficent perms!")
assert(RuntimeService:HasNative(), "native runtime scheduler is not available!")
local ToolbarBorder = EngineCore.PseudoStylesClass.new("border", 150, 150, 150, 3)
local ToolbarRound = EngineCore.PseudoStylesClass.new("corner", 8)
local ShutdownEvent = EngineCore.Event.new("shutdown-program")
local renderer = GameWindow:GetRenderer() or {}
local wdx, wdy = GameWindow:GetDimensions()

GameWindow:SetAliasingQuality("2d", 4)
EngineCore.ColorPrint("INFO", renderer.description, "\nNAME:\t" .. renderer.name:upper())

local ToolbarInterface = GameWindow:CreateRect(0, 0, 200, 50)
ToolbarInterface = ToolbarInterface:ApplyStyle(ToolbarBorder)
ToolbarInterface = ToolbarInterface:ApplyStyle(ToolbarRound)
ToolbarInterface["items"] = {} --- @type GameObject[]

---@return nil
local function update_toolbar_position()
    local toolbar_pos_x = (wdx / 2) - (ToolbarInterface.Size.w / 2) -- calculate based on width and window x dimension
    ToolbarInterface:SetPosition(toolbar_pos_x, wdy - ToolbarInterface.Size.h - 10)
    ToolbarInterface.Size.w = wdx - 20
end

---@param SelectableElement GameObject
local function add_toolbar_selectable(SelectableElement)
    local selection_order_index = #ToolbarInterface["items"] + 1
    local bottom_y_pos = ToolbarInterface.Position.y + ToolbarInterface.Size.h
    local target_x_position, y_target = 0, bottom_y_pos / 2
    SelectableElement:SetPosition(target_x_position, y_target)
end

local InterfaceConnection, InterfaceJob = RuntimeService.RenderStepped:Connect(function(dt)
    if not GameWindow:IsRunning() then ShutdownEvent:Fire(); return end
    wdx, wdy = GameWindow:GetDimensions()
    GameWindow:RenderScene2D()
    update_toolbar_position()
end, { key = "interface-rendering", safe = true, whenRunning = "replace" })

RuntimeService.RenderStepped:Connect(function(dt, wait, join)
    GameWindow:SwapBuffers(); GameWindow:ClearCanvas();
end, { key = "main-rendering", whenRunning = "skip", yieldable = true, safe = true })

ShutdownEvent:Connect(function()
    RuntimeService._native:shutdown()
    io:flush(); os.exit(true)
end); RuntimeService:KeepAlive()
