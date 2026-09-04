-- =========================================================================
-- services/sub_modules/game_object.lua
-- =========================================================================
local InstanceTyping = require("instance_type")

-- Pulls the SAME Event class every other service uses (Connect/Fire/etc,
-- see max_core.lua) so GameObject.Collided behaves identically to every
-- other event in the framework instead of being a one-off reimplementation.
--
-- Resolved LAZILY (on first actual use, not at file-load time) to dodge a
-- require cycle: this sub_module is required by window_service.lua, which
-- max_core.lua's own service bootstrap (RunnerService/ResolveService) can
-- load WHILE max_core.lua itself is still mid-load (its `return` is the
-- very last line of the file). require("max_core") called from in here at
-- THAT moment hands back an incomplete/placeholder module instead of the
-- real one -- ".call()" on it then indexes something that isn't there
-- yet, which is the "attempt to index a nil value" RunnerService Job
-- error. Deferring the require until the first GameObject is actually
-- constructed guarantees max_core.lua has long since finished loading.
local _EventClass = nil
local function NewCollidedEvent()
    if not _EventClass then
        _EventClass = require("max_core").call().Event
    end
    return _EventClass.new("Collided")
end

---@class GameObject
---@field Position { x: number, y: number, z: number } World position offset
---@field Rotation { x: number, y: number, z: number } Euler rotations (radians)
---@field Scale { x: number, y: number, z: number } Object scale multipliers
---@field Color { r: integer, g: integer, b: integer } RGB rendering color (0-255)
---@field Visible boolean Lifecycle visibility toggle
---@field _physicsBody PhysicsBody? Set by WindowObject:BindPhysics; nil until this object is bound to physics
---@field Collided Event Fires (self:Fire(otherGameObject)) every Step for as long as this object's PhysicsBody stays touching another's -- only meaningful once BindPhysics has been called; see WindowObject:StepPhysics
local GameObject = {}
GameObject.__index = GameObject
InstanceTyping.SetType(GameObject, "GameObject")

---@return GameObject
function GameObject.new()
    local self = setmetatable({}, GameObject)
    self.Position = { x = 0.0, y = 0.0, z = 0.0 }
    self.Rotation = { x = 0.0, y = 0.0, z = 0.0 }
    self.Scale = { x = 1.0, y = 1.0, z = 1.0 }
    self.Color = { r = 255, g = 255, b = 255 }
    self.Visible = true
    self.Collided = NewCollidedEvent()
    return self
end

---Sets position coordinates.
---@param x number?
---@param y number?
---@param z number?
function GameObject:SetPosition(x, y, z)
    self.Position.x, self.Position.y, self.Position.z = x or 0.0, y or 0.0, z or 0.0
end

---Sets euler rotation angles in radians.
---@param rx number?
---@param ry number?
---@param rz number?
function GameObject:SetRotation(rx, ry, rz)
    self.Rotation.x, self.Rotation.y, self.Rotation.z = rx or 0.0, ry or 0.0, rz or 0.0
end

---Sets scale components.
---@param sx number?
---@param sy number?
---@param sz number?
function GameObject:SetScale(sx, sy, sz)
    self.Scale.x, self.Scale.y, self.Scale.z = sx or 1.0, sy or 1.0, sz or 1.0
end

---Sets RGB color values.
---@param r integer?
---@param g integer?
---@param b integer?
function GameObject:SetColor(r, g, b)
    self.Color.r, self.Color.g, self.Color.b = r or 255, g or 255, b or 255
end

---Gets world position coordinates as a tuple.
---@return number, number, number
function GameObject:GetPosition()
    return self.Position.x, self.Position.y, self.Position.z
end

---Gets euler rotation angles in radians as a tuple.
---@return number, number, number
function GameObject:GetRotation()
    return self.Rotation.x, self.Rotation.y, self.Rotation.z
end

---Gets scale components as a tuple.
---@return number, number, number
function GameObject:GetScale()
    return self.Scale.x, self.Scale.y, self.Scale.z
end

---Gets RGB color values as a tuple.
---@return integer, integer, integer
function GameObject:GetColor()
    return self.Color.r, self.Color.g, self.Color.b
end

---Whether this object's bound PhysicsBody is CURRENTLY touching another
---body (see PhysicsBody:IsColliding) -- false if physics was never bound
---via WindowObject:BindPhysics. `Collided` fires this same info as an
---event (every Step the touch persists) if you'd rather react than poll.
---@return boolean
function GameObject:IsColliding()
    if not self._physicsBody then return false end
    return self._physicsBody:IsColliding()
end

---Virtual render method to be overridden by derived objects.
---@param window WindowObject
function GameObject:Render(window) end

-- =========================================================================
-- RECT OBJECT
-- =========================================================================

---@class RectObject : GameObject
---@field Size { w: number, h: number }
local RectObject = setmetatable({}, { __index = GameObject })
RectObject.__index = RectObject
InstanceTyping.SetType(RectObject, "RectObject")

---@param x number?
---@param y number?
---@param w number?
---@param h number?
---@param r integer?
---@param g integer?
---@param b integer?
---@return RectObject
function RectObject.new(x, y, w, h, r, g, b)
    local self = setmetatable({
        Position = { x = 0.0, y = 0.0, z = 0.0 },
        Rotation = { x = 0.0, y = 0.0, z = 0.0 },
        Scale = { x = 1.0, y = 1.0, z = 1.0 },
        Color = { r = 255, g = 255, b = 255 },
        Visible = true,
        Collided = NewCollidedEvent()
    }, RectObject)
    self:SetPosition(x or 0, y or 0, 0)
    self.Size = { w = w or 100, h = h or 100 }
    self:SetColor(r or 255, g or 255, b or 255)
    return self
end

function RectObject:Render(window)
    if not self.Visible then return end
    window:DrawRectangle(
        math.floor(self.Position.x),
        math.floor(self.Position.y),
        math.floor(self.Size.w * self.Scale.x),
        math.floor(self.Size.h * self.Scale.y),
        self.Color.r,
        self.Color.g,
        self.Color.b
    )
end

-- =========================================================================
-- CIRCLE OBJECT
-- =========================================================================

---@class CircleObject : GameObject
---@field Radius number
---@field Fill boolean
local CircleObject = setmetatable({}, { __index = GameObject })
CircleObject.__index = CircleObject
InstanceTyping.SetType(CircleObject, "CircleObject")

---@param cx number?
---@param cy number?
---@param radius number?
---@param fill boolean?
---@param r integer?
---@param g integer?
---@param b integer?
---@return CircleObject
function CircleObject.new(cx, cy, radius, fill, r, g, b)
    local self = setmetatable({
        Position = { x = 0.0, y = 0.0, z = 0.0 },
        Rotation = { x = 0.0, y = 0.0, z = 0.0 },
        Scale = { x = 1.0, y = 1.0, z = 1.0 },
        Color = { r = 255, g = 255, b = 255 },
        Visible = true,
        Collided = NewCollidedEvent()
    }, CircleObject)
    self:SetPosition(cx or 0, cy or 0, 0)
    self.Radius = radius or 50
    self.Fill = fill or false
    self:SetColor(r or 255, g or 255, b or 255)
    return self
end

function CircleObject:Render(window)
    if not self.Visible then return end
    local avgScale = (self.Scale.x + self.Scale.y) / 2
    window:DrawCircle(
        math.floor(self.Position.x),
        math.floor(self.Position.y),
        math.floor(self.Radius * avgScale),
        self.Fill,
        self.Color.r,
        self.Color.g,
        self.Color.b
    )
end

-- =========================================================================
-- LINE OBJECT
-- =========================================================================

---@class LineObject : GameObject
---@field EndPosition { x: number, y: number }
local LineObject = setmetatable({}, { __index = GameObject })
LineObject.__index = LineObject
InstanceTyping.SetType(LineObject, "LineObject")

---@param x0 number?
---@param y0 number?
---@param x1 number?
---@param y1 number?
---@param r integer?
---@param g integer?
---@param b integer?
---@return LineObject
function LineObject.new(x0, y0, x1, y1, r, g, b)
    local self = setmetatable({
        Position = { x = 0.0, y = 0.0, z = 0.0 },
        Rotation = { x = 0.0, y = 0.0, z = 0.0 },
        Scale = { x = 1.0, y = 1.0, z = 1.0 },
        Color = { r = 255, g = 255, b = 255 },
        Visible = true,
        Collided = NewCollidedEvent()
    }, LineObject)
    self:SetPosition(x0 or 0, y0 or 0, 0)
    self.EndPosition = { x = x1 or 100, y = y1 or 100 }
    self:SetColor(r or 255, g or 255, b or 255)
    return self
end

function LineObject:Render(window)
    if not self.Visible then return end
    window:DrawLine(
        math.floor(self.Position.x),
        math.floor(self.Position.y),
        math.floor(self.EndPosition.x),
        math.floor(self.EndPosition.y),
        self.Color.r,
        self.Color.g,
        self.Color.b
    )
end

-- =========================================================================
-- TEXT OBJECT
-- =========================================================================

---@class TextObject : GameObject
---@field Text string
---@field TextScale integer
---@field Quality integer Sampling/anti-aliasing level forwarded to WindowObject:DrawText (0 = legacy blocky rendering, default)
local TextObject = setmetatable({}, { __index = GameObject })
TextObject.__index = TextObject
InstanceTyping.SetType(TextObject, "TextObject")

---@param text string?
---@param x number?
---@param y number?
---@param scale integer?
---@param r integer?
---@param g integer?
---@param b integer?
---@param quality integer? Sampling/anti-aliasing level (default 0 = legacy blocky rendering)
---@return TextObject
function TextObject.new(text, x, y, scale, r, g, b, quality)
    local self = setmetatable({
        Position = { x = 0.0, y = 0.0, z = 0.0 },
        Rotation = { x = 0.0, y = 0.0, z = 0.0 },
        Scale = { x = 1.0, y = 1.0, z = 1.0 },
        Color = { r = 255, g = 255, b = 255 },
        Visible = true,
        Collided = NewCollidedEvent(),
        -- Declared directly in the literal (same missing-fields fix as
        -- CubeObject.Wireframe / MeshObject.Vertices/Faces) rather than
        -- assigned after setmetatable.
        Quality = quality or 0
    }, TextObject)
    self.Text = text or ""
    self:SetPosition(x or 0, y or 0, 0)
    self.TextScale = scale or 1
    self:SetColor(r or 255, g or 255, b or 255)
    return self
end

---Sets string content for the text object.
---@param text string
function TextObject:SetText(text)
    self.Text = text or string.char(0)
end

---Sets the sampling/anti-aliasing quality level (0 = legacy blocky).
---@param quality integer
function TextObject:SetQuality(quality)
    self.Quality = quality or 0
end

function TextObject:Render(window)
    if not self.Visible or #self.Text == 0 then return end
    window:DrawText(
        self.Text,
        math.floor(self.Position.x),
        math.floor(self.Position.y),
        math.floor(self.TextScale * self.Scale.x),
        self.Color.r,
        self.Color.g,
        self.Color.b,
        self.Quality
    )
end

-- =========================================================================
-- POLYGON OBJECT
-- =========================================================================

---@class PolygonObject : GameObject
---@field Points table
---@field Fill boolean
local PolygonObject = setmetatable({}, { __index = GameObject })
PolygonObject.__index = PolygonObject
InstanceTyping.SetType(PolygonObject, "PolygonObject")

---@param points table? Vertex points array (nested {{x,y}...} or flat {x1,y1,x2,y2...})
---@param fill boolean? Fill shape toggle
---@param r integer?
---@param g integer?
---@param b integer?
---@return PolygonObject
function PolygonObject.new(points, fill, r, g, b)
    local self = setmetatable({
        Position = { x = 0.0, y = 0.0, z = 0.0 },
        Rotation = { x = 0.0, y = 0.0, z = 0.0 },
        Scale = { x = 1.0, y = 1.0, z = 1.0 },
        Color = { r = 255, g = 255, b = 255 },
        Visible = true,
        Collided = NewCollidedEvent()
    }, PolygonObject)
    self.Points = points or {}
    self.Fill = fill or false
    self:SetColor(r or 255, g or 255, b or 255)
    return self
end

---Sets vertex points for the polygon object.
---@param points table
function PolygonObject:SetPoints(points)
    self.Points = points or {}
end

function PolygonObject:Render(window)
    if not self.Visible or #self.Points == 0 then return end
    window:DrawPolygon(
        self.Points,
        self.Color.r,
        self.Color.g,
        self.Color.b,
        self.Fill
    )
end

-- =========================================================================
-- IMAGE OBJECT
-- =========================================================================

---@class ImageObject : GameObject
---@field ImageId integer?
local ImageObject = setmetatable({}, { __index = GameObject })
ImageObject.__index = ImageObject
InstanceTyping.SetType(ImageObject, "ImageObject")

---@param imageId integer?
---@param x number?
---@param y number?
---@return ImageObject
function ImageObject.new(imageId, x, y)
    local self = setmetatable({
        Position = { x = 0.0, y = 0.0, z = 0.0 },
        Rotation = { x = 0.0, y = 0.0, z = 0.0 },
        Scale = { x = 1.0, y = 1.0, z = 1.0 },
        Color = { r = 255, g = 255, b = 255 },
        Visible = true,
        Collided = NewCollidedEvent()
    }, ImageObject)
    self.ImageId = imageId
    self:SetPosition(x or 0, y or 0, 0)
    return self
end

function ImageObject:Render(window)
    if not self.Visible or not self.ImageId then return end
    window:DrawImage(
        self.ImageId,
        math.floor(self.Position.x),
        math.floor(self.Position.y)
    )
end

-- =========================================================================
-- CUBE OBJECT
-- =========================================================================

---@alias CubeFillMode
---| '"wireframe"' # outline edges only
---| '"solid"' # filled, depth-sorted faces
---| '"point"' # a dot at each of the 8 vertices

---@class CubeObject : GameObject
---@field Wireframe CubeFillMode|boolean|integer Fill mode forwarded as-is to WindowObject:DrawCube. Kept as the original field name for backward compat with any code poking it directly -- it now also accepts "wireframe"|"solid"|"point"|integer, not just a plain boolean.
local CubeObject = setmetatable({}, { __index = GameObject })
CubeObject.__index = CubeObject
InstanceTyping.SetType(CubeObject, "CubeObject")

---@param px number?
---@param py number?
---@param pz number?
---@param size number?
---@param r integer?
---@param g integer?
---@param b integer?
---@param fillMode CubeFillMode|boolean|integer? "wireframe"|"solid"|"point" (default "wireframe", matching this object's original default behavior)
---@return CubeObject
function CubeObject.new(px, py, pz, size, r, g, b, fillMode)
    local self = setmetatable({
        Position = { x = 0.0, y = 0.0, z = 0.0 },
        Rotation = { x = 0.0, y = 0.0, z = 0.0 },
        Scale = { x = 1.0, y = 1.0, z = 1.0 },
        Color = { r = 255, g = 255, b = 255 },
        Visible = true,
        Collided = NewCollidedEvent(),
        -- Declared in the literal (not bolted on after setmetatable) so
        -- the class's required @field Wireframe is satisfied and the
        -- linter's missing-fields check stays quiet. Preserves the
        -- original default (plain `true` -> wireframe) when no fillMode
        -- is given, while accepting the richer string/int modes too.
        Wireframe = fillMode == nil and true or fillMode
    }, CubeObject)
    self:SetPosition(px or 0.0, py or 0.0, pz or 3.0)
    local s = size or 1.0
    self:SetScale(s, s, s)
    self:SetColor(r or 0, g or 255, b or 0)
    return self
end

---Sets this cube's fill mode ("wireframe"|"solid"|"point", or legacy boolean/int).
---@param fillMode CubeFillMode|boolean|integer
function CubeObject:SetFillMode(fillMode)
    self.Wireframe = fillMode
end

---Gets this cube's current fill mode value.
---@return CubeFillMode|boolean|integer
function CubeObject:GetFillMode()
    return self.Wireframe
end

---Legacy convenience shim: toggle plain wireframe on/off. Prefer
---SetFillMode("solid"|"point") for the newer modes.
---@param enabled boolean
function CubeObject:SetWireframe(enabled)
    self.Wireframe = enabled and "wireframe" or "solid"
end

function CubeObject:Render(window)
    if not self.Visible then return end
    window:DrawCube(
        self.Position.x, self.Position.y, self.Position.z,
        self.Rotation.x, self.Rotation.y, self.Rotation.z,
        self.Scale.x, self.Scale.y, self.Scale.z,
        self.Color.r, self.Color.g, self.Color.b,
        self.Wireframe
    )
end

-- =========================================================================
-- MESH OBJECT (custom 3D vertices + faces -- the 3D equivalent of PolygonObject)
-- =========================================================================

---@class MeshObject : GameObject
---@field Vertices Vertex3[] Local-space vertex list, e.g. {{0,0,0}, {1,0,0}, ...}
---@field Faces MeshFace[] Each face is a list of 1-based vertex indices (3+), wound counter-clockwise viewed from outside for correct shading
---@field FillMode CubeFillMode|boolean|integer
local MeshObject = setmetatable({}, { __index = GameObject })
MeshObject.__index = MeshObject
InstanceTyping.SetType(MeshObject, "MeshObject")

---@param vertices Vertex3[]? Local-space vertex list
---@param faces MeshFace[]? Each face is a list of 1-based vertex indices (3+)
---@param r integer?
---@param g integer?
---@param b integer?
---@param fillMode CubeFillMode|boolean|integer? "wireframe"|"solid"|"point" (default "solid")
---@return MeshObject
function MeshObject.new(vertices, faces, r, g, b, fillMode)
    local self = setmetatable({
        Position = { x = 0.0, y = 0.0, z = 0.0 },
        Rotation = { x = 0.0, y = 0.0, z = 0.0 },
        Scale = { x = 1.0, y = 1.0, z = 1.0 },
        Color = { r = 255, g = 255, b = 255 },
        Visible = true,
        Collided = NewCollidedEvent(),
        -- Declared directly in the literal (same fix applied to CubeObject's
        -- Wireframe field) so the class's required fields are satisfied and
        -- the linter's missing-fields check stays quiet.
        Vertices = vertices or {},
        Faces = faces or {},
        FillMode = fillMode == nil and "solid" or fillMode
    }, MeshObject)
    self:SetColor(r or 255, g or 255, b or 255)
    return self
end

---Replaces this mesh's vertex list.
---@param vertices Vertex3[]
function MeshObject:SetVertices(vertices)
    self.Vertices = vertices or {}
end

---Replaces this mesh's face list.
---@param faces MeshFace[]
function MeshObject:SetFaces(faces)
    self.Faces = faces or {}
end

---Sets this mesh's fill mode ("wireframe"|"solid"|"point", or legacy boolean/int).
---@param fillMode CubeFillMode|boolean|integer
function MeshObject:SetFillMode(fillMode)
    self.FillMode = fillMode
end

---Gets this mesh's current fill mode value.
---@return CubeFillMode|boolean|integer
function MeshObject:GetFillMode()
    return self.FillMode
end

function MeshObject:Render(window)
    if not self.Visible or #self.Vertices == 0 or #self.Faces == 0 then return end
    window:DrawMesh(
        self.Vertices, self.Faces,
        self.Position.x, self.Position.y, self.Position.z,
        self.Rotation.x, self.Rotation.y, self.Rotation.z,
        self.Scale.x, self.Scale.y, self.Scale.z,
        self.Color.r, self.Color.g, self.Color.b,
        self.FillMode
    )
end

return {
    GameObject = GameObject,
    RectObject = RectObject,
    CircleObject = CircleObject,
    LineObject = LineObject,
    TextObject = TextObject,
    PolygonObject = PolygonObject,
    ImageObject = ImageObject,
    CubeObject = CubeObject,
    MeshObject = MeshObject
};
