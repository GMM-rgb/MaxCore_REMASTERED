-- =========================================================================
-- services/sub_modules/game_object.lua
-- =========================================================================
local InstanceTyping = require("instance_type")
local PseudoStyles = require("sub_modules.pseudo_styles")

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

-- Minimal structural type for the `window` argument every Render method
-- receives. WindowObject itself is declared in window_service.lua, which
-- requires THIS file, so naming it here would be a require cycle; this
-- interface lists only the drawing calls objects actually use, and
-- WindowObject satisfies it structurally.
---@class RenderSurface
---@field DrawRectangle fun(self: RenderSurface, x: number, y: number, w: number, h: number, r: integer, g: integer, b: integer)
---@field DrawLine fun(self: RenderSurface, x0: number, y0: number, x1: number, y1: number, r: integer, g: integer, b: integer)
---@field DrawCircle fun(self: RenderSurface, cx: number, cy: number, radius: number, fill: boolean, r: integer, g: integer, b: integer)
---@field DrawPolygon fun(self: RenderSurface, pointsOrFill: table|boolean, ...: any)
---@field DrawImage fun(self: RenderSurface, imageId: integer, x: number, y: number)
---@field DrawImageScaled fun(self: RenderSurface, imageId: integer, x: number, y: number, destW: number, destH: number)
---@field DestroyImage fun(self: RenderSurface, imageId: integer?): boolean
---@field CreateTextureSet fun(self: RenderSurface): integer?
---@field BindTexture fun(self: RenderSurface, setId: integer, slot: integer, imageId: integer, options: TextureOptions?): boolean
---@field DestroyTextureSet fun(self: RenderSurface, setId: integer?): boolean
---@field RenderTextToImage fun(self: RenderSurface, existingImgId: integer?, text: string, scale: integer, r: integer, g: integer, b: integer, quality: integer?, strokeR: integer?, strokeG: integer?, strokeB: integer?, strokeWidth: integer?): integer?, integer?, integer?
---@field DrawCube fun(self: RenderSurface, px: number, py: number, pz: number, rx: number, ry: number, rz: number, sx: number, sy: number, sz: number, r: integer, g: integer, b: integer, fillMode: any, textureSetId: integer?)
---@field DrawMesh fun(self: RenderSurface, vertices: table, faces: table, px: number, py: number, pz: number, rx: number, ry: number, rz: number, sx: number, sy: number, sz: number, r: integer, g: integer, b: integer, fillMode: any, textureSetId: integer?, uvs: table?)


---@class GameObject
---@field Position { x: number, y: number, z: number } World position offset
---@field Rotation { x: number, y: number, z: number } Euler rotations (radians)
---@field Scale { x: number, y: number, z: number } Object scale multipliers
---@field Color { r: integer, g: integer, b: integer } RGB rendering color (0-255)
---@field Visible boolean Lifecycle visibility toggle
---@field _physicsBody PhysicsBody? Set by WindowObject:BindPhysics; nil until this object is bound to physics
---@field Collided Event Fires (self:Fire(otherGameObject)) every Step for as long as this object's PhysicsBody stays touching another's -- only meaningful once BindPhysics has been called; see WindowObject:StepPhysics
---@field Vertices Vertex3[]? Only present on MeshObject (declared here, nilable, so code that takes a plain GameObject -- e.g. WindowObject:BindPhysics's {shape="hull"} path -- can check gameObject.Vertices without the language server complaining); see MeshObject's own (non-nilable) override below
---@field Faces MeshFace[]? Only present on MeshObject, same reasoning as Vertices above
---@field Styles table<string, PseudoStyle> Attached style elements, one per kind (see PseudoStyles)
---@field TypeName string Class name, used to check which styles an object accepts
local GameObject = {}
GameObject.__index = GameObject
InstanceTyping.SetType(GameObject, "GameObject")

-- Forward-declared: ImageObject is defined further down in this file,
-- but TextObject.new (defined before it) needs to reference it to
-- inject an ImageObject preset for its bitmap. Without this forward
-- local, TextObject.new's reference to ImageObject would resolve to a
-- nil GLOBAL instead of the later `local ImageObject` -- a real bug,
-- caught before shipping by re-checking Lua's scoping rules here rather
-- than assuming closures just work across declaration order.
--
-- The class is declared HERE, on the variable itself, so the language server
-- binds ImageObject to this one local. The fields and methods are defined
-- further down at the "IMAGE OBJECT" section.
---@class ImageObject : GameObject
---@field ImageId integer?
---@field private _destWidth number? Set via SetSize -- when present, Render uses DrawImageScaled instead of a 1:1 DrawImage blit
---@field private _destHeight number?
local ImageObject

---@return GameObject
function GameObject.new()
    local self = setmetatable({}, GameObject)
    self.Position = { x = 0.0, y = 0.0, z = 0.0 }
    self.Rotation = { x = 0.0, y = 0.0, z = 0.0 }
    self.Scale = { x = 1.0, y = 1.0, z = 1.0 }
    self.Color = { r = 255, g = 255, b = 255 }
    self.Visible = true
    self.Collided = NewCollidedEvent()
    self.Styles = {}
    return self
end

---Attaches a style element made by PseudoStyles.new. One style per kind:
---applying a second "border" replaces the first. Returns the object so
---calls chain. Errors if this object type doesn't accept that kind.
---@generic T: GameObject
---@param self T
---@param style PseudoStyle
---@return T
function GameObject:ApplyStyle(style)
    assert(type(style) == "table" and style.Kind, "ApplyStyle expects a style from PseudoStyles.new")
    local typeName = self.TypeName or "GameObject"
    -- Checks the whole class lineage, so a custom class that extends
    -- CubeObject/MeshObject/RectObject... is accepted under its own name.
    if not PseudoStyles.AcceptsObject(style, self) then
        error(("%s cannot take a %q style (accepts: %s, or any class that extends one of them)"):format(
            typeName, style.Kind, table.concat(style.Targets, ", ")), 2)
    end
    local old = self.Styles[style.Kind]
    if old and old ~= style and old.Destroy then old:Destroy() end
    self.Styles[style.Kind] = style
    local hook = self["_on" .. style.Kind:sub(1, 1):upper() .. style.Kind:sub(2) .. "Applied"]
    if hook then hook(self, style) end
    return self
end

---Puts an image on this object. The same thing as
---`obj:ApplyStyle(PseudoStyles.new("texture", imageId, face, options))`,
---but reads the way you would expect: bind the texture to the object.
---
---    coin:SetTexture(img)                  -- every face
---    coin:SetTexture(img, "top")           -- one face (cube: front back top bottom right left)
---    coin:SetTexture(img, {"left","right"})-- several faces, same image
---    mesh:SetTexture(img, 2)               -- mesh faces are numbered, 1 = first face in the list
---    coin:SetTexture(img, "all", { filter = "bilinear", wrap = "repeat", uScale = 2, vScale = 2 })
---
---Binding another face keeps the faces already set: SetTexture(a, "top")
---then SetTexture(b, "bottom") gives a top AND a bottom. Pass nil as the
---image to remove every texture from the object. A class of your own that
---extends CubeObject, MeshObject, RectObject... can call this too.
---@generic T: GameObject
---@param self T
---@param imageId integer? An image from window:CreateImage, or nil to remove the texture
---@param face (PseudoFace|integer|(PseudoFace|integer)[])? Default "all"
---@param options TextureOptions?
---@return T
function GameObject:SetTexture(imageId, face, options)
    if imageId == nil then return self:RemoveStyle("texture") end
    local fresh = PseudoStyles.new("texture", imageId, face, options)
    local old = self.Styles.texture --[[@as TextureStyle?]]
    if old then
        -- Keep the faces that were already bound, then add / replace these.
        for key, binding in pairs(old.Faces) do
            if fresh.Faces[key] == nil then fresh.Faces[key] = binding end
        end
    end
    return self:ApplyStyle(fresh)
end

---Detaches the style of this kind, if any, and frees anything it owned.
---@generic T: GameObject
---@param self T
---@param kind PseudoStyleType
---@return T
function GameObject:RemoveStyle(kind)
    local old = self.Styles[kind]
    if old then
        self.Styles[kind] = nil
        local hook = self["_on" .. kind:sub(1, 1):upper() .. kind:sub(2) .. "Removed"]
        if hook then hook(self, old) end
        if old.Destroy then old:Destroy() end
    end
    return self
end

---Gets the attached style of this kind, or nil.
---@param kind PseudoStyleType
---@return PseudoStyle?
function GameObject:GetStyle(kind)
    return self.Styles[kind]
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

---Releases anything native this object owns (e.g. a cached bitmap).
---Base objects own nothing, so this is a no-op; TextObject/ImageObject
---override it. Always safe to call more than once.
function GameObject:Destroy() end

---Virtual render method to be overridden by derived objects.
---@param window RenderSurface
function GameObject:Render(window) end


---Objects that can draw an outline (RectObject, CircleObject, PolygonObject).
---Declared once so the border hooks below are typed against the fields they set.
---@class BorderedObject : GameObject
---@field BorderColor { r: integer, g: integer, b: integer }? Outline color, nil = no border
---@field BorderWidth integer Border thickness in pixels (only used when BorderColor is set)


-- Shared "border" style hooks. A border style just fills the BorderColor /
-- BorderWidth fields that RectObject/CircleObject/PolygonObject:Render
-- already read, so the border renderers themselves don't change.
---@param class table A class table with BorderColor/BorderWidth fields (RectObject, CircleObject, PolygonObject)
local function InstallBorderHooks(class)
    ---@param self BorderedObject
    ---@param style BorderStyle
    function class:_onBorderApplied(style)
        self.BorderColor = { r = style.Color.r, g = style.Color.g, b = style.Color.b }
        self.BorderWidth = style.Width
    end
    ---@param self BorderedObject
    function class:_onBorderRemoved()
        self.BorderColor = nil
    end
    ---Backward-compatible shim: same signature as before, now routed
    ---through the style system. Pass r=nil to remove the border.
    ---@param r integer? Border red, or nil to remove the border entirely
    ---@param g integer?
    ---@param b integer?
    ---@param width integer? Border width in pixels (default 1)
    function class:SetBorder(r, g, b, width)
        if r == nil then
            self:RemoveStyle("border")
        else
            self:ApplyStyle(PseudoStyles.new("border", r, g, b, width))
        end
    end
end

-- =========================================================================
-- RECT OBJECT
-- =========================================================================

---@class RectObject : GameObject
---@field Size { w: number, h: number }
---@field BorderColor { r: integer, g: integer, b: integer }? Outline color, nil = no border
---@field BorderWidth integer Border thickness in pixels (only used when BorderColor is set)
local RectObject = setmetatable({}, { __index = GameObject })
RectObject.__index = RectObject
InstanceTyping.SetType(RectObject, "RectObject")
RectObject.TypeName = "RectObject"
InstallBorderHooks(RectObject)

---@param x number?
---@param y number?
---@param w number?
---@param h number?
---@param r integer?
---@param g integer?
---@param b integer?
---@return RectObject
function RectObject.new(x, y, w, h, r, g, b)
    local self = setmetatable(GameObject.new(), RectObject) --[[@as RectObject]]
    self:SetPosition(x or 0, y or 0, 0)
    self.Size = { w = w or 100, h = h or 100 }
    self:SetColor(r or 255, g or 255, b or 255)
    self.BorderColor = nil
    self.BorderWidth = 1
    return self
end


---@param window RenderSurface
---@param x integer
---@param y integer
---@param w integer
---@param h integer
---@param radius integer Clamped to half the shorter side
---@param r integer
---@param g integer
---@param b integer
local function DrawRoundedRect(window, x, y, w, h, radius, r, g, b)
    if w <= 0 or h <= 0 then return end
    local rad = math.min(radius, math.floor(math.min(w, h) / 2))
    if rad <= 0 then
        window:DrawRectangle(x, y, w, h, r, g, b)
        return
    end
    -- Three rects cover everything except the four corner squares...
    window:DrawRectangle(x + rad, y, w - 2 * rad, h, r, g, b)             -- middle column, full height
    window:DrawRectangle(x, y + rad, rad, h - 2 * rad, r, g, b)           -- left strip
    window:DrawRectangle(x + w - rad, y + rad, rad, h - 2 * rad, r, g, b) -- right strip
    -- ...and a filled disc fills each corner square's rounded quarter.
    window:DrawCircle(x + rad, y + rad, rad, true, r, g, b)
    window:DrawCircle(x + w - rad - 1, y + rad, rad, true, r, g, b)
    window:DrawCircle(x + rad, y + h - rad - 1, rad, true, r, g, b)
    window:DrawCircle(x + w - rad - 1, y + h - rad - 1, rad, true, r, g, b)
end

---Draws the attached "texture" style (if any) stretched over a 2D rect.
---@param self GameObject
---@param window RenderSurface
---@param x integer
---@param y integer
---@param w integer
---@param h integer
local function DrawStyleTexture2D(self, window, x, y, w, h)
    local tex = self.Styles.texture --[[@as TextureStyle?]]
    if not tex or w <= 0 or h <= 0 then return end
    local binding = tex.Faces.all or select(2, next(tex.Faces))
    if binding and binding.imageId then
        window:DrawImageScaled(binding.imageId, x, y, w, h)
    end
end

---@param window RenderSurface
function RectObject:Render(window)
    if not self.Visible then return end
    local x, y = math.floor(self.Position.x), math.floor(self.Position.y)
    local w = math.floor(self.Size.w * self.Scale.x)
    local h = math.floor(self.Size.h * self.Scale.y)

    local corner = self.Styles.corner --[[@as CornerStyle?]]
    local radius = corner and corner.Radius or 0

    if self.BorderColor and self.BorderWidth > 0 then
        -- Outer shape in the border color, then the fill inset on top.
        -- With a corner style the outer shape is rounded and the inner
        -- radius shrinks by the border width, so the ring stays an even
        -- thickness around the curve instead of going square inside.
        local bw = self.BorderWidth
        DrawRoundedRect(window, x, y, w, h, radius, self.BorderColor.r, self.BorderColor.g, self.BorderColor.b)
        local innerW, innerH = w - 2 * bw, h - 2 * bw
        if innerW > 0 and innerH > 0 then
            DrawRoundedRect(window, x + bw, y + bw, innerW, innerH, math.max(0, radius - bw), self.Color.r, self.Color.g, self.Color.b)
            DrawStyleTexture2D(self, window, x + bw, y + bw, innerW, innerH)
        end
    else
        DrawRoundedRect(window, x, y, w, h, radius, self.Color.r, self.Color.g, self.Color.b)
        DrawStyleTexture2D(self, window, x, y, w, h)
    end
end

-- =========================================================================
-- CIRCLE OBJECT
-- =========================================================================

---@class CircleObject : GameObject
---@field Radius number
---@field Fill boolean
---@field BorderColor { r: integer, g: integer, b: integer }? Outline color, nil = no border
---@field BorderWidth integer Border thickness in pixels (only used when BorderColor is set)
local CircleObject = setmetatable({}, { __index = GameObject })
CircleObject.__index = CircleObject
InstanceTyping.SetType(CircleObject, "CircleObject")
CircleObject.TypeName = "CircleObject"
InstallBorderHooks(CircleObject)

---@param cx number?
---@param cy number?
---@param radius number?
---@param fill boolean?
---@param r integer?
---@param g integer?
---@param b integer?
---@return CircleObject
function CircleObject.new(cx, cy, radius, fill, r, g, b)
    local self = setmetatable(GameObject.new(), CircleObject) --[[@as CircleObject]]
    self:SetPosition(cx or 0, cy or 0, 0)
    self.Radius = radius or 50
    self.Fill = fill or false
    self:SetColor(r or 255, g or 255, b or 255)
    self.BorderColor = nil
    self.BorderWidth = 1
    return self
end


---@param window RenderSurface
function CircleObject:Render(window)
    if not self.Visible then return end
    local avgScale = (self.Scale.x + self.Scale.y) / 2
    local x, y = math.floor(self.Position.x), math.floor(self.Position.y)
    local radius = math.floor(self.Radius * avgScale)

    if self.BorderColor and self.BorderWidth > 0 then
        -- Same double-draw trick as RectObject: a filled outer disc in
        -- the border color, then the circle's own look (filled or
        -- outline, per self.Fill) drawn inset on top of it.
        window:DrawCircle(x, y, radius, true, self.BorderColor.r, self.BorderColor.g, self.BorderColor.b)
        local innerRadius = radius - self.BorderWidth
        if innerRadius > 0 then
            window:DrawCircle(x, y, innerRadius, self.Fill, self.Color.r, self.Color.g, self.Color.b)
        end
    else
        window:DrawCircle(x, y, radius, self.Fill, self.Color.r, self.Color.g, self.Color.b)
    end
end

-- =========================================================================
-- LINE OBJECT
-- =========================================================================

---@class LineObject : GameObject
---@field EndPosition { x: number, y: number }
local LineObject = setmetatable({}, { __index = GameObject })
LineObject.__index = LineObject
InstanceTyping.SetType(LineObject, "LineObject")
LineObject.TypeName = "LineObject"

---@param x0 number?
---@param y0 number?
---@param x1 number?
---@param y1 number?
---@param r integer?
---@param g integer?
---@param b integer?
---@return LineObject
function LineObject.new(x0, y0, x1, y1, r, g, b)
    local self = setmetatable(GameObject.new(), LineObject) --[[@as LineObject]]
    self:SetPosition(x0 or 0, y0 or 0, 0)
    self.EndPosition = { x = x1 or 100, y = y1 or 100 }
    self:SetColor(r or 255, g or 255, b or 255)
    return self
end

---@param window RenderSurface
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
---@field TextScale integer Display-time size multiplier -- cheap to change every frame, does NOT force the cached bitmap to rebuild (see Resolution below)
---@field Quality integer Sampling/anti-aliasing level baked into the cached bitmap (0 = legacy blocky rendering, default 0)
---@field Resolution integer Internal rasterization scale used when (re)building the cached bitmap -- higher gives denser source detail for crisper edges once scaled down to TextScale at render time (roughly "matches system font resolution" the higher this is). Default 4.
---@field StrokeColor { r: integer, g: integer, b: integer }? Outline color, nil = no stroke
---@field StrokeWidth integer Outline width in pixels (only used when StrokeColor is set)
---@field private _image ImageObject Injected ImageObject preset that actually blits the cached bitmap
---@field private _imageId integer? Native handle of the cached text bitmap (nil until first Render)
---@field private _cachedWidth integer Pixel width of the cached bitmap
---@field private _cachedHeight integer Pixel height of the cached bitmap
---@field private _ownerWindow RenderSurface? Window that built the cached bitmap, kept so Destroy/__gc can free it
---@field private _dirty boolean True when something baked into the bitmap changed and the next Render must rebuild it
local TextObject = setmetatable({}, { __index = GameObject })
TextObject.__index = TextObject
InstanceTyping.SetType(TextObject, "TextObject")
TextObject.TypeName = "TextObject"

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
    local self = setmetatable(GameObject.new(), TextObject) --[[@as TextObject]]
    self.Text = text or ""
    self:SetPosition(x or 0, y or 0, 0)
    self.TextScale = scale or 1
    self.Quality = quality or 0
    self.Resolution = 4
    self.StrokeColor = nil
    self.StrokeWidth = 1
    self:SetColor(r or 255, g or 255, b or 255)

    -- The actual cached bitmap is held by an INJECTED ImageObject preset
    -- rather than TextObject reimplementing its own DrawImage/
    -- DrawImageScaled dispatch -- ImageObject already IS "a GameObject
    -- that displays a bitmap," so that's what handles the pixels here;
    -- TextObject's own job is just building/rebuilding that bitmap's
    -- content via RenderTextToImage and telling the injected
    -- ImageObject where and how big to draw it.
    self._image = ImageObject.new(nil, x, y)

    -- Cached-render state -- nil/true until the first Render call
    -- actually builds the offscreen image via WindowObject:RenderTextToImage.
    -- Every setter that changes something baked INTO the bitmap (text,
    -- color, quality, stroke, resolution) flips _dirty back to true so
    -- the NEXT Render rebuilds it, reusing the SAME _imageId (see
    -- RenderTextToImage's existingImgId param) instead of leaking a
    -- fresh image every time. TextScale/Position/Scale changes do NOT
    -- mark dirty -- those are just a cheap scaled blit every frame.
    self._imageId = nil
    self._cachedWidth = 0
    self._cachedHeight = 0
    self._dirty = true
    return self
end

---Sets string content for the text object.
---@param text string
function TextObject:SetText(text)
    self.Text = text or string.char(0)
    self._dirty = true
end

---Sets the sampling/anti-aliasing quality level (0 = legacy blocky).
---@param quality integer
function TextObject:SetQuality(quality)
    self.Quality = quality or 0
    self._dirty = true
end

---Sets the internal rasterization resolution used to (re)build the
---cached bitmap -- higher values give crisper source detail (roughly
---matching a system font's density) at the cost of a bigger cached
---image; TextScale still controls the final on-screen size cheaply,
---independent of this.
---@param resolution integer
function TextObject:SetResolution(resolution)
    self.Resolution = resolution or 4
    self._dirty = true
end

---Sets (or removes, by passing r=nil) an outline/stroke around the text.
---@param r integer? Stroke red, or nil to remove the stroke entirely
---@param g integer?
---@param b integer?
---@param width integer? Stroke width in pixels (default 1)
function TextObject:SetStroke(r, g, b, width)
    if r == nil then
        self.StrokeColor = nil
    else
        self.StrokeColor = { r = r, g = g or 0, b = b or 0 }
        self.StrokeWidth = width or 1
    end
    self._dirty = true
end

---Overrides GameObject:SetColor -- fill color is baked into the cached
---bitmap, so changing it has to trigger a rebuild.
---@param r integer?
---@param g integer?
---@param b integer?
function TextObject:SetColor(r, g, b)
    GameObject.SetColor(self, r, g, b)
    self._dirty = true
end

---Frees this text's cached bitmap now. Safe to call repeatedly. After
---this the object can still be rendered; it just rebuilds its bitmap.
function TextObject:Destroy()
    if self._imageId and self._ownerWindow then
        self._ownerWindow:DestroyImage(self._imageId)
    end
    self._imageId = nil
    self._cachedWidth = 0
    self._cachedHeight = 0
    self._dirty = true
    if self._image then self._image.ImageId = nil end
end

-- Safety net for objects that are simply dropped without Destroy(): when
-- Lua collects the table, free the native bitmap too. Without this the
-- registry would keep every dropped TextObject's pixels until exit.
TextObject.__gc = function(self)
    pcall(TextObject.Destroy, self)
end

---@param window RenderSurface
function TextObject:Render(window)
    if not self.Visible then return end
    if #self.Text == 0 then
        -- Empty text has no bitmap. Release the old one instead of leaving
        -- it pinned (native side also frees it when handed an empty string).
        if self._imageId then self:Destroy() end
        return
    end
    self._ownerWindow = window

    if self._dirty or not self._imageId then
        local strokeR, strokeG, strokeB, strokeWidth
        if self.StrokeColor then
            strokeR, strokeG, strokeB = self.StrokeColor.r, self.StrokeColor.g, self.StrokeColor.b
            strokeWidth = self.StrokeWidth
        end
        local imgId, w, h = window:RenderTextToImage(
            self._imageId, self.Text, self.Resolution,
            self.Color.r, self.Color.g, self.Color.b, self.Quality,
            strokeR, strokeG, strokeB, strokeWidth
        )
        self._imageId = imgId
        self._cachedWidth = w or 0
        self._cachedHeight = h or 0
        self._dirty = false
        self._image.ImageId = imgId -- hand the freshly-(re)built bitmap to the injected ImageObject
    end

    if not self._imageId then return end -- empty text measures to nothing

    local destW = self._cachedWidth * (self.TextScale * self.Scale.x) / self.Resolution
    local destH = self._cachedHeight * (self.TextScale * self.Scale.y) / self.Resolution

    self._image:SetPosition(self.Position.x, self.Position.y, self.Position.z)
    self._image.Visible = self.Visible
    if math.floor(destW + 0.5) == self._cachedWidth and math.floor(destH + 0.5) == self._cachedHeight then
        self._image:ClearSize() -- exact native size -- cheap 1:1 blit, no resample needed
    else
        self._image:SetSize(destW, destH)
    end
    self._image:Render(window) -- delegate the actual blit to the injected ImageObject preset
end

-- =========================================================================
-- POLYGON OBJECT
-- =========================================================================

---@class PolygonObject : GameObject
---@field Points table
---@field Fill boolean
---@field BorderColor { r: integer, g: integer, b: integer }? Outline color, nil = no border
---@field BorderWidth integer Border thickness in pixels (only used when BorderColor is set)
local PolygonObject = setmetatable({}, { __index = GameObject })
PolygonObject.__index = PolygonObject
InstanceTyping.SetType(PolygonObject, "PolygonObject")
PolygonObject.TypeName = "PolygonObject"
InstallBorderHooks(PolygonObject)

---@param points table? Vertex points array (nested {{x,y}...} or flat {x1,y1,x2,y2...})
---@param fill boolean? Fill shape toggle
---@param r integer?
---@param g integer?
---@param b integer?
---@return PolygonObject
function PolygonObject.new(points, fill, r, g, b)
    local self = setmetatable(GameObject.new(), PolygonObject) --[[@as PolygonObject]]
    self.Points = points or {}
    self.Fill = fill or false
    self:SetColor(r or 255, g or 255, b or 255)
    self.BorderColor = nil
    self.BorderWidth = 1
    return self
end

---Sets vertex points for the polygon object.
---@param points table
function PolygonObject:SetPoints(points)
    self.Points = points or {}
end


---Draws one polygon edge as a "thick line" -- native draw_line is only
---ever 1px, so a border width > 1 is approximated by stacking parallel
---1px lines centered on the edge, offset along its perpendicular
---normal. Works for both convex and concave polygons since it's just
---tracing the edge itself, not reasoning about inside/outside.
---@param window RenderSurface
---@param x0 number
---@param y0 number
---@param x1 number
---@param y1 number
---@param width integer Border thickness in pixels
---@param r integer
---@param g integer
---@param b integer
local function DrawThickEdge(window, x0, y0, x1, y1, width, r, g, b)
    local dx, dy = x1 - x0, y1 - y0
    local len = math.sqrt(dx * dx + dy * dy)
    if len < 0.0001 then return end
    local nx, ny = -dy / len, dx / len -- unit perpendicular
    local half = (width - 1) / 2
    for i = 0, width - 1 do
        local offset = -half + i
        local ox, oy = nx * offset, ny * offset
        window:DrawLine(
            math.floor(x0 + ox), math.floor(y0 + oy),
            math.floor(x1 + ox), math.floor(y1 + oy),
            r, g, b
        )
    end
end

---@param window RenderSurface
function PolygonObject:Render(window)
    if not self.Visible or #self.Points == 0 then return end

    -- self.Points is LOCAL space (nested {{x,y},...} or flat
    -- {x1,y1,x2,y2,...}, both accepted -- see PolygonObject.new) --
    -- offset+scale each point by this object's own Position/Scale
    -- before handing off, exactly like every other 2D primitive
    -- (RectObject:Render, etc.) already does. Without this, SetPosition
    -- and SetScale had literally no effect on a polygon -- the native
    -- draw_polygon has no position/scale of its own, it just draws
    -- whatever raw coordinates it's given.
    local px, py = self.Position.x, self.Position.y
    local sx, sy = self.Scale.x, self.Scale.y
    local worldPoints = {}
    if type(self.Points[1]) == "table" then
        for i = 1, #self.Points do
            local p = self.Points[i]
            worldPoints[i] = { p[1] * sx + px, p[2] * sy + py }
        end
    else
        local j = 1
        for i = 1, #self.Points, 2 do
            worldPoints[j] = { self.Points[i] * sx + px, self.Points[i + 1] * sy + py }
            j = j + 1
        end
    end

    window:DrawPolygon(
        worldPoints,
        self.Color.r,
        self.Color.g,
        self.Color.b,
        self.Fill
    )

    if self.BorderColor and self.BorderWidth > 0 then
        local n = #worldPoints
        for i = 1, n do
            local p1 = worldPoints[i]
            local p2 = worldPoints[(i % n) + 1] -- wraps last vertex back to first, closing the loop
            DrawThickEdge(window, p1[1], p1[2], p2[1], p2[2], self.BorderWidth, self.BorderColor.r, self.BorderColor.g, self.BorderColor.b)
        end
    end
end

-- =========================================================================
-- IMAGE OBJECT
-- =========================================================================

-- No `local` here on purpose: ImageObject is forward-declared near the top of
-- the file (TextObject needs it before this point), so this assigns to that
-- local. Writing `local ImageObject` would create a second variable and leave
-- the forward-declared one nil. Its ---@class lives on that declaration.
ImageObject = setmetatable({}, { __index = GameObject }) --[[@as ImageObject]]
ImageObject.__index = ImageObject
InstanceTyping.SetType(ImageObject, "ImageObject")
ImageObject.TypeName = "ImageObject"

---@param imageId integer?
---@param x number?
---@param y number?
---@return ImageObject
function ImageObject.new(imageId, x, y)
    local self = setmetatable(GameObject.new(), ImageObject) --[[@as ImageObject]]
    self.ImageId = imageId
    self:SetPosition(x or 0, y or 0, 0)
    self._destWidth = nil
    self._destHeight = nil
    return self
end

---Draws this image at an explicit destination size (bilinear scaled)
---instead of its native pixel size. Other GameObjects that hold a
---bitmap (e.g. TextObject) compose an ImageObject internally and use
---this to display a cached image at whatever size they need, instead
---of every bitmap-holding object reimplementing its own blit dispatch.
---@param width number?
---@param height number?
function ImageObject:SetSize(width, height)
    self._destWidth = width
    self._destHeight = height
end

---Reverts to drawing at the image's native pixel size (plain DrawImage).
function ImageObject:ClearSize()
    self._destWidth = nil
    self._destHeight = nil
end

---@param window RenderSurface
function ImageObject:Render(window)
    if not self.Visible or not self.ImageId then return end
    local x, y = math.floor(self.Position.x), math.floor(self.Position.y)
    if self._destWidth and self._destHeight then
        window:DrawImageScaled(self.ImageId, x, y, math.floor(self._destWidth), math.floor(self._destHeight))
    else
        window:DrawImage(self.ImageId, x, y)
    end
end


-- =========================================================================
-- 3D "texture" STYLE (CubeObject / MeshObject)
-- =========================================================================
---State and methods shared by every object that can carry a 3D "texture"
---style (CubeObject, MeshObject). Declared once, ahead of the hooks that
---install the methods, so the language server knows they exist and which
---class owns them. The underscore fields are internal to this file.
---@class TexturedObject : GameObject
---@field _texSetId integer? Native texture set built from this object's texture style (nil until first Render)
---@field _texWindow RenderSurface? Window that owns _texSetId, kept so it can be freed from Destroy or __gc
---@field _texStyleDirty boolean True when the style changed and the native set must be rebuilt before the next draw
---@field _onTextureApplied fun(self: TexturedObject, style: TextureStyle)
---@field _onTextureRemoved fun(self: TexturedObject)
---@field _releaseTextureSet fun(self: TexturedObject)
---@field _textureSetFor fun(self: TexturedObject, window: RenderSurface): integer?
---@field Destroy fun(self: TexturedObject)


-- The native texture set is created lazily on first Render (that is the
-- first moment a window is available) and rebuilt whenever the style is
-- applied again. It is freed when the style is removed, replaced, or the
-- object is destroyed or garbage collected, so a dropped object can't
-- strand a set.

---@param class table A class table that inherits TexturedObject (CubeObject, MeshObject)
local function InstallTexture3DHooks(class)
    ---@param self TexturedObject
    ---@param style TextureStyle
    function class:_onTextureApplied(style)
        self:_releaseTextureSet()
        style.SetId = nil
        self._texStyleDirty = true
    end
    ---@param self TexturedObject
    function class:_onTextureRemoved()
        self:_releaseTextureSet()
    end
    ---@param self TexturedObject
    function class:_releaseTextureSet()
        if self._texSetId and self._texWindow then
            self._texWindow:DestroyTextureSet(self._texSetId)
        end
        self._texSetId = nil
        self._texStyleDirty = false
    end
    ---Returns the native texture set id for this object's texture style
    ---(building it if needed), or nil when there is no texture style.
    ---@param window RenderSurface
    ---@return integer?
    function class:_textureSetFor(window)
        local style = self.Styles.texture --[[@as TextureStyle?]]
        if not style then return nil end
        if self._texSetId and not self._texStyleDirty and self._texWindow == window then
            return self._texSetId
        end
        self:_releaseTextureSet()
        local setId = window:CreateTextureSet()
        if not setId then return nil end
        for face, binding in pairs(style.Faces) do
            local slot = PseudoStyles.SlotOf(face)
            if slot then window:BindTexture(setId, slot, binding.imageId, binding.options) end
        end
        self._texSetId = setId
        self._texWindow = window
        self._texStyleDirty = false
        style.SetId = setId
        return setId
    end
    ---@param self TexturedObject
    function class:Destroy()
        self:_releaseTextureSet()
    end
    ---@param self TexturedObject
    class.__gc = function(self) pcall(class.Destroy, self) end
end

-- =========================================================================
-- CUBE OBJECT
-- =========================================================================

---@alias CubeFillMode
---| '"wireframe"' # outline edges only
---| '"solid"' # filled, depth-sorted faces
---| '"point"' # a dot at each of the 8 vertices

---@class CubeObject : TexturedObject
---@field Wireframe CubeFillMode|boolean|integer Fill mode forwarded as-is to WindowObject:DrawCube. Kept as the original field name for backward compat with any code poking it directly -- it now also accepts "wireframe"|"solid"|"point"|integer, not just a plain boolean.
local CubeObject = setmetatable({}, { __index = GameObject })
CubeObject.__index = CubeObject
InstanceTyping.SetType(CubeObject, "CubeObject")
CubeObject.TypeName = "CubeObject"
InstallTexture3DHooks(CubeObject)

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
    local self = setmetatable(GameObject.new(), CubeObject) --[[@as CubeObject]]
    -- Preserves the original default (plain `true` -> wireframe) when no
    -- fillMode is given, while accepting the richer string/int modes too.
    self.Wireframe = (fillMode == nil and true or fillMode) --[[@as CubeFillMode|boolean|integer]]
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

---@param window RenderSurface
function CubeObject:Render(window)
    if not self.Visible then return end
    window:DrawCube(
        self.Position.x, self.Position.y, self.Position.z,
        self.Rotation.x, self.Rotation.y, self.Rotation.z,
        self.Scale.x, self.Scale.y, self.Scale.z,
        self.Color.r, self.Color.g, self.Color.b,
        self.Wireframe,
        self:_textureSetFor(window)
    )
end

-- =========================================================================
-- MESH OBJECT (custom 3D vertices + faces -- the 3D equivalent of PolygonObject)
-- =========================================================================

---@class MeshObject : TexturedObject
---@field Vertices Vertex3[] Local-space vertex list, e.g. {{0,0,0}, {1,0,0}, ...}
---@field Faces MeshFace[] Each face is a list of 1-based vertex indices (3+), wound counter-clockwise viewed from outside for correct shading
---@field FillMode CubeFillMode|boolean|integer
local MeshObject = setmetatable({}, { __index = GameObject })
MeshObject.__index = MeshObject
InstanceTyping.SetType(MeshObject, "MeshObject")
MeshObject.TypeName = "MeshObject"
InstallTexture3DHooks(MeshObject)

---@param vertices Vertex3[]? Local-space vertex list
---@param faces MeshFace[]? Each face is a list of 1-based vertex indices (3+)
---@param r integer?
---@param g integer?
---@param b integer?
---@param fillMode CubeFillMode|boolean|integer? "wireframe"|"solid"|"point" (default "solid")
---@return MeshObject
function MeshObject.new(vertices, faces, r, g, b, fillMode)
    local self = setmetatable(GameObject.new(), MeshObject) --[[@as MeshObject]]
    self.Vertices = vertices or {}
    self.Faces = faces or {}
    self.FillMode = (fillMode == nil and "solid" or fillMode) --[[@as CubeFillMode|boolean|integer]]
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

---Optional per-face UVs for the mesh's texture: uvs[faceIndex] = { {u,v}, ... }
---one pair per corner. Faces without an entry get UVs generated from their geometry.
---@return table?
function MeshObject:_meshUVs()
    local style = self.Styles.texture --[[@as TextureStyle?]]
    return style and style.UVs or nil
end

---@param window RenderSurface
function MeshObject:Render(window)
    if not self.Visible or #self.Vertices == 0 or #self.Faces == 0 then return end
    window:DrawMesh(
        self.Vertices, self.Faces,
        self.Position.x, self.Position.y, self.Position.z,
        self.Rotation.x, self.Rotation.y, self.Rotation.z,
        self.Scale.x, self.Scale.y, self.Scale.z,
        self.Color.r, self.Color.g, self.Color.b,
        self.FillMode,
        self:_textureSetFor(window),
        self:_meshUVs()
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
