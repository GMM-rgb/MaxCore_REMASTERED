---@alias Vertex3D { [1]: number, [2]: number, [3]: number }
local <const> WindowService = EngineCore:LoadService("WindowService")
local <const> RunnerService = EngineCore:LoadService("RunnerService")
local <const> InputService = EngineCore:LoadService("InputService")
local <const> SoundService = EngineCore:LoadService("SoundService")
local <const> StorageService = EngineCore:LoadService("StorageService")
local DemoGraphics = WindowService:CreateWindow("DEMO", 400, 400, "vulkan") or {}

EngineCore.ColorPrint("DEBUG", "RENDERER", DemoGraphics:GetRenderer().name)
SoundService:SetStorageService(StorageService)
SoundService:SetCacheFolder("app_audio/")

---@param radius number
---@param subdivisions integer
---@return MeshObject
local function create_icosphere(radius, subdivisions)
    local t = (1 + math.sqrt(5)) / 2

    ---@type Vertex3D[]
    local vertices = {
        {-1, t, 0}, {1, t, 0},
        {-1, -t, 0}, {1, -t, 0},
        {0, -1, t}, {0, 1, t},
        {0, -1, -t}, {0, 1, -t},
        {t, 0, -1}, {t, 0, 1},
        {-t, 0, -1}, {-t, 0, 1}
    }

    ---@type integer[][]
    local faces = {
        {1,12,6}, {1,6,2}, {1,2,8}, {1,8,11}, {1,11,12},
        {2,6,10}, {6,12,5}, {12,11,3}, {11,8,7}, {8,2,9},
        {4,10,5}, {4,5,3}, {4,3,7}, {4,7,9}, {4,9,10},
        {5,10,6}, {3,5,12}, {7,3,11}, {9,7,8}, {10,9,2}
    }

    local function normalize(v)
        local length = math.sqrt(
            v[1]^2 + v[2]^2 + v[3]^2
        )

        return {
            v[1] / length,
            v[2] / length,
            v[3] / length
        }
    end

    for i, vertex in ipairs(vertices) do
        vertices[i] = normalize(vertex)
    end

    for _ = 1, subdivisions do
        local midpointCache = {}

        local function midpoint(a, b)
            local low = math.min(a, b)
            local high = math.max(a, b)
            local key = low .. ":" .. high

            local cached = midpointCache[key]
            if cached then return cached end

            local va = vertices[a]
            local vb = vertices[b]

            local vertex = normalize({
                (va[1] + vb[1]) * 0.5,
                (va[2] + vb[2]) * 0.5,
                (va[3] + vb[3]) * 0.5
            })

            vertices[#vertices + 1] = vertex

            local index = #vertices
            midpointCache[key] = index

            return index
        end

        local newFaces = {}

        for _, face in ipairs(faces) do
            local a = midpoint(face[1], face[2])
            local b = midpoint(face[2], face[3])
            local c = midpoint(face[3], face[1])

            newFaces[#newFaces + 1] = {face[1], a, c}
            newFaces[#newFaces + 1] = {face[2], b, a}
            newFaces[#newFaces + 1] = {face[3], c, b}
            newFaces[#newFaces + 1] = {a, b, c}
        end

        faces = newFaces
    end

    for _, vertex in ipairs(vertices) do
        for i = 1, 3 do
            vertex[i] = vertex[i] * radius
        end
    end

    return DemoGraphics:CreateMesh(vertices, faces, 0, 255, 0, "solid")
end

DemoGraphics:SetAliasingQuality("2d", 0)
DemoGraphics:SetAliasingQuality("3d", 4)

local SceneLight = DemoGraphics:CreateLight(0.5, -0.7, 1.0, 0.25, 1.0)
local SceneCamera = DemoGraphics:CreateCamera(0, 1, -2, 60)
DemoGraphics:SetActiveCamera(SceneCamera or {})
DemoGraphics:SetActiveLight(SceneLight or {})

local DemoSphere = create_icosphere(0.75, 3)
DemoSphere:SetPosition(0, 1, 0)

local SliderFrameWidth, SliderFrameHeight = 150, 65
local SliderTrackWidth, SliderTrackHeight = SliderFrameWidth - 50, SliderFrameHeight - 50
local WindowDimensionX, WindowDimensionY = DemoGraphics:GetDimensions()
local TrackDimensions = { x = SliderTrackWidth, y = SliderTrackHeight }

local Frame = DemoGraphics:CreateRect(50, 10, SliderFrameWidth, SliderFrameHeight, 255, 255, 255)
Frame = Frame:ApplyStyle(EngineCore.PseudoStylesClass.new("border", 140, 140, 140, 3))
Frame = Frame:ApplyStyle(EngineCore.PseudoStylesClass.new("corner", 10))
local FrameX, FrameY = Frame:GetPosition()

local SlideTrack = DemoGraphics:CreateRect(FrameX + 25, FrameY + 25, SliderTrackWidth, SliderTrackHeight)
SlideTrack:ApplyStyle(EngineCore.PseudoStylesClass.new("border", 140, 140, 140, 2))
SlideTrack:ApplyStyle(EngineCore.PseudoStylesClass.new("corner", 10))
SlideTrack:SetColor(52, 156, 235)

local SlideInteractable = DemoGraphics:CreateCircle(FrameX + SliderFrameWidth - 30, FrameY + SliderFrameHeight / 2, 10, true)
SlideInteractable:SetColor(162, 235, 52); SlideInteractable:ApplyStyle(EngineCore.PseudoStylesClass.new("border", 140, 140, 140, 2))

---@async
---@generic T
---@param InteractableObject GameObject<T>
---@param DirectionAxis "x" | "y"
---@return nil
local function DragInteractable(InteractableObject, DirectionAxis)
    local position, trackpos = InteractableObject.Position[DirectionAxis], SlideTrack.Position[DirectionAxis] --- @type number, number
    local x, y = InputService:GetMouseDelta(); local <const> deltas = { x = x, y = y }; local tracksize = TrackDimensions[DirectionAxis]
    local <const> DragLimits = { [DirectionAxis .. "1"] = { trackpos, function(a, b) return a > b end }, [DirectionAxis .. "2"] = { tracksize, function(a, b) return a < b end } }
    
    -- for LimitKey, LimitValue in pairs(DragLimits) do
    --     if LimitValue and type(LimitKey) == "string" then
    --         if not LimitValue[2](position, LimitValue[1]) then
    --             return nil
    --         end
    --     end
    -- end

    InteractableObject.Position[DirectionAxis] = deltas[DirectionAxis] + position
end

RunnerService:Spawn("RenderStepped", function(wait, join)
    while true and wait do
        local dt = wait()

        local x, y = SlideInteractable:GetPosition()
        local SlideRadius = SlideInteractable.Radius
        local HalfRadius = SlideRadius / 2
        local TopX, TopY = x - HalfRadius, y - HalfRadius
        local BottomX, BottomY = x + HalfRadius, y + HalfRadius
        local MouseX, MouseY = DemoGraphics:GetMousePosition()
        local <const> TargetSize = 1.25

        if TopX <= MouseX and TopY <= MouseY and BottomX >= MouseX and BottomY >= MouseY then
            if InputService:IsKeyDown("mouse1") then
                SlideInteractable:SetScale(TargetSize - 0.1, TargetSize - 0.1, TargetSize - 0.1)
                DragInteractable(SlideInteractable, "x")
            else
                SlideInteractable:SetScale(TargetSize, TargetSize, TargetSize)
            end
        else
            SlideInteractable:SetScale(1, 1, 1)
        end
    end
end, { whenRunning = "allow", maxConcurrent = 6 })

local GraphicRuntime = RunnerService.RenderStepped:Connect(function(dt, wait, join)
    if not DemoGraphics:IsRunning() then os.exit(true, true); return else DemoGraphics:SwapBuffers() end
    DemoSphere:SetRotation(0, DemoSphere.Rotation.y + (dt * math.rad(75.0)), 0)
    DemoGraphics:ClearCanvas(0, 0, 0)
    DemoGraphics:RenderScene3D()
    DemoGraphics:RenderScene2D()
    InputService:UpdateAll()
end, { yieldable = true, id = "main-graphic-process" }); RunnerService:KeepAlive()
