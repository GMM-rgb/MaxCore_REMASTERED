-- =========================================================================
-- services/sub_modules/pseudo_styles.lua
-- =========================================================================
-- Style elements that can be attached to game objects: "border", "corner",
-- "texture". Build one with PseudoStyles.new(type, ...), attach it with
-- gameObject:ApplyStyle(style).
--
--   local border = PseudoStyles.new("border", 255, 255, 255, 2)
--   local corner = PseudoStyles.new("corner", 8)
--   rect:ApplyStyle(border):ApplyStyle(corner)
--
-- A style is plain data plus a kind. Rendering stays in each object's own
-- Render method (which reads the fields a style sets), so styles never
-- hold native resources of their own, except "texture", which holds a
-- native texture set and frees it in Destroy.

---@alias PseudoStyleType
---| '"border"' # outline around a 2D object: (r, g, b, width)
---| '"corner"' # rounded corners on a 2D rect: (radius)
---| '"texture"' # an image on a 2D object, or per-face images on a 3D object: (imageId, face?, options?)

---@alias PseudoFace
---| '"all"' # every face without its own binding
---| '"front"'
---| '"back"'
---| '"top"'
---| '"bottom"'
---| '"right"'
---| '"left"'

---@class PseudoStyle
---@field Kind PseudoStyleType
---@field Targets string[] Object type names this style may be applied to
---@field Destroy fun(self: PseudoStyle)? Releases anything native the style owns

---@class BorderStyle : PseudoStyle
---@field Color { r: integer, g: integer, b: integer }
---@field Width integer

---@class CornerStyle : PseudoStyle
---@field Radius integer

---@class TextureOptions
---@field uOffset number?
---@field vOffset number?
---@field uScale number?
---@field vScale number?
---@field wrap '"clamp"'|'"repeat"'|'"mirror"'?
---@field filter '"nearest"'|'"bilinear"'?

---@class TextureStyle : PseudoStyle
---@field Faces table<string, { imageId: integer, options: TextureOptions? }> face name -> binding
---@field SetId integer? Native texture set id (3D only, created lazily on first render)
---@field UVs table? Optional mesh UVs: UVs[faceIndex] = { {u,v}, ... }

---@class PseudoStylesModule
local PseudoStyles = {}

---Valid style kinds, as lowercase strings.
---@type table<string, PseudoStyleType>
PseudoStyles.Type = {
    Border = "border",
    Corner = "corner",
    Texture = "texture",
}

---Face names for "texture". Kept as a table so code can write
---PseudoStyles.Face.Top instead of retyping the string.
---@type table<string, PseudoFace>
PseudoStyles.Face = {
    All = "all",
    Front = "front",
    Back = "back",
    Top = "top",
    Bottom = "bottom",
    Right = "right",
    Left = "left",
}

-- Face name -> native slot. "all" is slot 0; the rest follow the cube's
-- own face order in draw_cube (1-based position in its face list). Meshes
-- have no named faces, so they use numeric slots directly (see Face
-- handling in the texture style below).
local FACE_SLOT = {
    all = 0,
    front = 1, back = 2, top = 3, bottom = 4, right = 5, left = 6,
}

local TARGETS_2D = { "RectObject", "CircleObject", "PolygonObject", "ImageObject", "TextObject", "LineObject" }
local TARGETS_RECT = { "RectObject" }
local TARGETS_3D = { "CubeObject", "MeshObject" }

---@param list string[]
---@param extra string[]
---@return string[]
local function concat(list, extra)
    local out = {}
    for _, v in ipairs(list) do out[#out + 1] = v end
    for _, v in ipairs(extra) do out[#out + 1] = v end
    return out
end

---@param v any
---@param name string
---@param fallback number?
---@return number
local function number(v, name, fallback)
    if v == nil and fallback ~= nil then return fallback end
    if type(v) ~= "number" then
        error(("PseudoStyles: %s must be a number, got %s"):format(name, type(v)), 3)
    end
    return v
end

---@param v number
---@return integer
local function channel(v)
    v = math.floor(v + 0.5)
    if v < 0 then return 0 end
    if v > 255 then return 255 end
    return v
end

-- =========================================================================
-- CONSTRUCTOR REGISTRY (adding a style kind = adding one entry here)
-- =========================================================================
---@type table<string, fun(...): PseudoStyle>
local constructors = {}

---@param r number
---@param g number?
---@param b number?
---@param width number?
---@return BorderStyle
constructors.border = function(r, g, b, width)
    local w = math.floor(number(width, "border width", 1))
    if w < 0 then error("PseudoStyles: border width cannot be negative", 3) end
    ---@type BorderStyle
    return {
        Kind = "border",
        Targets = { "RectObject", "CircleObject", "PolygonObject" },
        Color = { r = channel(number(r, "border r")), g = channel(g or 0), b = channel(b or 0) },
        Width = w,
    }
end

---@param radius number
---@return CornerStyle
constructors.corner = function(radius)
    local rad = math.floor(number(radius, "corner radius"))
    if rad < 0 then error("PseudoStyles: corner radius cannot be negative", 3) end
    ---@type CornerStyle
    return { Kind = "corner", Targets = TARGETS_RECT, Radius = rad }
end

---@param imageId integer
---@param face (PseudoFace|integer|(PseudoFace|integer)[])? default "all". A list binds the same image to several faces. A number is a raw slot (for meshes).
---@param options TextureOptions?
---@return TextureStyle
constructors.texture = function(imageId, face, options)
    if type(imageId) ~= "number" then
        error("PseudoStyles: texture needs an image id (from CreateImage)", 3)
    end
    local faces = {}
    local list = type(face) == "table" and face or { face == nil and "all" or face }
    for _, f in ipairs(list) do
        local key
        if type(f) == "number" then
            key = f
        elseif type(f) == "string" and FACE_SLOT[f:lower()] ~= nil then
            key = f:lower()
        else
            error(("PseudoStyles: unknown face %q"):format(tostring(f)), 3)
        end
        faces[key] = { imageId = imageId, options = options }
    end
    ---@type TextureStyle
    return {
        Kind = "texture",
        Targets = concat(TARGETS_2D, TARGETS_3D),
        Faces = faces,
        SetId = nil,
    }
end

---Creates a style element. `type` picks which kind; the remaining
---arguments depend on it:
---  "border"  (r, g, b, width)
---  "corner"  (radius)
---  "texture" (imageId, face?, options?)
---@overload fun(type: '"border"', r: number, g: number?, b: number?, width: number?): BorderStyle
---@overload fun(type: '"corner"', radius: number): CornerStyle
---@overload fun(type: '"texture"', imageId: integer, face: (PseudoFace|integer|(PseudoFace|integer)[])?, options: TextureOptions?): TextureStyle
---@param kind PseudoStyleType
---@return PseudoStyle
function PseudoStyles.new(kind, ...)
    local make = constructors[kind]
    if not make then
        local names = {}
        for k in pairs(constructors) do names[#names + 1] = k end
        table.sort(names)
        error(("PseudoStyles.new: unknown type %q (valid: %s)"):format(tostring(kind), table.concat(names, ", ")), 2)
    end
    return make(...)
end

---Registers a new style kind. `targets` limits which object types accept it.
---@param kind string
---@param constructor fun(...): PseudoStyle
function PseudoStyles.Register(kind, constructor)
    assert(type(kind) == "string" and type(constructor) == "function", "PseudoStyles.Register(kind, constructor)")
    constructors[kind] = constructor
end

---Maps a face name to its native slot (nil if it isn't a known name).
---@param face string|integer
---@return integer?
function PseudoStyles.SlotOf(face)
    if type(face) == "number" then return face end
    return FACE_SLOT[tostring(face):lower()]
end

---Checks whether a style may be applied to an object, by the object's type name.
---@param style PseudoStyle
---@param objectTypeName string
---@return boolean
function PseudoStyles.Accepts(style, objectTypeName)
    for _, t in ipairs(style.Targets) do
        if t == objectTypeName then return true end
    end
    return false
end

return PseudoStyles
