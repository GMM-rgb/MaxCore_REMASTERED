// =========================================================================
// physics_management.cpp
// =========================================================================
// Native Lua binding layer over inline_headers/physics/physics.hpp -- the
// physics sibling of window_management.cpp. Deliberately its own compiled
// module (physics_management.{so,dylib,dll}, opened via
// require("physics_management")) rather than folded into window_management,
// mirroring how physics.hpp sits next to renderer.hpp: same pairing
// pattern, one step over.
#include <lua.hpp>
#include <unordered_map>
#include <cctype>

#include "inline_headers/physics/physics.hpp"

#if defined(_WIN32) || defined(_WIN64)
    #define EXPORT_FN __declspec(dllexport)
#else
    #define EXPORT_FN
#endif

static std::unordered_map<int, Physics::World> g_worlds;
static int g_next_world_id = 1;

static Physics::World* GetWorld(int worldId) {
    auto it = g_worlds.find(worldId);
    return it != g_worlds.end() ? &it->second : nullptr;
}

static bool EqualsIgnoreCase(const char* a, const char* b) {
    while (*a && *b) {
        if (std::tolower((unsigned char)*a) != std::tolower((unsigned char)*b)) return false;
        ++a; ++b;
    }
    return *a == *b;
}

// =========================================================================
// WORLD
// =========================================================================
static int world_create(lua_State* L) {
    float gx = static_cast<float>(luaL_optnumber(L, 1, 0.0));
    float gy = static_cast<float>(luaL_optnumber(L, 2, -9.81));
    float gz = static_cast<float>(luaL_optnumber(L, 3, 0.0));

    Physics::World world;
    world.gravity = {gx, gy, gz};

    int worldId = g_next_world_id++;
    g_worlds[worldId] = world;
    lua_pushinteger(L, worldId);
    return 1;
}

static int world_destroy(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    g_worlds.erase(worldId);
    return 0;
}

static int world_step(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    float dt = static_cast<float>(luaL_checknumber(L, 2));
    Physics::World* world = GetWorld(worldId);
    if (world) world->Step(dt);
    return 0;
}

static int world_set_gravity(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    Physics::World* world = GetWorld(worldId);
    if (world) world->gravity = {x, y, z};
    return 0;
}

static int world_get_gravity(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    Physics::World* world = GetWorld(worldId);
    if (world) {
        lua_pushnumber(L, world->gravity.x);
        lua_pushnumber(L, world->gravity.y);
        lua_pushnumber(L, world->gravity.z);
        return 3;
    }
    lua_pushnil(L); lua_pushnil(L); lua_pushnil(L);
    return 3;
}

// Quality levels are plain small integers (0=Low, 1=Medium, 2=High,
// 3=Ultra) -- same "just a number" convention as text/alias quality, no
// separate enum type exposed to Lua. Setting one manually disables
// automatic dynamic level shifting until world_set_auto_quality(true).
static int world_set_quality(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int level = static_cast<int>(luaL_checkinteger(L, 2));
    if (level < 0) level = 0;
    if (level > 3) level = 3;

    Physics::World* world = GetWorld(worldId);
    if (world) {
        world->quality = static_cast<Physics::QualityLevel>(level);
        world->manualQuality = true;
    }
    lua_pushinteger(L, level);
    return 1;
}

static int world_set_auto_quality(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    bool enabled = lua_toboolean(L, 2);
    Physics::World* world = GetWorld(worldId);
    if (world) world->manualQuality = !enabled;
    return 0;
}

static int world_get_quality(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    Physics::World* world = GetWorld(worldId);
    if (world) {
        lua_pushinteger(L, static_cast<int>(world->quality));
        lua_pushboolean(L, world->manualQuality ? 1 : 0);
        return 2;
    }
    lua_pushinteger(L, 1);
    lua_pushboolean(L, 0);
    return 2;
}

static int world_set_quality_thresholds(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int low = static_cast<int>(luaL_optinteger(L, 2, 40));
    int medium = static_cast<int>(luaL_optinteger(L, 3, 20));
    int high = static_cast<int>(luaL_optinteger(L, 4, 8));

    Physics::World* world = GetWorld(worldId);
    if (world) {
        world->lowThreshold = low;
        world->mediumThreshold = medium;
        world->highThreshold = high;
    }
    return 0;
}

static int world_get_body_count(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    Physics::World* world = GetWorld(worldId);
    lua_pushinteger(L, world ? (lua_Integer)world->bodies.size() : 0);
    return 1;
}

// =========================================================================
// BODY
// =========================================================================
// shapeType: "sphere" | "box" (case-insensitive, defaults to "sphere").
// Sphere uses shapeA as its radius (shapeB/shapeC ignored). Box uses
// shapeA/B/C as half-extents on X/Y/Z.
static int body_create(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    const char* shapeType = luaL_optstring(L, 2, "sphere");
    float shapeA = static_cast<float>(luaL_optnumber(L, 3, 0.5));
    float shapeB = static_cast<float>(luaL_optnumber(L, 4, 0.5));
    float shapeC = static_cast<float>(luaL_optnumber(L, 5, 0.5));
    float px = static_cast<float>(luaL_optnumber(L, 6, 0.0));
    float py = static_cast<float>(luaL_optnumber(L, 7, 0.0));
    float pz = static_cast<float>(luaL_optnumber(L, 8, 0.0));
    float mass = static_cast<float>(luaL_optnumber(L, 9, 1.0));
    bool isStatic = lua_toboolean(L, 10);

    Physics::World* world = GetWorld(worldId);
    if (!world) { lua_pushnil(L); return 1; }

    Physics::Body body;
    body.position = {px, py, pz};

    if (EqualsIgnoreCase(shapeType, "box")) {
        body.shape.type = Physics::ShapeType::Box;
        body.shape.halfExtents = {shapeA, shapeB, shapeC};
    } else {
        body.shape.type = Physics::ShapeType::Sphere;
        body.shape.radius = shapeA;
    }

    body.SetMass(mass);
    body.SetStatic(isStatic);

    int bodyId = world->AddBody(body);
    lua_pushinteger(L, bodyId);
    return 1;
}

static int body_destroy(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));
    Physics::World* world = GetWorld(worldId);
    if (world) world->RemoveBody(bodyId);
    return 0;
}

static int body_set_position(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));
    float x = static_cast<float>(luaL_checknumber(L, 3));
    float y = static_cast<float>(luaL_checknumber(L, 4));
    float z = static_cast<float>(luaL_checknumber(L, 5));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    if (body) body->position = {x, y, z};
    return 0;
}

static int body_get_position(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    if (body) {
        lua_pushnumber(L, body->position.x);
        lua_pushnumber(L, body->position.y);
        lua_pushnumber(L, body->position.z);
        return 3;
    }
    lua_pushnil(L); lua_pushnil(L); lua_pushnil(L);
    return 3;
}

static int body_set_velocity(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));
    float x = static_cast<float>(luaL_checknumber(L, 3));
    float y = static_cast<float>(luaL_checknumber(L, 4));
    float z = static_cast<float>(luaL_checknumber(L, 5));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    if (body) body->velocity = {x, y, z};
    return 0;
}

static int body_get_velocity(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    if (body) {
        lua_pushnumber(L, body->velocity.x);
        lua_pushnumber(L, body->velocity.y);
        lua_pushnumber(L, body->velocity.z);
        return 3;
    }
    lua_pushnil(L); lua_pushnil(L); lua_pushnil(L);
    return 3;
}

// Continuous force -- accumulates into the body's force buffer, which is
// cleared automatically at the end of every World::Step. Good for
// thrust/wind/gravity-like effects you want to reapply every frame.
static int body_apply_force(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));
    float x = static_cast<float>(luaL_checknumber(L, 3));
    float y = static_cast<float>(luaL_checknumber(L, 4));
    float z = static_cast<float>(luaL_checknumber(L, 5));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    if (body) {
        body->force.x += x;
        body->force.y += y;
        body->force.z += z;
    }
    return 0;
}

// Instantaneous impulse -- directly changes velocity by impulse * invMass,
// once, right now. Good for jumps, hits, explosions.
static int body_apply_impulse(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));
    float x = static_cast<float>(luaL_checknumber(L, 3));
    float y = static_cast<float>(luaL_checknumber(L, 4));
    float z = static_cast<float>(luaL_checknumber(L, 5));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    if (body) {
        body->velocity.x += x * body->invMass;
        body->velocity.y += y * body->invMass;
        body->velocity.z += z * body->invMass;
    }
    return 0;
}

// Rotation is Euler angles in radians -- same convention as
// GameObject.Rotation, so a bound body's rotation drops straight onto its
// visual object with no conversion (see WindowObject:BindPhysics/StepPhysics).
static int body_set_rotation(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));
    float x = static_cast<float>(luaL_checknumber(L, 3));
    float y = static_cast<float>(luaL_checknumber(L, 4));
    float z = static_cast<float>(luaL_checknumber(L, 5));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    if (body) body->rotation = {x, y, z};
    return 0;
}

static int body_get_rotation(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    if (body) {
        lua_pushnumber(L, body->rotation.x);
        lua_pushnumber(L, body->rotation.y);
        lua_pushnumber(L, body->rotation.z);
        return 3;
    }
    lua_pushnil(L); lua_pushnil(L); lua_pushnil(L);
    return 3;
}

// Angular velocity, radians/sec per axis.
static int body_set_angular_velocity(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));
    float x = static_cast<float>(luaL_checknumber(L, 3));
    float y = static_cast<float>(luaL_checknumber(L, 4));
    float z = static_cast<float>(luaL_checknumber(L, 5));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    if (body) body->angularVelocity = {x, y, z};
    return 0;
}

static int body_get_angular_velocity(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    if (body) {
        lua_pushnumber(L, body->angularVelocity.x);
        lua_pushnumber(L, body->angularVelocity.y);
        lua_pushnumber(L, body->angularVelocity.z);
        return 3;
    }
    lua_pushnil(L); lua_pushnil(L); lua_pushnil(L);
    return 3;
}

// Continuous torque -- accumulates into the body's torque buffer, cleared
// automatically at the end of every World::Step, same lifecycle as
// body_apply_force. This is a direct spin-up (e.g. "spin this like a top");
// the ROLLING that naturally happens when a body is pushed/dropped comes
// from friction torque computed automatically inside collision resolution,
// not from this.
static int body_apply_torque(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));
    float x = static_cast<float>(luaL_checknumber(L, 3));
    float y = static_cast<float>(luaL_checknumber(L, 4));
    float z = static_cast<float>(luaL_checknumber(L, 5));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    if (body) {
        body->torque.x += x;
        body->torque.y += y;
        body->torque.z += z;
    }
    return 0;
}

static int body_set_angular_damping(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));
    float value = static_cast<float>(luaL_checknumber(L, 3));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    if (body) body->angularDamping = value;
    return 0;
}

static int body_set_mass(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));
    float mass = static_cast<float>(luaL_checknumber(L, 3));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    if (body && !body->isStatic) body->SetMass(mass);
    return 0;
}

static int body_get_mass(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    lua_pushnumber(L, body ? body->mass : 0.0);
    return 1;
}

// Alternative to body_set_mass: derives mass from density * this body's
// current shape volume (mass = density * volume). Independent otherwise --
// changing the shape afterward does not auto-recompute mass; call this
// again if you want that.
static int body_set_density(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));
    float density = static_cast<float>(luaL_checknumber(L, 3));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    if (body && !body->isStatic) body->SetDensity(density);
    return 0;
}

static int body_get_density(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    lua_pushnumber(L, body ? body->density : 0.0);
    return 1;
}

// "Anchoring" -- true pins the body in place (infinite mass; forces,
// impulses, and collisions never move it, but other bodies still collide
// against it normally). false lets it move again, restoring mass-derived invMass.
static int body_set_static(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));
    bool isStatic = lua_toboolean(L, 3);

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    if (body) body->SetStatic(isStatic);
    return 0;
}

static int body_is_static(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    lua_pushboolean(L, (body && body->isStatic) ? 1 : 0);
    return 1;
}

// Enabled/disabled is separate from static/anchored: disabling freezes the
// body in place AND skips it from collision checks entirely (cheaper than
// destroying/recreating it if you'll want it back later).
static int body_set_enabled(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));
    bool enabled = lua_toboolean(L, 3);

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    if (body) body->isEnabled = enabled;
    return 0;
}

static int body_is_enabled(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    lua_pushboolean(L, (body && body->isEnabled) ? 1 : 0);
    return 1;
}

static int body_set_restitution(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));
    float value = static_cast<float>(luaL_checknumber(L, 3));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    if (body) body->restitution = value;
    return 0;
}

static int body_set_friction(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));
    float value = static_cast<float>(luaL_checknumber(L, 3));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    if (body) body->friction = value;
    return 0;
}

static int body_set_damping(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));
    float value = static_cast<float>(luaL_checknumber(L, 3));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    if (body) body->linearDamping = value;
    return 0;
}

static int body_set_shape_sphere(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));
    float radius = static_cast<float>(luaL_checknumber(L, 3));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    if (body) {
        body->shape.type = Physics::ShapeType::Sphere;
        body->shape.radius = radius;
    }
    return 0;
}

static int body_set_shape_box(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));
    float hx = static_cast<float>(luaL_checknumber(L, 3));
    float hy = static_cast<float>(luaL_checknumber(L, 4));
    float hz = static_cast<float>(luaL_checknumber(L, 5));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    if (body) {
        body->shape.type = Physics::ShapeType::Box;
        body->shape.halfExtents = {hx, hy, hz};
    }
    return 0;
}

// group/collidesWith are bitmasks -- plain integers (1, 2, 4, 8, ... or any
// combination). Two bodies only physically react to each other if EACH
// considers the other's group bit present in its own collidesWith mask.
static int body_set_group(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));
    lua_Integer group = luaL_checkinteger(L, 3);

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    if (body) body->group = static_cast<uint32_t>(group);
    return 0;
}

static int body_get_group(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    lua_pushinteger(L, body ? (lua_Integer)body->group : 0);
    return 1;
}

static int body_set_collides_with(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));
    lua_Integer mask = luaL_checkinteger(L, 3);

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    if (body) body->collidesWith = static_cast<uint32_t>(mask);
    return 0;
}

static int body_get_collides_with(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    lua_pushinteger(L, body ? (lua_Integer)body->collidesWith : (lua_Integer)0xFFFFFFFFu);
    return 1;
}

static int body_predict_position(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));
    float dt = static_cast<float>(luaL_checknumber(L, 3));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    if (body) {
        Physics::Vec3 predicted = Physics::PredictPosition(*body, world->gravity, dt);
        lua_pushnumber(L, predicted.x);
        lua_pushnumber(L, predicted.y);
        lua_pushnumber(L, predicted.z);
        return 3;
    }
    lua_pushnil(L); lua_pushnil(L); lua_pushnil(L);
    return 3;
}

static int body_predict_impact(lua_State* L) {
    int worldId = static_cast<int>(luaL_checkinteger(L, 1));
    int bodyId = static_cast<int>(luaL_checkinteger(L, 2));
    int targetBodyId = static_cast<int>(luaL_checkinteger(L, 3));
    float dt = static_cast<float>(luaL_checknumber(L, 4));
    int samples = static_cast<int>(luaL_optinteger(L, 5, 8));

    Physics::World* world = GetWorld(worldId);
    Physics::Body* body = world ? world->GetBody(bodyId) : nullptr;
    Physics::Body* target = world ? world->GetBody(targetBodyId) : nullptr;
    if (!body || !target) {
        lua_pushboolean(L, 0);
        return 1;
    }

    Physics::PredictedImpact impact = Physics::PredictImpact(*body, *target, world->gravity, dt, samples);
    lua_pushboolean(L, impact.willCollide ? 1 : 0);
    lua_pushnumber(L, impact.timeToImpact);
    lua_pushnumber(L, impact.position.x);
    lua_pushnumber(L, impact.position.y);
    lua_pushnumber(L, impact.position.z);
    return 5;
}

extern "C" EXPORT_FN int luaopen_physics_management(lua_State* L) {
    static const luaL_Reg PhysicsManagement[] = {
        {"create_world", world_create},
        {"destroy_world", world_destroy},
        {"step_world", world_step},
        {"world_set_gravity", world_set_gravity},
        {"world_get_gravity", world_get_gravity},
        {"world_set_quality", world_set_quality},
        {"world_set_auto_quality", world_set_auto_quality},
        {"world_get_quality", world_get_quality},
        {"world_set_quality_thresholds", world_set_quality_thresholds},
        {"world_get_body_count", world_get_body_count},

        {"create_body", body_create},
        {"destroy_body", body_destroy},
        {"body_set_position", body_set_position},
        {"body_get_position", body_get_position},
        {"body_set_velocity", body_set_velocity},
        {"body_get_velocity", body_get_velocity},
        {"body_apply_force", body_apply_force},
        {"body_apply_impulse", body_apply_impulse},
        {"body_set_rotation", body_set_rotation},
        {"body_get_rotation", body_get_rotation},
        {"body_set_angular_velocity", body_set_angular_velocity},
        {"body_get_angular_velocity", body_get_angular_velocity},
        {"body_apply_torque", body_apply_torque},
        {"body_set_angular_damping", body_set_angular_damping},
        {"body_set_mass", body_set_mass},
        {"body_get_mass", body_get_mass},
        {"body_set_density", body_set_density},
        {"body_get_density", body_get_density},
        {"body_set_static", body_set_static},
        {"body_is_static", body_is_static},
        {"body_set_enabled", body_set_enabled},
        {"body_is_enabled", body_is_enabled},
        {"body_set_restitution", body_set_restitution},
        {"body_set_friction", body_set_friction},
        {"body_set_damping", body_set_damping},
        {"body_set_shape_sphere", body_set_shape_sphere},
        {"body_set_shape_box", body_set_shape_box},
        {"body_set_group", body_set_group},
        {"body_get_group", body_get_group},
        {"body_set_collides_with", body_set_collides_with},
        {"body_get_collides_with", body_get_collides_with},
        {"body_predict_position", body_predict_position},
        {"body_predict_impact", body_predict_impact},

        {nullptr, nullptr}
    };

    luaL_newlib(L, PhysicsManagement);
    return 1;
}
