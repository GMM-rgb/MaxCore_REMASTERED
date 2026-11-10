---@alias ChildrenEventDictionary { ["event"]: Event, ["connection"]: EventConnection }
local GraphicService = EngineCore:LoadService("WindowService")
local RuntimeService = EngineCore:LoadService("RunnerService")
local <const> GameWindow = GraphicService:CreateWindow("my world", 300, 200, { renderer = "vulkan" })
assert(GameWindow, "game window failed to be instanced, insufficent perms!")
assert(RuntimeService:HasNative(), "native runtime scheduler is not available!")
local ToolbarBorder = EngineCore.PseudoStylesClass.new("border", 150, 150, 150, 3)
local ToolbarRound = EngineCore.PseudoStylesClass.new("corner", 8)
local ShutdownEvent = EngineCore.Event.new("shutdown-program")
local renderer = GameWindow:GetRenderer() or {}
local wdx, wdy = GameWindow:GetDimensions()
local ToolbarChildrenEvents = {} --- @type table<integer, ChildrenEventDictionary>
local toolbar_padding = 10


GameWindow:SetAliasingQuality("2d", 4)
EngineCore.ColorPrint("INFO", renderer.description, "\nNAME:\t" .. renderer.name:upper())

local ToolbarInterface = GameWindow:CreateRect(0, 0, 200, 50)
ToolbarInterface = ToolbarInterface:ApplyStyle(ToolbarBorder)
ToolbarInterface = ToolbarInterface:ApplyStyle(ToolbarRound)
ToolbarInterface["items"] = {} --- @type RectObject[]

---@return nil
local function update_toolbar_position()
    local toolbar_pos_x = (wdx / 2) - (ToolbarInterface.Size.w / 2) -- calculate based on width and window x dimension
    ToolbarInterface:SetPosition(toolbar_pos_x, wdy - ToolbarInterface.Size.h - 10)

    for _, values in ipairs(ToolbarChildrenEvents) do
        
    end

    ToolbarInterface.Size.w = wdx - 20
end

---@param SelectableElement GameObject
local function add_toolbar_selectable(SelectableElement)
    local EventName = "update-event-" .. tostring(SelectableElement)
    local <const> UpdateEvent = EngineCore.Event.new(EventName)

    local function update_wrapper()
        local selection_order_index = #ToolbarInterface["items"] + 1
        local bottom_y_pos = ToolbarInterface.Position.y + ToolbarInterface.Size.h
        local target_x_position, y_target = ToolbarInterface.Position.x + toolbar_padding, bottom_y_pos / 2

        if #ToolbarInterface["items"] > 0 then
            for _, object in pairs(ToolbarInterface["items"]) do
                local width = object.Size.w --- @as number
                local new_pos = target_x_position + width + toolbar_padding
                target_x_position = new_pos
            end
        end

        ---@param position { x: number, y: number, z: number }
        local function unpack_position(position)
            local unpacked = {} --- @type table<number>
            for _, v in pairs(position) do table.insert(unpacked, v) end
            return table.concat(unpacked, ", ")
        end

        SelectableElement:SetPosition(target_x_position, y_target)
        EngineCore.ColorPrint("DEBUG", unpack_position(SelectableElement.Position))
    end

    local connection = UpdateEvent:Connect(update_wrapper); update_wrapper()
    table.insert(ToolbarChildrenEvents, { ["event"] = UpdateEvent, ["connection"] = connection })
end

local myelement = GameWindow:CreateRect(0, 0, 30, 30)
add_toolbar_selectable(myelement)

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
