---@alias ChildrenEventDictionary { ["event"]: Event, ["connection"]: EventConnection }
local GraphicService = EngineCore:LoadService("WindowService")
local RuntimeService = EngineCore:LoadService("RunnerService")
local StorageService = EngineCore:LoadService("StorageService")
local <const> GameWindow = GraphicService:CreateWindow("my world", 500, 400, { renderer = "software" })
assert(GameWindow, "game window failed to be instanced, insufficent perms!")
assert(RuntimeService:HasNative(), "native runtime scheduler is not available!")
local ToolbarBorder = EngineCore.PseudoStylesClass.new("border", 150, 150, 150, 3)
local ToolbarRound = EngineCore.PseudoStylesClass.new("corner", 8)
local ShutdownEvent = EngineCore.Event.new("shutdown-program")
local renderer = GameWindow:GetRenderer() or {}
local wdx, wdy = GameWindow:GetDimensions()
local toolbar_padding = 10

for key, value in pairs(_G.arg) do
    print(key, value)
end

GameWindow:SetAliasingQuality("2d", 4)
EngineCore.ColorPrint("INFO", renderer.description, "NAME:\t" .. renderer.name:upper())
local CambriaFontData = StorageService:GetFile("./fonts/cambria.ttf"):Read()
local id, width_reason, height = GameWindow:LoadImage("./resources/app_default.png")

assert(CambriaFontData, "couldn't fetch font data!")
assert(id, "image unavailable!"); assert(height)
assert(type(width_reason) == "number", width_reason)

local fid, reason = GameWindow:LoadFontData(CambriaFontData)
local BackgroundText = GameWindow:CreateText(nil, 20, 20, 5, 255, 255, 255, GraphicService:GetMaxTextQuality())
BackgroundText:SetText(BackgroundText.Text .. " i'm a <" .. BackgroundText.TypeName .. ">")
BackgroundText:SetStroke(140, 140, 140, 0.25)
BackgroundText:SetFont(fid)

local i = Vector2.new(0, 100) - Vector2.new(22, 78)
print(i)

local ToolbarInterface = GameWindow:CreateRect(0, 0, 200, 50, 210, 210, 210)
ToolbarInterface = ToolbarInterface:ApplyStyle(ToolbarBorder)
ToolbarInterface = ToolbarInterface:ApplyStyle(ToolbarRound)
ToolbarInterface["items"] = {} --- @type RectObject[]
local InventoryButton = GameWindow:CreateRect(0, 0)

local function update_inventory_button()
    local ToolbarX, ToolbarY = ToolbarInterface:GetPosition()
    InventoryButton:SetPosition(Vector2.new(ToolbarX + toolbar_padding, ToolbarY + toolbar_padding))
    InventoryButton.Size.h = ToolbarInterface.Size.h - toolbar_padding * 2
    InventoryButton.Size.w = 50 % ToolbarInterface.Size.w
end

---@return nil
local function update_toolbar_position()
    local toolbar_pos_x = (wdx / 2) - (ToolbarInterface.Size.w / 2) -- calculate based on width and window x dimension
    ToolbarInterface:SetPosition(Vector2.new(toolbar_pos_x, wdy - ToolbarInterface.Size.h - 10))
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
    os.execute("clear")
    io:flush(); os.exit(true)
end); RuntimeService:KeepAlive()
