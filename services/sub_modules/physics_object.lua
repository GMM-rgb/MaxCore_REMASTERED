-- =========================================================================
-- services/sub_modules/physics_object.lua
-- =========================================================================
-- The physics sibling of sub_modules/game_object.lua. Wraps the native
-- physics_management module in a couple of clean OOP classes; window_service.lua
-- requires this the same way it requires game_object.lua, and wires
-- PhysicsBody into GameObjects via WindowObject:BindPhysics.
local InstanceTyping = require("instance_type")

---@alias PhysicsShapeType
---| '"sphere"'
---| '"box"'

---@alias PhysicsQualityLevel
---| 0 # Low -- fewest substeps/iterations, cheapest
---| 1 # Medium (default)
---| 2 # High
---| 3 # Ultra -- most substeps/iterations, most accurate

---@class PhysicsInterface
---@field create_world fun(gx: number?, gy: number?, gz: number?): integer
---@field destroy_world fun(worldId: integer): nil
---@field step_world fun(worldId: integer, dt: number): nil
---@field world_set_gravity fun(worldId: integer, x: number, y: number, z: number): nil
---@field world_get_gravity fun(worldId: integer): number, number, number
---@field world_set_quality fun(worldId: integer, level: PhysicsQualityLevel): integer effectiveLevel
---@field world_set_auto_quality fun(worldId: integer, enabled: boolean): nil
---@field world_get_quality fun(worldId: integer): PhysicsQualityLevel, boolean isManual
---@field world_set_quality_thresholds fun(worldId: integer, low: integer, medium: integer, high: integer): nil
---@field world_get_body_count fun(worldId: integer): integer
---@field create_body fun(worldId: integer, shapeType: PhysicsShapeType, shapeA: number, shapeB: number, shapeC: number, px: number, py: number, pz: number, mass: number, isStatic: boolean): integer?
---@field destroy_body fun(worldId: integer, bodyId: integer): nil
---@field body_set_position fun(worldId: integer, bodyId: integer, x: number, y: number, z: number): nil
---@field body_get_position fun(worldId: integer, bodyId: integer): number, number, number
---@field body_set_velocity fun(worldId: integer, bodyId: integer, x: number, y: number, z: number): nil
---@field body_get_velocity fun(worldId: integer, bodyId: integer): number, number, number
---@field body_apply_force fun(worldId: integer, bodyId: integer, x: number, y: number, z: number): nil
---@field body_apply_impulse fun(worldId: integer, bodyId: integer, x: number, y: number, z: number): nil
---@field body_set_rotation fun(worldId: integer, bodyId: integer, x: number, y: number, z: number): nil
---@field body_get_rotation fun(worldId: integer, bodyId: integer): number, number, number
---@field body_set_angular_velocity fun(worldId: integer, bodyId: integer, x: number, y: number, z: number): nil
---@field body_get_angular_velocity fun(worldId: integer, bodyId: integer): number, number, number
---@field body_apply_torque fun(worldId: integer, bodyId: integer, x: number, y: number, z: number): nil
---@field body_set_angular_damping fun(worldId: integer, bodyId: integer, value: number): nil
---@field body_set_mass fun(worldId: integer, bodyId: integer, mass: number): nil
---@field body_get_mass fun(worldId: integer, bodyId: integer): number
---@field body_set_density fun(worldId: integer, bodyId: integer, density: number): nil
---@field body_get_density fun(worldId: integer, bodyId: integer): number
---@field body_set_static fun(worldId: integer, bodyId: integer, isStatic: boolean): nil
---@field body_is_static fun(worldId: integer, bodyId: integer): boolean
---@field body_set_enabled fun(worldId: integer, bodyId: integer, enabled: boolean): nil
---@field body_is_enabled fun(worldId: integer, bodyId: integer): boolean
---@field body_set_restitution fun(worldId: integer, bodyId: integer, value: number): nil
---@field body_set_friction fun(worldId: integer, bodyId: integer, value: number): nil
---@field body_set_damping fun(worldId: integer, bodyId: integer, value: number): nil
---@field body_set_shape_sphere fun(worldId: integer, bodyId: integer, radius: number): nil
---@field body_set_shape_box fun(worldId: integer, bodyId: integer, hx: number, hy: number, hz: number): nil
---@field body_set_group fun(worldId: integer, bodyId: integer, group: integer): nil
---@field body_get_group fun(worldId: integer, bodyId: integer): integer
---@field body_set_collides_with fun(worldId: integer, bodyId: integer, mask: integer): nil
---@field body_get_collides_with fun(worldId: integer, bodyId: integer): integer
---@field body_predict_position fun(worldId: integer, bodyId: integer, dt: number): number, number, number
---@field body_predict_impact fun(worldId: integer, bodyId: integer, targetBodyId: integer, dt: number, samples: integer?): (willCollide: boolean, timeToImpact: number, predictedX: number, predictedY: number, predictedZ: number)

---@type boolean, PhysicsInterface|string
local physics_ok, physics_interface = pcall(require, "physics_management")

if not physics_ok then
    print("\27[33m[PhysicsService Warning]\27[0m Failed to load physics_interface: " .. tostring(physics_interface))
end

-- Common collision groups as plain bit flags -- combine with `+`/`-`
-- (Lua 5.1-safe, as long as you don't double-add the same flag) or the
-- bitwise `|` operator (Lua 5.3+) to build custom masks. Purely a
-- convenience starting point: SetGroup/SetCollidesWith take any integer.
---@class PhysicsGroups
local PhysicsGroups = {
    None    = 0,
    Default = 1,
    Static  = 2,
    Player  = 4,
    Enemy   = 8,
    Prop    = 16,
    Trigger = 32,
    All     = 0xFFFFFFFF,
}

-- =========================================================================
-- PHYSICS BODY
-- =========================================================================

---@class PhysicsBody
---@field private _worldId integer
---@field private _id integer
local PhysicsBody = {}
PhysicsBody.__index = PhysicsBody
InstanceTyping.SetType(PhysicsBody, "PhysicsBody")

---@param worldId integer
---@param bodyId integer
---@return PhysicsBody
function PhysicsBody._wrap(worldId, bodyId)
    local self = setmetatable({}, PhysicsBody)
    self._worldId = worldId
    self._id = bodyId
    return self
end

---@return integer
function PhysicsBody:GetId()
    return self._id
end

---Sets this body's world position directly (a teleport -- not physically simulated).
---@param x number
---@param y number
---@param z number
function PhysicsBody:SetPosition(x, y, z)
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.body_set_position(self._worldId, self._id, x, y, z)
    end
end

---@return number x
---@return number y
---@return number z
function PhysicsBody:GetPosition()
    if physics_ok and type(physics_interface) ~= "string" then
        return physics_interface.body_get_position(self._worldId, self._id)
    end
    return 0, 0, 0
end

---Sets this body's linear velocity directly.
---@param x number
---@param y number
---@param z number
function PhysicsBody:SetVelocity(x, y, z)
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.body_set_velocity(self._worldId, self._id, x, y, z)
    end
end

---@return number x
---@return number y
---@return number z
function PhysicsBody:GetVelocity()
    if physics_ok and type(physics_interface) ~= "string" then
        return physics_interface.body_get_velocity(self._worldId, self._id)
    end
    return 0, 0, 0
end

---Applies a continuous force -- accumulates until the world's next Step,
---then automatically resets to 0. Use for thrust, wind, custom gravity-like
---effects; reapply every frame if you want it to keep acting.
---@param x number
---@param y number
---@param z number
function PhysicsBody:ApplyForce(x, y, z)
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.body_apply_force(self._worldId, self._id, x, y, z)
    end
end

---Applies an instantaneous impulse (directly changes velocity by
---impulse/mass, once). Use for jumps, hits, explosions.
---@param x number
---@param y number
---@param z number
function PhysicsBody:ApplyImpulse(x, y, z)
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.body_apply_impulse(self._worldId, self._id, x, y, z)
    end
end

---Sets this body's rotation directly (Euler angles, radians -- same
---convention as GameObject:SetRotation). A teleport, not physically simulated.
---@param x number
---@param y number
---@param z number
function PhysicsBody:SetRotation(x, y, z)
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.body_set_rotation(self._worldId, self._id, x, y, z)
    end
end

---@return number x
---@return number y
---@return number z
function PhysicsBody:GetRotation()
    if physics_ok and type(physics_interface) ~= "string" then
        return physics_interface.body_get_rotation(self._worldId, self._id)
    end
    return 0, 0, 0
end

---Sets this body's angular velocity directly (radians/sec per axis).
---@param x number
---@param y number
---@param z number
function PhysicsBody:SetAngularVelocity(x, y, z)
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.body_set_angular_velocity(self._worldId, self._id, x, y, z)
    end
end

---@return number x
---@return number y
---@return number z
function PhysicsBody:GetAngularVelocity()
    if physics_ok and type(physics_interface) ~= "string" then
        return physics_interface.body_get_angular_velocity(self._worldId, self._id)
    end
    return 0, 0, 0
end

---Applies a continuous torque -- accumulates until the world's next Step,
---then automatically resets to 0. This is a direct spin-up (e.g. "make
---this spin like a top"). You do NOT need this for a pushed/dropped
---object to roll -- that rolling happens automatically from friction
---during collision resolution, driven by SetFriction.
---@param x number
---@param y number
---@param z number
function PhysicsBody:ApplyTorque(x, y, z)
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.body_apply_torque(self._worldId, self._id, x, y, z)
    end
end

---Sets per-substep angular velocity damping (rotational air-resistance-like decay), 0 (none) to 1 (stops spinning instantly).
---@param value number
function PhysicsBody:SetAngularDamping(value)
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.body_set_angular_damping(self._worldId, self._id, value)
    end
end

---Sets this body's mass. Ignored while static/anchored (mass is
---meaningless for an immovable body) -- call SetStatic(false) first if you
---need to change it.
---@param mass number
function PhysicsBody:SetMass(mass)
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.body_set_mass(self._worldId, self._id, mass)
    end
end

---@return number
function PhysicsBody:GetMass()
    if physics_ok and type(physics_interface) ~= "string" then
        return physics_interface.body_get_mass(self._worldId, self._id)
    end
    return 0
end

---Alternative to SetMass: derives mass from density * this body's current
---shape volume (mass = density * volume -- sphere: 4/3 * pi * r^3, box:
---full width * height * depth). Independent of SetMass otherwise --
---changing the shape afterward does NOT auto-recompute mass, call this
---again if you want that. Ignored while static/anchored.
---@param density number
function PhysicsBody:SetDensity(density)
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.body_set_density(self._worldId, self._id, density)
    end
end

---@return number
function PhysicsBody:GetDensity()
    if physics_ok and type(physics_interface) ~= "string" then
        return physics_interface.body_get_density(self._worldId, self._id)
    end
    return 0
end

---Anchors (true) or un-anchors (false) this body. An anchored body has
---infinite mass -- forces, impulses, and collisions never move it -- but
---other bodies still collide against it normally. Use this for ground,
---walls, and any fixed geometry.
---@param isStatic boolean
function PhysicsBody:SetStatic(isStatic)
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.body_set_static(self._worldId, self._id, isStatic)
    end
end

---@return boolean
function PhysicsBody:IsStatic()
    if physics_ok and type(physics_interface) ~= "string" then
        return physics_interface.body_is_static(self._worldId, self._id)
    end
    return false
end

---Enables or disables simulation for this body entirely -- disabled means
---frozen in place and skipped by both integration and collision (cheaper
---than destroying/recreating it if you'll want it back).
---@param enabled boolean
function PhysicsBody:SetEnabled(enabled)
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.body_set_enabled(self._worldId, self._id, enabled)
    end
end

---@return boolean
function PhysicsBody:IsEnabled()
    if physics_ok and type(physics_interface) ~= "string" then
        return physics_interface.body_is_enabled(self._worldId, self._id)
    end
    return true
end

---Sets bounciness, 0 (no bounce) to 1 (perfectly elastic). A collision uses
---the smaller of the two bodies' restitution values.
---@param value number
function PhysicsBody:SetRestitution(value)
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.body_set_restitution(self._worldId, self._id, value)
    end
end

---Sets how much a collision damps tangential (sliding) velocity, 0 (ice)
---to 1 (sticky). A collision uses the smaller of the two bodies' friction values.
---@param value number
function PhysicsBody:SetFriction(value)
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.body_set_friction(self._worldId, self._id, value)
    end
end

---Sets per-substep linear velocity damping (air-resistance-like decay), 0
---(none) to 1 (stops instantly).
---@param value number
function PhysicsBody:SetDamping(value)
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.body_set_damping(self._worldId, self._id, value)
    end
end

---Switches this body's collision shape to a sphere.
---@param radius number
function PhysicsBody:SetShapeSphere(radius)
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.body_set_shape_sphere(self._worldId, self._id, radius)
    end
end

---Switches this body's collision shape to an axis-aligned box, given as
---half-extents (half the box's full width/height/depth on each axis).
---@param hx number
---@param hy number
---@param hz number
function PhysicsBody:SetShapeBox(hx, hy, hz)
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.body_set_shape_box(self._worldId, self._id, hx, hy, hz)
    end
end

---Sets which physics group(s) this body belongs to (bitmask -- see
---PhysicsGroups for common flags, or use your own integers).
---@param group integer
function PhysicsBody:SetGroup(group)
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.body_set_group(self._worldId, self._id, group)
    end
end

---@return integer
function PhysicsBody:GetGroup()
    if physics_ok and type(physics_interface) ~= "string" then
        return physics_interface.body_get_group(self._worldId, self._id)
    end
    return PhysicsGroups.Default
end

---Sets which group(s) this body will physically react to (bitmask). Two
---bodies only actually collide if EACH has the other's group bit present
---in its own CollidesWith mask -- set this to PhysicsGroups.None (or any
---mask that excludes everything relevant) and this body still gets
---simulated and DRAWN normally, it just won't physically interact with
---anything. That's the knob that turns a bound-but-inert body into a real
---participant in the simulation.
---@param mask integer
function PhysicsBody:SetCollidesWith(mask)
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.body_set_collides_with(self._worldId, self._id, mask)
    end
end

---@return integer
function PhysicsBody:GetCollidesWith()
    if physics_ok and type(physics_interface) ~= "string" then
        return physics_interface.body_get_collides_with(self._worldId, self._id)
    end
    return PhysicsGroups.All
end

---Predicts where this body would be after `dt` seconds under its current
---velocity/gravity/forces, WITHOUT actually stepping the simulation or
---resolving any collisions along the way. Handy for trajectory previews or
---simple AI lookahead.
---@param dt number
---@return number x
---@return number y
---@return number z
function PhysicsBody:PredictPosition(dt)
    if physics_ok and type(physics_interface) ~= "string" then
        return physics_interface.body_predict_position(self._worldId, self._id, dt)
    end
    return self:GetPosition()
end

---Samples this body's predicted trajectory over the next `dt` seconds and
---reports whether/when it would first touch `target`'s CURRENT shape -- a
---cheap sampled approximation (not true continuous collision detection),
---good enough for gameplay warnings or AI decisions.
---@param target PhysicsBody
---@param dt number
---@param samples integer? Number of trajectory samples to check (default 8; more = finer but pricier)
---@return boolean willCollide
---@return number timeToImpact
---@return number predictedX
---@return number predictedY
---@return number predictedZ
function PhysicsBody:PredictImpact(target, dt, samples)
    if physics_ok and type(physics_interface) ~= "string" and target then
        return physics_interface.body_predict_impact(self._worldId, self._id, target:GetId(), dt, samples or 8)
    end
    return false, 0, 0, 0, 0
end

---Removes this body from its world -- it stops being simulated entirely.
---If you bound it to a GameObject via WindowObject:BindPhysics, call
---WindowObject:UnbindPhysics too so the link gets cleaned up and the
---object goes back to being purely visual.
function PhysicsBody:Destroy()
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.destroy_body(self._worldId, self._id)
    end
end

-- =========================================================================
-- PHYSICS WORLD
-- =========================================================================

---@class PhysicsWorld
---@field private _id integer
local PhysicsWorld = {}
PhysicsWorld.__index = PhysicsWorld
InstanceTyping.SetType(PhysicsWorld, "PhysicsWorld")

---Creates a new physics simulation world.
---@param gx number? Gravity X (default 0)
---@param gy number? Gravity Y (default -9.81, i.e. standard downward gravity)
---@param gz number? Gravity Z (default 0)
---@return PhysicsWorld?
function PhysicsWorld.new(gx, gy, gz)
    if not physics_ok or type(physics_interface) == "string" then return nil end

    local worldId = physics_interface.create_world(gx or 0.0, gy or -9.81, gz or 0.0)
    if not worldId then return nil end

    ---@type PhysicsWorld
    local self = setmetatable({}, PhysicsWorld)
    self._id = worldId
    return self
end

---@return integer
function PhysicsWorld:GetId()
    return self._id
end

---Advances the simulation by `dt` seconds -- integrates all enabled,
---non-static bodies and resolves collisions between bodies whose groups/
---masks match, using however many substeps/iterations the current quality
---level calls for (see SetQuality and dynamic auto-adjustment).
---@param dt number
function PhysicsWorld:Step(dt)
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.step_world(self._id, dt)
    end
end

---Sets world gravity.
---@param x number
---@param y number
---@param z number
function PhysicsWorld:SetGravity(x, y, z)
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.world_set_gravity(self._id, x, y, z)
    end
end

---@return number x
---@return number y
---@return number z
function PhysicsWorld:GetGravity()
    if physics_ok and type(physics_interface) ~= "string" then
        return physics_interface.world_get_gravity(self._id)
    end
    return 0, -9.81, 0
end

---Manually pins the simulation quality level (0=Low .. 3=Ultra -- more
---substeps/collision iterations means more accurate but pricier) and
---disables automatic dynamic level shifting until SetAutoQuality(true) is
---called again. This is the manual override; leave it alone if you're
---happy letting the world adjust itself as the scene gets busier.
---@param level PhysicsQualityLevel
---@return integer effectiveLevel The level actually applied, after clamping to 0..3
function PhysicsWorld:SetQuality(level)
    if physics_ok and type(physics_interface) ~= "string" then
        return physics_interface.world_set_quality(self._id, level)
    end
    return level
end

---Re-enables (true) or disables (false) automatic dynamic quality
---shifting. When enabled (the default), the world nudges its own quality
---level up or down once per Step based on how many active bodies exist
---(see SetQualityThresholds) -- trading accuracy for speed as the scene
---gets busier, and vice versa when it's quiet.
---@param enabled boolean
function PhysicsWorld:SetAutoQuality(enabled)
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.world_set_auto_quality(self._id, enabled)
    end
end

---@return PhysicsQualityLevel level
---@return boolean isManual True if SetQuality pinned this and auto-shifting is currently off
function PhysicsWorld:GetQuality()
    if physics_ok and type(physics_interface) ~= "string" then
        return physics_interface.world_get_quality(self._id)
    end
    return 1, false
end

---Tunes the active-body-count thresholds automatic quality shifting uses:
---at/above `low` active bodies the world drops to Low quality, at/above
---`medium` it's Medium, at/above `high` it's High, and below all three it's
---Ultra. Has no effect while manually pinned (see SetQuality).
---@param low integer? Default 40
---@param medium integer? Default 20
---@param high integer? Default 8
function PhysicsWorld:SetQualityThresholds(low, medium, high)
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.world_set_quality_thresholds(self._id, low or 40, medium or 20, high or 8)
    end
end

---@return integer
function PhysicsWorld:GetBodyCount()
    if physics_ok and type(physics_interface) ~= "string" then
        return physics_interface.world_get_body_count(self._id)
    end
    return 0
end

---Creates a new physics body in this world. This is a bare body with no
---connection to anything visual -- pair it with a GameObject yourself (or
---use WindowObject:BindPhysics for the one-call path) if you want it to
---move something on screen.
---@param shapeType PhysicsShapeType "sphere" or "box"
---@param shapeParams number[] Sphere: {radius}. Box: {halfExtentX, halfExtentY, halfExtentZ}.
---@param px number? Position X
---@param py number? Position Y
---@param pz number? Position Z
---@param mass number? Default 1.0
---@param isStatic boolean? Anchored/immovable (default false)
---@return PhysicsBody?
function PhysicsWorld:CreateBody(shapeType, shapeParams, px, py, pz, mass, isStatic)
    if not physics_ok or type(physics_interface) == "string" then return nil end

    shapeParams = shapeParams or {}
    local a = shapeParams[1] or 0.5
    local b = shapeParams[2] or a
    local c = shapeParams[3] or a

    local bodyId = physics_interface.create_body(
        self._id, shapeType or "sphere", a, b, c,
        px or 0.0, py or 0.0, pz or 0.0,
        mass or 1.0, isStatic or false
    )
    if not bodyId then return nil end

    return PhysicsBody._wrap(self._id, bodyId)
end

---Destroys the world and every body in it.
function PhysicsWorld:Destroy()
    if physics_ok and type(physics_interface) ~= "string" then
        physics_interface.destroy_world(self._id)
    end
end

return {
    PhysicsWorld = PhysicsWorld,
    PhysicsBody = PhysicsBody,
    PhysicsGroups = PhysicsGroups,
}
