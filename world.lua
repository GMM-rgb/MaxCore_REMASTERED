---@alias ChildrenEventDictionary { ["event"]: Event, ["connection"]: EventConnection }
local GraphicService = EngineCore:LoadService("WindowService")
local RuntimeService = EngineCore:LoadService("RunnerService")
local StorageService = EngineCore:LoadService("StorageService")
local InputService = EngineCore:LoadService("InputService")
local <const> GameWindow = GraphicService:CreateWindow("my world", 500, 400, { renderer = "auto" })
assert(GameWindow, "game window failed to be instanced, insufficent perms!")
assert(RuntimeService:HasNative(), "native runtime scheduler is not available!")
local ToolbarBorder = EngineCore.PseudoStylesClass.new("border", 150, 150, 150, 3)
local ToolbarRound = EngineCore.PseudoStylesClass.new("corner", 8)
local ShutdownEvent = EngineCore.Event.new("shutdown-program")
local renderer = GameWindow:GetRenderer() or {}
local wdx, wdy = GameWindow:GetDimensions()
local toolbar_padding = 10

GameWindow:SetAliasingQuality("2d", 4)
GameWindow:SetIcon("./resources/app_default.png", { scope ="window" })
EngineCore.ColorPrint("INFO", renderer.description, "NAME:\t" .. renderer.name:upper())
local CambriaFontData = StorageService:GetFile("./fonts/cambria.ttf"):Read()
local id, width_reason, height = GameWindow:LoadImage("./resources/app_default.png")

assert(CambriaFontData, "couldn't fetch font data!")
assert(id, "image unavailable!"); assert(height)
assert(type(width_reason) == "number", width_reason)

local SceneLight = GameWindow:CreateLight(10, -10, 10, 0.45, 0.9)
local MainCamera = GameWindow:CreateCamera(0, 0.75, -4, 90, 0.1, 100)

assert(SceneLight, "game light isn't available!")
assert(MainCamera, "main camera wasn't created correctly!")

GameWindow:SetActiveLight(SceneLight)
GameWindow:SetActiveCamera(MainCamera)

local Terrain = GameWindow:CreateMesh()
local TestCube = GameWindow:CreateCube(0, 0, 5, 1)
TestCube:SetRotation(0, 45, 0)
TestCube:SetFillMode("solid")

Terrain:SetVertices({
    { 0, 0,  0 }, -- Vertex 1
    { 0, 0, -5 }, -- Vertex 2
    { 5, 0, -5 }, -- Vertex 3
    { 5, 0,  0 },  -- Vertex 4
})

Terrain:SetFaces({
    { 1, 2, 3 },
    { 1, 3, 4 },
})

Terrain:SetFillMode("wireframe")

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
    GameWindow:RenderScene3D()
    update_toolbar_position()
end, { key = "interface-rendering", safe = true, whenRunning = "replace" })

RuntimeService.RenderStepped:Connect(function(dt, wait, join)
    GameWindow:SwapBuffers() GameWindow:ClearCanvas(); InputService:UpdateAll()
    local <const> mem_info = EngineCore.PlatformUtils.get_memory_info()
    -- print("USED MEMORY:\t", mem_info["total_mb"] - mem_info["available_mb"] .. " MB")
end, { key = "main-rendering", whenRunning = "skip", yieldable = true, safe = true })

local MovementConfiguration = {
    ["w"] = Vector3.new(0, 0, 1),
    ["s"] = Vector3.new(0, 0, -1),
    ["a"] = Vector3.new(-1, 0, 0),
    ["d"] = Vector3.new(1, 0, 0)
}

local MOVEMENT_SPEED = 2 -- Adjust this value to set your actual movement speed
local TURN_SPEED = 1.57

local function turn_camera(direction)
    MainCamera:SetRotation(direction.x, direction.y, direction.z)
end

RuntimeService.Stepped:Connect(function(dt)
    local moveDirection = Vector3.new(0, 0, 0)

    for key, vector in pairs(MovementConfiguration) do
        if InputService and InputService:IsKeyDown(key) then
            moveDirection = moveDirection + vector
        end
    end

    local currentRotation = Vector3.new(MainCamera:GetRotation())
    local rotationChange = Vector3.new(0, 0, 0)

    if InputService:IsKeyDown("left") then
        rotationChange = rotationChange + Vector3.new(0, TURN_SPEED * dt, 0)
    elseif InputService:IsKeyDown("right") then
        rotationChange = rotationChange - Vector3.new(0, TURN_SPEED * dt, 0)
    end

    local newRotation = currentRotation + rotationChange
    turn_camera(newRotation)

    if moveDirection.Magnitude and moveDirection.Magnitude > 0 then
        moveDirection = moveDirection.Unit
        local yaw = newRotation.y

        local rotatedX = moveDirection.x * math.cos(yaw) - moveDirection.z * math.sin(yaw)
        local rotatedZ = moveDirection.x * math.sin(yaw) + moveDirection.z * math.cos(yaw)
        local worldMove = Vector3.new(rotatedX, moveDirection.y, rotatedZ)

        local scalar = (MOVEMENT_SPEED * dt) --- @as number
        local currentPos = Vector3.new(MainCamera:GetPosition())
        local displacement = Vector3.new(worldMove.x * scalar, worldMove.y * scalar, worldMove.z * scalar)
        local newPosition = currentPos + displacement
        MainCamera:SetPosition(newPosition.x, newPosition.y, newPosition.z)
    end
end)

ShutdownEvent:Connect(function()
    RuntimeService._native:shutdown()
    -- os.execute("clear")
    io:flush(); os.exit(true)
end); RuntimeService:KeepAlive()
