local InstanceTyping = require("instance_type")

-- =========================================================================
-- RANDOM CLASS & OBJECT
-- =========================================================================

---@class RandomObject
---@field Seed number
---@field SetSeed fun(self: RandomObject, seed: number)
---@field NextInteger fun(self: RandomObject, min?: integer, max?: integer): integer
---@field NextNumber fun(self: RandomObject, min?: number, max?: number): number
---@field NextItem fun(self: RandomObject, tbl: table): any
---@field NextColor fun(self: RandomObject): integer, integer, integer
local RandomObject = {}
RandomObject.__index = RandomObject
InstanceTyping.SetType(RandomObject, "RandomObject")

---@class RandomClass
---@field new fun(seed?: number): RandomObject
local RandomClass = {}

--- Creates a new Random generator instance using Lua's standard math library.
---@param seed number? Optional custom seed value (defaults to os.time()).
---@return RandomObject
function RandomClass.new(seed)
    local self = setmetatable({
        Seed = seed or os.time()
    }, RandomObject)
    self:SetSeed(self.Seed)
    return self
end

--- Sets a new seed for this random generator instance.
---@param seed number The seed value to apply.
function RandomObject:SetSeed(seed)
    self.Seed = seed
    math.randomseed(seed)
end

--- Returns a random integer between min and max (inclusive).
---@param min integer? Minimum bound (defaults to 0).
---@param max integer? Maximum bound (defaults to 1).
---@return integer
function RandomObject:NextInteger(min, max)
    min = min or 0
    max = max or 1
    return math.random(min, max)
end

--- Returns a random floating-point number between min and max.
---@param min number? Minimum bound (defaults to 0.0).
---@param max number? Maximum bound (defaults to 1.0).
---@return number
function RandomObject:NextNumber(min, max)
    min = min or 0.0
    max = max or 1.0
    return min + math.random() * (min - max ~= 0 and math.random() or 1)
end

--- Picks a random element from a given array table.
---@param tbl table The array table to pick from.
---@return any
function RandomObject:NextItem(tbl)
    if #tbl == 0 then return nil end
    return tbl[math.random(1, #tbl)]
end

--- Generates a random RGB color set of values from 0 to 255.
---@return integer red Red value component (0-255).
---@return integer green Green value component (0-255).
---@return integer blue Blue value component (0-255).
function RandomObject:NextColor()
    return math.random(0, 255), math.random(0, 255), math.random(0, 255)
end

-- =========================================================================
-- PSEUDO CLASS & OBJECT (LCG-based generator)
-- =========================================================================

---@class PseudoObject
---@field Seed number
---@field State number
---@field Next fun(self: PseudoObject): number
---@field NextInteger fun(self: PseudoObject, min: integer, max: integer): integer
local PseudoObject = {}
PseudoObject.__index = PseudoObject
InstanceTyping.SetType(PseudoObject, "PseudoObject")

---@class PseudoClass
---@field new fun(seed?: number): PseudoObject
local PseudoClass = {}

--- Creates a new custom Linear Congruential Generator (LCG) pseudo-random instance.
---@param seed number? Optional starting seed (defaults to os.time()).
---@return PseudoObject
function PseudoClass.new(seed)
    local self = setmetatable({
        Seed = seed or os.time(),
        State = seed or os.time()
    }, PseudoObject)
    return self
end

--- Advances the internal LCG state and returns a pseudo-random float between 0 and 1.
---@return number
function PseudoObject:Next()
    self.State = (self.State * 1103515245 + 12345) % 2147483648
    return self.State / 2147483648
end

--- Returns a pseudo-random integer between min and max using the LCG state.
---@param min integer Minimum bound.
---@param max integer Maximum bound.
---@return integer
function PseudoObject:NextInteger(min, max)
    local val = self:Next()
    return math.floor(min + val * (max - min + 1))
end

-- =========================================================================
-- NOISE CLASS & OBJECT (Smooth 2D Perlin-style noise)
-- =========================================================================

---@class NoiseObject
---@field Seed number
---@field OffsetX number
---@field OffsetY number
---@field Sample2D fun(self: NoiseObject, x: number, y: number): number
local NoiseObject = {}
NoiseObject.__index = NoiseObject
InstanceTyping.SetType(NoiseObject, "NoiseObject")

---@class NoiseClass
---@field new fun(seed?: number): NoiseObject
local NoiseClass = {}

--- Creates a new Smooth 2D Noise generator instance with randomized grid offsets.
---@param seed number? Optional seed to determine noise layout patterns (defaults to os.time()).
---@return NoiseObject
function NoiseClass.new(seed)
    local seedVal = seed or os.time()
    math.randomseed(seedVal)
    local self = setmetatable({
        Seed = seedVal,
        OffsetX = math.random() * 10000,
        OffsetY = math.random() * 10000
    }, NoiseObject)
    return self
end

--- Smoothstep fade function for noise curve interpolation.
---@param t number Input fractional value between 0 and 1.
---@return number Smoothed output value.
local function fade(t)
    return t * t * t * (t * (t * 6 - 15) + 10)
end

--- Linear interpolation between two scalar values.
---@param t number Interpolation weighting factor (0 to 1).
---@param a number Start value.
---@param b number End value.
---@return number Interpolated result.
local function lerp(t, a, b)
    return a + t * (b - a)
end

--- Computes a pseudo-random gradient value based on hash lookup and coordinates.
---@param hash integer Integer hash code from permutation logic.
---@param x number Local X coordinate.
---@param y number Local Y coordinate.
---@return number Gradient contribution value.
local function grad(hash, x, y)
    local h = hash % 4
    local u = h < 2 and x or y
    local v = h < 2 and y or x
    return ((h % 2 == 0) and u or -u) + (((math.floor(h / 2) % 2 == 0) and v or -v))
end

--- Samples smooth 2D gradient noise at given coordinates, returning a continuous value roughly between -1 and 1.
---@param x number X coordinate to sample.
---@param y number Y coordinate to sample.
---@return number Smooth noise value.
function NoiseObject:Sample2D(x, y)
    x = x + self.OffsetX
    y = y + self.OffsetY

    local X = math.floor(x) % 256
    local Y = math.floor(y) % 256

    x = x - math.floor(x)
    y = y - math.floor(y)

    local u = fade(x)
    local v = fade(y)

    local A = (X + Y * 57) % 256
    local B = (X + 1 + Y * 57) % 256
    local AA = (A + Y + 1) % 256
    local AB = (A + Y) % 256
    local BA = (B + Y + 1) % 256
    local BB = (B + Y) % 256

    local res = lerp(v, lerp(u, grad(AA, x, y), grad(BA, x - 1, y)),
    lerp(u, grad(AB, x, y - 1), grad(BB, x - 1, y - 1)))
    return res * 1.414
end

return {
    Random = RandomClass,
    Pseudo = PseudoClass,
    Noise = NoiseClass,
};
