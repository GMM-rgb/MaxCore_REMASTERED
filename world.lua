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

-- ---@param SelectableElement RectObject
-- local function add_toolbar_selectable(SelectableElement)
--     local EventName = "update-event-" .. tostring(SelectableElement)
--     local <const> UpdateEvent = EngineCore.Event.new(EventName)

--     local function update_wrapper()
--         local selection_order_index = #ToolbarInterface["items"] + 1
--         local target_x_position, y_target = ToolbarInterface.Position.x + toolbar_padding, ToolbarInterface.Position.y + toolbar_padding

--         for _, object in pairs(ToolbarInterface["items"]) do
--             local width = object.Size.w --- @as number
--             local new_pos = target_x_position + width + toolbar_padding
--             target_x_position = new_pos
--         end

--         ---@param position Vector3
--         local function unpack_position(position)
--             local unpacked = {} --- @type table<number>
--             for _, v in pairs(position) do table.insert(unpacked, v) end
--             return table.concat(unpacked, ", ")
--         end

--         SelectableElement.Size.h = ToolbarInterface.Size.h - toolbar_padding * 2
--         SelectableElement:SetPosition(target_x_position, y_target)
--         -- EngineCore.ColorPrint("DEBUG", unpack_position(SelectableElement.Position))
--     end

--     local connection = UpdateEvent:Connect(update_wrapper); update_wrapper()
--     table.insert(ToolbarChildrenEvents, { ["event"] = UpdateEvent, ["connection"] = connection })
--     table.insert(ToolbarInterface["items"], SelectableElement)
-- end

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
