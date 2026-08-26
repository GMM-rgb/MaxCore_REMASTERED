-- =========================================================================
-- services/sub_modules/game_object.lua
-- =========================================================================
local InstanceTyping = require("instance_type")

---@class GameObject
---@field Position { x: number, y: number, z: number } World position offset
---@field Rotation { x: number, y: number, z: number } Euler rotations (radians)
---@field Scale { x: number, y: number, z: number } Object scale multipliers
---@field Color { r: integer, g: integer, b: integer } RGB rendering color (0-255)
---@field Visible boolean Lifecycle visibility toggle
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
        Visible = true
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
        Visible = true
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
        Visible = true
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
---@return TextObject
function TextObject.new(text, x, y, scale, r, g, b)
    local self = setmetatable({
        Position = { x = 0.0, y = 0.0, z = 0.0 },
        Rotation = { x = 0.0, y = 0.0, z = 0.0 },
        Scale = { x = 1.0, y = 1.0, z = 1.0 },
        Color = { r = 255, g = 255, b = 255 },
        Visible = true
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

function TextObject:Render(window)
    if not self.Visible or #self.Text == 0 then return end
    window:DrawText(
        self.Text,
        math.floor(self.Position.x),
        math.floor(self.Position.y),
        math.floor(self.TextScale * self.Scale.x),
        self.Color.r,
        self.Color.g,
        self.Color.b
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
        Visible = true
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
        Visible = true
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

---@class CubeObject : GameObject
---@field Wireframe boolean
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
---@return CubeObject
function CubeObject.new(px, py, pz, size, r, g, b)
    local self = setmetatable({
        Position = { x = 0.0, y = 0.0, z = 0.0 },
        Rotation = { x = 0.0, y = 0.0, z = 0.0 },
        Scale = { x = 1.0, y = 1.0, z = 1.0 },
        Color = { r = 255, g = 255, b = 255 },
        Visible = true
    }, CubeObject)
    self:SetPosition(px or 0.0, py or 0.0, pz or 3.0)
    local s = size or 1.0
    self:SetScale(s, s, s)
    self:SetColor(r or 0, g or 255, b or 0)
    self.Wireframe = true
    return self
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

return {
    GameObject = GameObject,
    RectObject = RectObject,
    CircleObject = CircleObject,
    LineObject = LineObject,
    TextObject = TextObject,
    PolygonObject = PolygonObject,
    ImageObject = ImageObject,
    CubeObject = CubeObject
};
