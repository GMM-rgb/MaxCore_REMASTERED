// =========================================================================
// inline_headers/physics/physics.hpp
// =========================================================================
// A small, self-contained rigid-body physics core -- the physics sibling
// of renderer.hpp. Reuses Graphics::Vec3 and its math helpers so a body's
// position is the exact same type the renderer projects, no conversion
// needed at the boundary.
//
// Scope, deliberately: sphere and axis-aligned box shapes only, naive
// O(n^2) broad+narrow phase, impulse-based resolution with positional
// correction, and NO rotation/angular dynamics (bodies translate only).
// That's plenty for gameplay-style physics (falling props, projectiles,
// triggers, simple stacking) without dragging in a full SAT/GJK solver.
#ifndef PHYSICS_HPP
#define PHYSICS_HPP

#include "../graphics/renderer.hpp"

#include <unordered_map>
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace Physics {

using Graphics::Vec3;
using Graphics::Dot;
using Graphics::Cross;
using Graphics::Subtract;
using Graphics::Normalize;

// =========================================================================
// QUALITY / DYNAMIC LEVEL SHIFTING
// =========================================================================
// Higher levels run more integration substeps and more collision-
// resolution iterations per Step() -- more accurate (less tunneling,
// firmer stacking) but proportionally pricier. World::AutoAdjustQuality
// picks a level automatically each Step based on how many active bodies
// exist, unless manualQuality is set (see World::quality / SetQuality).
enum class QualityLevel : int { Low = 0, Medium = 1, High = 2, Ultra = 3 };

struct QualityProfile {
    int substeps;
    int iterations;
};

inline QualityProfile GetQualityProfile(QualityLevel level) {
    switch (level) {
        case QualityLevel::Low:    return {1, 1};
        case QualityLevel::Medium: return {2, 2};
        case QualityLevel::High:   return {4, 4};
        case QualityLevel::Ultra:  return {8, 6};
    }
    return {2, 2};
}

// =========================================================================
// COLLISION SHAPES
// =========================================================================
enum class ShapeType : int { Sphere = 0, Box = 1 };

struct Shape {
    ShapeType type = ShapeType::Sphere;
    float radius = 0.5f;                 // used when type == Sphere
    Vec3 halfExtents{0.5f, 0.5f, 0.5f};   // used when type == Box (axis-aligned only)
};

// Solid-shape volume -- used by Body::SetDensity to derive mass (mass =
// density * volume) instead of setting mass directly.
inline float ComputeVolume(const Shape& shape) {
    if (shape.type == ShapeType::Sphere) {
        return (4.0f / 3.0f) * 3.14159265f * shape.radius * shape.radius * shape.radius;
    }
    // Box full dimensions are (2*hx, 2*hy, 2*hz).
    return 8.0f * shape.halfExtents.x * shape.halfExtents.y * shape.halfExtents.z;
}

// =========================================================================
// RIGID BODY
// =========================================================================
struct Body {
    Vec3 position{0.0f, 0.0f, 0.0f};
    Vec3 velocity{0.0f, 0.0f, 0.0f};
    Vec3 force{0.0f, 0.0f, 0.0f}; // accumulated this step via ApplyForce; cleared after every Step()

    // Rotation is Euler angles in radians, same convention GameObject.Rotation
    // uses -- so a bound body's rotation drops straight onto the visual
    // object with no conversion. angularVelocity is the corresponding
    // per-axis rate (radians/sec); torque accumulates via ApplyTorque and
    // clears after every Step(), same lifecycle as `force`.
    Vec3 rotation{0.0f, 0.0f, 0.0f};
    Vec3 angularVelocity{0.0f, 0.0f, 0.0f};
    Vec3 torque{0.0f, 0.0f, 0.0f};
    float angularDamping = 0.01f;

    float mass = 1.0f;
    float invMass = 1.0f; // 0 for static/anchored bodies -- that's what makes them immovable
    float density = 1.0f; // informational unless SetDensity was used to derive `mass` from it; independent of `mass` otherwise
    float restitution = 0.4f;   // bounciness, 0 (no bounce) .. 1 (perfectly elastic). A contact uses min(a, b).
    float friction = 0.2f;      // tangential grip on contact, 0 (ice) .. 1 (very grippy). A contact uses min(a, b). This is also what makes a pushed sphere roll -- see World::ResolveContact.
    float linearDamping = 0.01f; // per-substep velocity decay, crude air-resistance stand-in

    bool isStatic = false; // "anchored" -- infinite mass, forces/impulses/collisions never move it, but other bodies still collide against it
    bool isEnabled = true; // false = frozen in place, skipped by integration AND collision entirely (cheaper than destroying it)

    Shape shape;

    // Physics groups: a body only collides with another if EACH considers
    // the other's `group` bit present in its own `collidesWith` mask. A
    // body with collidesWith == 0 (or a mask that never matches anything
    // relevant) still gets simulated (gravity/integration still moves it,
    // unless static) but never resolves a contact -- see ShouldCollide.
    uint32_t group = 1;
    uint32_t collidesWith = 0xFFFFFFFFu;

    void SetMass(float m) {
        mass = m;
        invMass = (m > 0.0f) ? (1.0f / m) : 0.0f;
    }

    // Alternative to SetMass: derives mass from density * this body's
    // current shape volume (mass = density * volume). Independent of
    // SetMass otherwise -- changing the shape afterward does NOT
    // auto-recompute mass, call SetDensity again if you want that.
    void SetDensity(float d) {
        density = d;
        float volume = ComputeVolume(shape);
        if (volume > 0.0f) SetMass(d * volume);
    }

    void SetStatic(bool s) {
        isStatic = s;
        if (s) {
            invMass = 0.0f;
            velocity = {0.0f, 0.0f, 0.0f};
            angularVelocity = {0.0f, 0.0f, 0.0f};
        } else {
            invMass = (mass > 0.0f) ? (1.0f / mass) : 0.0f;
        }
    }
};

// Solid-shape moment of inertia about the body's own center, per axis
// (diagonal only -- no cross terms/products of inertia, i.e. this treats
// each axis independently rather than using a full 3x3 tensor). Exact for
// a sphere (isotropic, so this has zero error there); an approximation
// for a box, but a standard and effective one for arcade-style rigid
// body response.
inline Vec3 ComputeInertia(const Shape& shape, float mass) {
    if (mass <= 0.0f) return {0.0f, 0.0f, 0.0f};

    if (shape.type == ShapeType::Sphere) {
        float i = 0.4f * mass * shape.radius * shape.radius; // solid sphere: (2/5) m r^2
        return {i, i, i};
    }

    // Solid box, full dimensions (2hx, 2hy, 2hz): I_xx = (m/12)((2hy)^2+(2hz)^2) = (m/3)(hy^2+hz^2), etc.
    float hx2 = shape.halfExtents.x * shape.halfExtents.x;
    float hy2 = shape.halfExtents.y * shape.halfExtents.y;
    float hz2 = shape.halfExtents.z * shape.halfExtents.z;
    return {
        (mass / 3.0f) * (hy2 + hz2),
        (mass / 3.0f) * (hx2 + hz2),
        (mass / 3.0f) * (hx2 + hy2)
    };
}

inline Vec3 ComputeInverseInertia(const Body& body) {
    if (body.isStatic || body.invMass <= 0.0f) return {0.0f, 0.0f, 0.0f};

    Vec3 inertia = ComputeInertia(body.shape, body.mass);
    return {
        inertia.x > 0.0001f ? 1.0f / inertia.x : 0.0f,
        inertia.y > 0.0001f ? 1.0f / inertia.y : 0.0f,
        inertia.z > 0.0001f ? 1.0f / inertia.z : 0.0f
    };
}

inline bool ShouldCollide(const Body& a, const Body& b) {
    if (a.isStatic && b.isStatic) return false; // nothing to resolve between two immovable bodies
    return (a.group & b.collidesWith) != 0 && (b.group & a.collidesWith) != 0;
}

// =========================================================================
// NARROW-PHASE COLLISION (sphere-sphere, sphere-box, box-box)
// =========================================================================
struct Contact {
    Vec3 normal{0.0f, 1.0f, 0.0f}; // always points from body A toward body B
    float penetration = 0.0f;      // positive = overlapping by this much
    Vec3 point{0.0f, 0.0f, 0.0f};  // approximate world-space contact point -- used to derive rolling/torque response, see World::ResolveContact
};

inline bool SphereVsSphere(const Body& a, const Body& b, Contact& out) {
    Vec3 delta = Subtract(b.position, a.position);
    float dist2 = Dot(delta, delta);
    float radiusSum = a.shape.radius + b.shape.radius;
    if (dist2 >= radiusSum * radiusSum) return false;

    float dist = std::sqrt(dist2);
    out.normal = (dist > 0.0001f) ? Vec3{delta.x / dist, delta.y / dist, delta.z / dist} : Vec3{0.0f, 1.0f, 0.0f};
    out.penetration = radiusSum - dist;

    // Contact point: midpoint of the two surface points along the normal.
    Vec3 surfaceA = { a.position.x + out.normal.x * a.shape.radius, a.position.y + out.normal.y * a.shape.radius, a.position.z + out.normal.z * a.shape.radius };
    Vec3 surfaceB = { b.position.x - out.normal.x * b.shape.radius, b.position.y - out.normal.y * b.shape.radius, b.position.z - out.normal.z * b.shape.radius };
    out.point = { (surfaceA.x + surfaceB.x) * 0.5f, (surfaceA.y + surfaceB.y) * 0.5f, (surfaceA.z + surfaceB.z) * 0.5f };
    return true;
}

inline Vec3 ClosestPointOnBox(const Vec3& point, const Vec3& boxCenter, const Vec3& halfExtents) {
    return {
        (std::max)(boxCenter.x - halfExtents.x, (std::min)(point.x, boxCenter.x + halfExtents.x)),
        (std::max)(boxCenter.y - halfExtents.y, (std::min)(point.y, boxCenter.y + halfExtents.y)),
        (std::max)(boxCenter.z - halfExtents.z, (std::min)(point.z, boxCenter.z + halfExtents.z))
    };
}

// Normal points from the box's surface toward the sphere's center (i.e. a
// "box -> sphere" direction) -- callers combine/flip this to whatever A->B
// convention their two bodies need; see ComputeContact.
inline bool SphereVsBox(const Vec3& spherePos, float sphereRadius, const Vec3& boxPos, const Vec3& boxHalfExtents, Contact& out) {
    Vec3 closest = ClosestPointOnBox(spherePos, boxPos, boxHalfExtents);
    Vec3 delta = Subtract(spherePos, closest);
    float dist2 = Dot(delta, delta);
    if (dist2 >= sphereRadius * sphereRadius) return false;

    float dist = std::sqrt(dist2);
    out.normal = (dist > 0.0001f) ? Vec3{delta.x / dist, delta.y / dist, delta.z / dist} : Vec3{0.0f, 1.0f, 0.0f};
    out.penetration = sphereRadius - dist;
    out.point = closest; // already the closest point on the box's surface -- a solid approximation of the true contact point
    return true;
}

// Resolves along the axis of least penetration -- the standard cheap AABB
// trick; not exact for corner-on-corner cases but plenty for gameplay.
inline bool BoxVsBox(const Body& a, const Body& b, Contact& out) {
    Vec3 delta = Subtract(b.position, a.position);
    Vec3 overlap = {
        (a.shape.halfExtents.x + b.shape.halfExtents.x) - std::fabs(delta.x),
        (a.shape.halfExtents.y + b.shape.halfExtents.y) - std::fabs(delta.y),
        (a.shape.halfExtents.z + b.shape.halfExtents.z) - std::fabs(delta.z)
    };
    if (overlap.x <= 0.0f || overlap.y <= 0.0f || overlap.z <= 0.0f) return false;

    if (overlap.x < overlap.y && overlap.x < overlap.z) {
        out.normal = { delta.x < 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f };
        out.penetration = overlap.x;
    } else if (overlap.y < overlap.z) {
        out.normal = { 0.0f, delta.y < 0.0f ? -1.0f : 1.0f, 0.0f };
        out.penetration = overlap.y;
    } else {
        out.normal = { 0.0f, 0.0f, delta.z < 0.0f ? -1.0f : 1.0f };
        out.penetration = overlap.z;
    }

    // Approximate contact point: midpoint of the two centers. Not exact --
    // a true box-box contact can be a face, edge, or single point -- but
    // good enough to derive a believable torque/rolling response.
    out.point = { (a.position.x + b.position.x) * 0.5f, (a.position.y + b.position.y) * 0.5f, (a.position.z + b.position.z) * 0.5f };
    return true;
}

// Dispatches to the right narrow-phase test for whatever shape combination
// `a` and `b` have, always returning a contact normal pointing A -> B.
inline bool ComputeContact(const Body& a, const Body& b, Contact& out) {
    if (a.shape.type == ShapeType::Sphere && b.shape.type == ShapeType::Sphere) {
        return SphereVsSphere(a, b, out);
    }
    if (a.shape.type == ShapeType::Box && b.shape.type == ShapeType::Box) {
        return BoxVsBox(a, b, out);
    }
    if (a.shape.type == ShapeType::Box && b.shape.type == ShapeType::Sphere) {
        // SphereVsBox naturally returns a box->sphere normal, i.e. exactly A->B here.
        return SphereVsBox(b.position, b.shape.radius, a.position, a.shape.halfExtents, out);
    }
    // a is the sphere, b is the box: SphereVsBox gives box->sphere (B->A) -- flip it.
    if (!SphereVsBox(a.position, a.shape.radius, b.position, b.shape.halfExtents, out)) return false;
    out.normal = { -out.normal.x, -out.normal.y, -out.normal.z };
    return true;
}

// =========================================================================
// WORLD
// =========================================================================
struct World {
    std::unordered_map<int, Body> bodies;
    int nextBodyId = 1;

    Vec3 gravity{0.0f, -9.81f, 0.0f};

    // --- Quality / dynamic level shifting ---
    QualityLevel quality = QualityLevel::Medium;
    bool manualQuality = false; // true = pinned by SetQuality; AutoAdjustQuality becomes a no-op until re-enabled

    // Active (enabled, non-static) body count thresholds AutoAdjustQuality
    // compares against each Step when not manually pinned. There's no real
    // profiling hook at this layer, so active body count is used as a
    // cheap stand-in for "how expensive is this frame" -- it's this
    // simulation's dominant cost driver anyway (O(n^2) collision checks).
    int lowThreshold = 40;    // >= this many active bodies -> Low
    int mediumThreshold = 20; // >= this many -> Medium
    int highThreshold = 8;    // >= this many -> High; fewer than that -> Ultra

    int AddBody(const Body& body) {
        int id = nextBodyId++;
        bodies[id] = body;
        return id;
    }

    void RemoveBody(int id) {
        bodies.erase(id);
    }

    Body* GetBody(int id) {
        auto it = bodies.find(id);
        return it != bodies.end() ? &it->second : nullptr;
    }

    void AutoAdjustQuality() {
        if (manualQuality) return;

        int activeCount = 0;
        for (auto& pair : bodies) {
            if (pair.second.isEnabled && !pair.second.isStatic) activeCount++;
        }

        if (activeCount >= lowThreshold) quality = QualityLevel::Low;
        else if (activeCount >= mediumThreshold) quality = QualityLevel::Medium;
        else if (activeCount >= highThreshold) quality = QualityLevel::High;
        else quality = QualityLevel::Ultra;
    }

    // Advances the simulation by `dt` seconds using however many substeps
    // and collision-resolution iterations the current quality level calls
    // for. Call once per frame/tick with your real frame delta -- the
    // substepping happens internally.
    void Step(float dt) {
        AutoAdjustQuality();
        QualityProfile profile = GetQualityProfile(quality);
        float subDt = dt / (float)profile.substeps;

        for (int s = 0; s < profile.substeps; ++s) {
            Integrate(subDt);
            for (int iter = 0; iter < profile.iterations; ++iter) {
                ResolveCollisions();
            }
            for (auto& pair : bodies) {
                pair.second.force = {0.0f, 0.0f, 0.0f};
                pair.second.torque = {0.0f, 0.0f, 0.0f};
            }
        }
    }

    void Integrate(float dt) {
        for (auto& pair : bodies) {
            Body& b = pair.second;
            if (!b.isEnabled || b.isStatic) continue;

            // Linear
            b.velocity.x += (b.force.x * b.invMass + gravity.x) * dt;
            b.velocity.y += (b.force.y * b.invMass + gravity.y) * dt;
            b.velocity.z += (b.force.z * b.invMass + gravity.z) * dt;

            float damp = (std::max)(0.0f, 1.0f - b.linearDamping);
            b.velocity.x *= damp;
            b.velocity.y *= damp;
            b.velocity.z *= damp;

            b.position.x += b.velocity.x * dt;
            b.position.y += b.velocity.y * dt;
            b.position.z += b.velocity.z * dt;

            // Angular -- per-axis, using this body's own moment of inertia
            // (diagonal only, see ComputeInertia). This is what turns
            // ApplyTorque, and the rolling torque ResolveContact derives
            // from friction, into an actual spin that then rotates the
            // bound GameObject once StepPhysics syncs it.
            Vec3 invInertia = ComputeInverseInertia(b);
            b.angularVelocity.x += b.torque.x * invInertia.x * dt;
            b.angularVelocity.y += b.torque.y * invInertia.y * dt;
            b.angularVelocity.z += b.torque.z * invInertia.z * dt;

            float angDamp = (std::max)(0.0f, 1.0f - b.angularDamping);
            b.angularVelocity.x *= angDamp;
            b.angularVelocity.y *= angDamp;
            b.angularVelocity.z *= angDamp;

            b.rotation.x += b.angularVelocity.x * dt;
            b.rotation.y += b.angularVelocity.y * dt;
            b.rotation.z += b.angularVelocity.z * dt;
        }
    }

    void ResolveCollisions() {
        // Naive O(n^2) broad+narrow phase -- perfectly fine at the body
        // counts a hobby engine like this deals with, and it's exactly
        // what AutoAdjustQuality's thresholds are tuned against.
        std::vector<int> ids;
        ids.reserve(bodies.size());
        for (auto& pair : bodies) ids.push_back(pair.first);

        for (size_t i = 0; i < ids.size(); ++i) {
            Body* a = GetBody(ids[i]);
            if (!a || !a->isEnabled) continue;
            for (size_t j = i + 1; j < ids.size(); ++j) {
                Body* b = GetBody(ids[j]);
                if (!b || !b->isEnabled) continue;
                if (!ShouldCollide(*a, *b)) continue;

                Contact c;
                if (!ComputeContact(*a, *b, c)) continue;
                ResolveContact(*a, *b, c);
            }
        }
    }

    // How much a diagonal inverse-inertia tensor resists rotation from an
    // impulse applied at offset `r` along direction `n` -- the rotational
    // term in the classic impulse-with-rotation denominator:
    //   dot( invInertia * (r x n), r x n... via one more cross with r ) -- see below.
    static float AngularEffect(const Vec3& invInertia, const Vec3& r, const Vec3& n) {
        Vec3 rxn = Cross(r, n);
        Vec3 scaled = { rxn.x * invInertia.x, rxn.y * invInertia.y, rxn.z * invInertia.z }; // valid since invInertia is diagonal
        Vec3 crossed = Cross(scaled, r);
        return Dot(crossed, n);
    }

    void ResolveContact(Body& a, Body& b, const Contact& c) {
        float totalInvMass = a.invMass + b.invMass;
        if (totalInvMass <= 0.0f) return; // both static/anchored -- nothing to do

        // Positional correction: push the two bodies apart along the
        // contact normal, split by relative (inverse) mass. Purely
        // linear -- penetration doesn't need a rotational fix.
        float corrA = c.penetration * (a.invMass / totalInvMass);
        float corrB = c.penetration * (b.invMass / totalInvMass);
        a.position.x -= c.normal.x * corrA;
        a.position.y -= c.normal.y * corrA;
        a.position.z -= c.normal.z * corrA;
        b.position.x += c.normal.x * corrB;
        b.position.y += c.normal.y * corrB;
        b.position.z += c.normal.z * corrB;

        Vec3 invInertiaA = ComputeInverseInertia(a);
        Vec3 invInertiaB = ComputeInverseInertia(b);
        Vec3 rA = Subtract(c.point, a.position); // contact point relative to A's center
        Vec3 rB = Subtract(c.point, b.position); // contact point relative to B's center

        // Velocity AT the contact point, i.e. linear velocity plus each
        // body's own spin (v_point = v + omega x r) -- this is what makes
        // a spinning body's surface speed matter to friction, and
        // therefore what lets a rolling ball touch down "correctly"
        // instead of always behaving like a frictionless point mass.
        Vec3 velA = {
            a.velocity.x + (a.angularVelocity.y * rA.z - a.angularVelocity.z * rA.y),
            a.velocity.y + (a.angularVelocity.z * rA.x - a.angularVelocity.x * rA.z),
            a.velocity.z + (a.angularVelocity.x * rA.y - a.angularVelocity.y * rA.x)
        };
        Vec3 velB = {
            b.velocity.x + (b.angularVelocity.y * rB.z - b.angularVelocity.z * rB.y),
            b.velocity.y + (b.angularVelocity.z * rB.x - b.angularVelocity.x * rB.z),
            b.velocity.z + (b.angularVelocity.x * rB.y - b.angularVelocity.y * rB.x)
        };
        Vec3 relVel = Subtract(velB, velA);

        float velAlongNormal = Dot(relVel, c.normal);
        if (velAlongNormal > 0.0f) return; // already separating, nothing to resolve

        float restitution = (std::min)(a.restitution, b.restitution);
        float normalDenom = totalInvMass + AngularEffect(invInertiaA, rA, c.normal) + AngularEffect(invInertiaB, rB, c.normal);
        if (normalDenom <= 0.0001f) normalDenom = (totalInvMass > 0.0001f) ? totalInvMass : 1.0f;

        float impulseMag = -(1.0f + restitution) * velAlongNormal / normalDenom;
        Vec3 impulse = { c.normal.x * impulseMag, c.normal.y * impulseMag, c.normal.z * impulseMag };

        // Linear response to the normal impulse.
        a.velocity.x -= impulse.x * a.invMass;
        a.velocity.y -= impulse.y * a.invMass;
        a.velocity.z -= impulse.z * a.invMass;
        b.velocity.x += impulse.x * b.invMass;
        b.velocity.y += impulse.y * b.invMass;
        b.velocity.z += impulse.z * b.invMass;

        // Angular response to the normal impulse (torque = r x J) -- zero
        // for a sphere hitting dead-on through its center, but real for
        // any off-center contact (e.g. hitting the corner of a box).
        Vec3 negImpulse = { -impulse.x, -impulse.y, -impulse.z };
        Vec3 torqueA = Cross(rA, negImpulse);
        Vec3 torqueB = Cross(rB, impulse);
        a.angularVelocity.x += torqueA.x * invInertiaA.x;
        a.angularVelocity.y += torqueA.y * invInertiaA.y;
        a.angularVelocity.z += torqueA.z * invInertiaA.z;
        b.angularVelocity.x += torqueB.x * invInertiaB.x;
        b.angularVelocity.y += torqueB.y * invInertiaB.y;
        b.angularVelocity.z += torqueB.z * invInertiaB.z;

        // Friction: tangential to the normal, computed from the SAME
        // contact-point relative velocity (so an object's own spin feeds
        // back into it) -- this is the actual mechanism that makes a
        // sliding/pushed sphere start rolling, and a spinning one that
        // touches down grip and start moving.
        float friction = (std::min)(a.friction, b.friction);
        if (friction > 0.0f) {
            Vec3 tangentVel = {
                relVel.x - c.normal.x * velAlongNormal,
                relVel.y - c.normal.y * velAlongNormal,
                relVel.z - c.normal.z * velAlongNormal
            };
            float tangentSpeed = std::sqrt(Dot(tangentVel, tangentVel));
            if (tangentSpeed > 0.0001f) {
                Vec3 tangent = { tangentVel.x / tangentSpeed, tangentVel.y / tangentSpeed, tangentVel.z / tangentSpeed };

                float frictionDenom = totalInvMass + AngularEffect(invInertiaA, rA, tangent) + AngularEffect(invInertiaB, rB, tangent);
                if (frictionDenom <= 0.0001f) frictionDenom = (totalInvMass > 0.0001f) ? totalInvMass : 1.0f;

                float frictionImpulseMag = -Dot(relVel, tangent) * friction / frictionDenom;

                // Coulomb clamp: friction can't exceed friction * normal impulse, or it could reverse/overshoot velocity unrealistically.
                float maxFriction = std::fabs(impulseMag) * friction;
                if (frictionImpulseMag > maxFriction) frictionImpulseMag = maxFriction;
                if (frictionImpulseMag < -maxFriction) frictionImpulseMag = -maxFriction;

                Vec3 frictionImpulse = { tangent.x * frictionImpulseMag, tangent.y * frictionImpulseMag, tangent.z * frictionImpulseMag };

                a.velocity.x -= frictionImpulse.x * a.invMass;
                a.velocity.y -= frictionImpulse.y * a.invMass;
                a.velocity.z -= frictionImpulse.z * a.invMass;
                b.velocity.x += frictionImpulse.x * b.invMass;
                b.velocity.y += frictionImpulse.y * b.invMass;
                b.velocity.z += frictionImpulse.z * b.invMass;

                // Torque from friction -- THIS is what makes a pushed
                // sphere start to roll, and a dropped-with-spin object
                // "catch" against the surface instead of sliding forever.
                Vec3 negFrictionImpulse = { -frictionImpulse.x, -frictionImpulse.y, -frictionImpulse.z };
                Vec3 frictionTorqueA = Cross(rA, negFrictionImpulse);
                Vec3 frictionTorqueB = Cross(rB, frictionImpulse);
                a.angularVelocity.x += frictionTorqueA.x * invInertiaA.x;
                a.angularVelocity.y += frictionTorqueA.y * invInertiaA.y;
                a.angularVelocity.z += frictionTorqueA.z * invInertiaA.z;
                b.angularVelocity.x += frictionTorqueB.x * invInertiaB.x;
                b.angularVelocity.y += frictionTorqueB.y * invInertiaB.y;
                b.angularVelocity.z += frictionTorqueB.z * invInertiaB.z;
            }
        }
    }
};

// =========================================================================
// PREDICTION
// =========================================================================
// Predicts where a body would be after `dt` seconds under its current
// velocity/force/gravity, WITHOUT mutating it or resolving any collisions
// -- useful for trajectory previews, AI lookahead, or a cheap swept check
// (see PredictImpact below).
inline Vec3 PredictPosition(const Body& body, const Vec3& gravity, float dt) {
    if (body.isStatic) return body.position;

    Vec3 predictedVel = {
        body.velocity.x + (body.force.x * body.invMass + gravity.x) * dt,
        body.velocity.y + (body.force.y * body.invMass + gravity.y) * dt,
        body.velocity.z + (body.force.z * body.invMass + gravity.z) * dt
    };
    return {
        body.position.x + predictedVel.x * dt,
        body.position.y + predictedVel.y * dt,
        body.position.z + predictedVel.z * dt
    };
}

struct PredictedImpact {
    bool willCollide = false;
    float timeToImpact = 0.0f;
    Vec3 position{0.0f, 0.0f, 0.0f}; // predicted position at impact (or at the end of dt if no impact found)
};

// Samples a body's predicted path over `samples` steps across `dt` seconds
// against `target`'s CURRENT shape/position, and reports the first sample
// (if any) that overlaps it. This is a sampled approximation, not true
// continuous-collision root-finding -- fast, and plenty for gameplay
// warnings ("is the ball about to hit the wall?") or simple AI decisions.
// Operates on a scratch copy, so it never mutates `body` or `target`.
inline PredictedImpact PredictImpact(Body body, const Body& target, const Vec3& gravity, float dt, int samples = 8) {
    PredictedImpact result;
    if (samples < 1) samples = 1;
    float step = dt / (float)samples;

    for (int i = 1; i <= samples; ++i) {
        Vec3 nextPos = PredictPosition(body, gravity, step);
        body.velocity.x += (body.force.x * body.invMass + gravity.x) * step;
        body.velocity.y += (body.force.y * body.invMass + gravity.y) * step;
        body.velocity.z += (body.force.z * body.invMass + gravity.z) * step;
        body.position = nextPos;

        Contact c;
        if (ComputeContact(body, target, c)) {
            result.willCollide = true;
            result.timeToImpact = step * (float)i;
            result.position = nextPos;
            return result;
        }
    }

    result.position = body.position;
    return result;
}

} // namespace Physics

#endif
