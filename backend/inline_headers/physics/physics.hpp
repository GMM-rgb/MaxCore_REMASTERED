// =========================================================================
// inline_headers/physics/physics.hpp
// =========================================================================
// A small, self-contained rigid-body physics core -- the physics sibling
// of renderer.hpp. Reuses Graphics::Vec3 and its math helpers so a body's
// position is the exact same type the renderer projects, no conversion
// needed at the boundary.
//
// Scope: sphere and box shapes (boxes are properly ORIENTED -- collision
// respects each body's current rotation via an OBB separating-axis test,
// not just its axis-aligned bounding box), naive O(n^2) broad+narrow
// phase, impulse-based resolution with positional correction, and full
// angular dynamics (rotation, torque, rolling via friction). A body's
// rotation can be locked (Body::SetRotationLocked) so physics only ever
// moves its position, never its orientation, while direct teleport-style
// sets (PhysicsBody:SetRotation) still work regardless of the lock.
#ifndef PHYSICS_HPP
#define PHYSICS_HPP

#include "../graphics/renderer.hpp"

#include <unordered_map>
#include <unordered_set>
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
enum class ShapeType : int { Sphere = 0, Box = 1, Hull = 2 };

// A Hull is an arbitrary CONVEX polyhedron -- the physics counterpart of
// a MeshObject's Vertices/Faces, letting collision test against the
// mesh's ACTUAL geometry (every real face normal + edge) instead of a
// box/sphere standing in for it. Concave meshes are NOT supported --
// every face/edge test below assumes convexity; a concave mesh needs to
// be split into convex pieces (each its own Hull-shaped Body) by whoever
// builds it. All fields here are LOCAL-space (relative to the owning
// Body's position/rotation) and precomputed once by Shape::SetHull, not
// every collision check.
struct HullData {
    std::vector<Vec3> vertices;   // as given, local-space
    std::vector<Vec3> faceNormals; // one per face, deduplicated, local-space unit vectors
    std::vector<Vec3> edgeDirs;    // one per unique edge direction, deduplicated (parallel/anti-parallel merged), local-space unit vectors
    Vec3 aabbHalfExtents{0.5f, 0.5f, 0.5f}; // local-space AABB of `vertices` -- used only as a cheap volume/inertia stand-in, see ComputeVolume/ComputeInertia
};

struct Shape {
    ShapeType type = ShapeType::Sphere;
    float radius = 0.5f;                 // used when type == Sphere
    Vec3 halfExtents{0.5f, 0.5f, 0.5f};   // used when type == Box -- ORIENTED per the owning Body's rotation, see BoxVsBox
    HullData hull;                        // used when type == Hull, see SetHull below

    // Builds this shape's convex hull data from a mesh's raw vertex list
    // and face list (each face 3+ 1-based-elsewhere-but-0-based-HERE
    // indices, wound counter-clockwise viewed from outside -- the SAME
    // convention MeshObject/draw_mesh already documents and requires for
    // correct shading, so a mesh that renders correctly also collides
    // correctly with no extra work). Call this once after construction;
    // it does the one-time face-normal/edge extraction and dedup work so
    // per-Step collision checks don't have to.
    void SetHull(const std::vector<Vec3>& localVertices, const std::vector<std::vector<int>>& faces) {
        type = ShapeType::Hull;
        hull.vertices = localVertices;
        hull.faceNormals.clear();
        hull.edgeDirs.clear();
        if (localVertices.empty()) return;

        Vec3 minV = localVertices[0], maxV = localVertices[0];
        for (const Vec3& v : localVertices) {
            minV.x = (std::min)(minV.x, v.x); maxV.x = (std::max)(maxV.x, v.x);
            minV.y = (std::min)(minV.y, v.y); maxV.y = (std::max)(maxV.y, v.y);
            minV.z = (std::min)(minV.z, v.z); maxV.z = (std::max)(maxV.z, v.z);
        }
        hull.aabbHalfExtents = { (maxV.x - minV.x) * 0.5f, (maxV.y - minV.y) * 0.5f, (maxV.z - minV.z) * 0.5f };

        auto addUniqueAxis = [](std::vector<Vec3>& list, Vec3 axis, bool mergeOpposites) {
            float len = std::sqrt(Dot(axis, axis));
            if (len < 0.00001f) return; // degenerate face/edge -- skip
            axis = { axis.x / len, axis.y / len, axis.z / len };
            for (const Vec3& existing : list) {
                float d = Dot(existing, axis);
                if (d > 0.9995f) return; // already have this direction
                if (mergeOpposites && d < -0.9995f) return; // already have the opposite (edges don't care which way they point)
            }
            list.push_back(axis);
        };

        for (const std::vector<int>& face : faces) {
            if (face.size() < 3) continue;
            // Face normal via cross of the first two edges -- correct as
            // long as the first 3 vertices aren't collinear, which a
            // valid face never is.
            Vec3 v0 = localVertices[face[0]];
            Vec3 v1 = localVertices[face[1]];
            Vec3 v2 = localVertices[face[2]];
            Vec3 normal = Cross(Subtract(v1, v0), Subtract(v2, v0));
            addUniqueAxis(hull.faceNormals, normal, false);

            for (size_t i = 0; i < face.size(); ++i) {
                Vec3 pa = localVertices[face[i]];
                Vec3 pb = localVertices[face[(i + 1) % face.size()]];
                addUniqueAxis(hull.edgeDirs, Subtract(pb, pa), true);
            }
        }
    }
};

// Solid-shape volume -- used by Body::SetDensity to derive mass (mass =
// density * volume) instead of setting mass directly.
inline float ComputeVolume(const Shape& shape) {
    if (shape.type == ShapeType::Sphere) {
        return (4.0f / 3.0f) * 3.14159265f * shape.radius * shape.radius * shape.radius;
    }
    if (shape.type == ShapeType::Hull) {
        // Approximation: the hull's own local AABB volume, not its true
        // (generally smaller) convex volume -- exact convex-polyhedron
        // volume needs a full tetrahedron decomposition; this is the
        // same tier of approximation ComputeInertia below already makes.
        return 8.0f * shape.hull.aabbHalfExtents.x * shape.hull.aabbHalfExtents.y * shape.hull.aabbHalfExtents.z;
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

    // When true, this body's orientation is frozen from the physics
    // engine's point of view -- ComputeInverseInertia treats it as having
    // infinite rotational inertia (same trick already used for isStatic),
    // so torque/collision angular response can never spin it, and
    // Integrate skips advancing `rotation` from `angularVelocity` as a
    // second guarantee. Direct teleport-style sets (PhysicsBody:SetRotation
    // -> body_set_rotation) write `rotation` directly and are completely
    // unaffected -- only physics-DRIVEN rotation changes are blocked.
    bool rotationLocked = false;

    // Weld (see World::ApplyWelds): while weldParentId >= 0, this body is
    // KINEMATIC -- Integrate skips it entirely and every Step it's
    // snapped to weldParentId's current position/rotation, offset by
    // the relative transform captured at weld time (WeldTo/body_weld_to)
    // -- so it rides along rigidly, like it's bolted on. invMass is
    // forced to 0 for the same reason isStatic forces it to 0: neither
    // its own forces nor collisions can move it independently. This does
    // NOT roll its mass/inertia into the parent -- the parent doesn't
    // get heavier or harder to spin because of what's welded to it.
    int weldParentId = -1;
    Vec3 weldLocalOffset{0.0f, 0.0f, 0.0f};         // this body's position relative to weldParentId, in the PARENT's local space, captured at weld time
    Vec3 weldLocalRotationOffset{0.0f, 0.0f, 0.0f}; // this body's rotation minus the parent's, captured at weld time (Euler difference)

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

    // Locking snaps angularVelocity/torque to zero immediately so a body
    // that was mid-spin freezes instantly instead of coasting to a stop --
    // "only position can be modified" means right away, not eventually.
    void SetRotationLocked(bool locked) {
        rotationLocked = locked;
        if (locked) {
            angularVelocity = {0.0f, 0.0f, 0.0f};
            torque = {0.0f, 0.0f, 0.0f};
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

    // Hull: approximated as a solid box matching the hull's own local
    // AABB half-extents -- exact convex-polyhedron inertia needs the same
    // tetrahedron-decomposition integral ComputeVolume's exact version
    // would; this keeps hull bodies rotating/tumbling believably without
    // that. Reasonable for roughly box-ish/rounded meshes; a very long,
    // thin, or oddly-shaped mesh will feel a bit stiffer or looser than
    // the "true" physical answer.
    Vec3 halfExtents = (shape.type == ShapeType::Hull) ? shape.hull.aabbHalfExtents : shape.halfExtents;

    // Solid box, full dimensions (2hx, 2hy, 2hz): I_xx = (m/12)((2hy)^2+(2hz)^2) = (m/3)(hy^2+hz^2), etc.
    float hx2 = halfExtents.x * halfExtents.x;
    float hy2 = halfExtents.y * halfExtents.y;
    float hz2 = halfExtents.z * halfExtents.z;
    return {
        (mass / 3.0f) * (hy2 + hz2),
        (mass / 3.0f) * (hx2 + hz2),
        (mass / 3.0f) * (hx2 + hy2)
    };
}

inline Vec3 ComputeInverseInertia(const Body& body) {
    // rotationLocked reuses the exact same "treat it as infinite inertia"
    // trick isStatic already relies on -- every torque/collision angular
    // response in this file multiplies by invInertia, so this one check
    // is all it takes to make a locked body's rotation physics-immune
    // everywhere at once, without touching ResolveContact/Integrate/etc.
    if (body.isStatic || body.rotationLocked || body.invMass <= 0.0f) return {0.0f, 0.0f, 0.0f};

    Vec3 inertia = ComputeInertia(body.shape, body.mass);
    return {
        inertia.x > 0.0001f ? 1.0f / inertia.x : 0.0f,
        inertia.y > 0.0001f ? 1.0f / inertia.y : 0.0f,
        inertia.z > 0.0001f ? 1.0f / inertia.z : 0.0f
    };
}

inline bool ShouldCollide(const Body& a, const Body& b, int idA, int idB) {
    if (a.isStatic && b.isStatic) return false; // nothing to resolve between two immovable bodies
    // A directly welded pair (either direction) is meant to sit rigidly
    // together, typically flush/overlapping by design (a wheel welded to
    // a chassis, a panel welded onto a frame) -- without this, the
    // dynamic side would get fought every Step by full-penetration
    // correction against its own now-immovable (invMass 0) welded part.
    if (a.weldParentId == idB || b.weldParentId == idA) return false;
    return (a.group & b.collidesWith) != 0 && (b.group & a.collidesWith) != 0;
}

// Order-independent identity for a body pair, used by World's touch
// tracking (touchingPairs/newCollisions/IsBodyColliding) below -- packs
// the smaller id into the low 32 bits and the larger into the high 32
// bits so (a,b) and (b,a) always hash/compare identically.
inline uint64_t PairKey(int a, int b) {
    uint32_t lo = (uint32_t)((std::min)(a, b));
    uint32_t hi = (uint32_t)((std::max)(a, b));
    return (uint64_t(hi) << 32) | uint64_t(lo);
}

// =========================================================================
// ORIENTATION HELPERS
// =========================================================================
// Rotates a LOCAL-space vector into WORLD-space orientation using a
// body's Euler rotation -- composing Z, then Y, then X, which is the
// EXACT same order window_management.cpp's draw_cube/draw_mesh apply when
// rendering. Keeping this in lockstep with the renderer is the whole
// point: a box's collision shape (BoxVsBox below) now rotates in
// agreement with what's actually drawn, instead of always staying axis-
// aligned no matter how the body has spun.
inline Vec3 RotateLocalToWorld(const Vec3& local, const Vec3& rotation) {
    Graphics::Matrix4x4 rotZ = Graphics::Matrix4x4::RotationZ(rotation.z);
    Graphics::Matrix4x4 rotY = Graphics::Matrix4x4::RotationY(rotation.y);
    Graphics::Matrix4x4 rotX = Graphics::Matrix4x4::RotationX(rotation.x);
    Graphics::Vec4 rZ = rotZ.MultiplyVector(local);
    Vec3 rZ3 = {rZ.x, rZ.y, rZ.z};
    Graphics::Vec4 rY = rotY.MultiplyVector(rZ3);
    Vec3 rY3 = {rY.x, rY.y, rY.z};
    Graphics::Vec4 rX = rotX.MultiplyVector(rY3);
    return {rX.x, rX.y, rX.z};
}

// Inverse of RotateLocalToWorld -- each per-axis rotation matrix is
// orthonormal, so the inverse of the combined rotation is the same three
// rotations applied in reverse order with negated angles.
inline Vec3 RotateWorldToLocal(const Vec3& world, const Vec3& rotation) {
    Graphics::Matrix4x4 rotXInv = Graphics::Matrix4x4::RotationX(-rotation.x);
    Graphics::Matrix4x4 rotYInv = Graphics::Matrix4x4::RotationY(-rotation.y);
    Graphics::Matrix4x4 rotZInv = Graphics::Matrix4x4::RotationZ(-rotation.z);
    Graphics::Vec4 rX = rotXInv.MultiplyVector(world);
    Vec3 rX3 = {rX.x, rX.y, rX.z};
    Graphics::Vec4 rY = rotYInv.MultiplyVector(rX3);
    Vec3 rY3 = {rY.x, rY.y, rY.z};
    Graphics::Vec4 rZ = rotZInv.MultiplyVector(rY3);
    return {rZ.x, rZ.y, rZ.z};
}

// This box's own world-space vertex that extends farthest along `dir` --
// its "support point" for that direction. Used to approximate a
// physically sensible box-box contact point (see BoxVsBox) without a
// full clipped contact manifold: the true contact for two boxes can be a
// face, edge, or single point, but the midpoint of each box's support
// point toward the other is a good, standard, cheap stand-in that -- 
// crucially -- actually sits ON the touching surface instead of
// somewhere between the two centers.
inline Vec3 SupportPoint(const Vec3& center, const Vec3& axisX, const Vec3& axisY, const Vec3& axisZ, const Vec3& halfExtents, const Vec3& dir) {
    float sx = Dot(dir, axisX) >= 0.0f ? halfExtents.x : -halfExtents.x;
    float sy = Dot(dir, axisY) >= 0.0f ? halfExtents.y : -halfExtents.y;
    float sz = Dot(dir, axisZ) >= 0.0f ? halfExtents.z : -halfExtents.z;
    return {
        center.x + axisX.x * sx + axisY.x * sy + axisZ.x * sz,
        center.y + axisX.y * sx + axisY.y * sy + axisZ.y * sz,
        center.z + axisX.z * sx + axisY.z * sy + axisZ.z * sz
    };
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
// convention their two bodies need; see ComputeContact. `boxRotation`
// makes this respect the box's actual current orientation instead of
// always treating it as axis-aligned: the sphere's position is rotated
// into the box's LOCAL space, the existing clamp-to-box logic runs there
// unchanged, and the result is rotated back out to world space.
inline bool SphereVsBox(const Vec3& spherePos, float sphereRadius, const Vec3& boxPos, const Vec3& boxRotation, const Vec3& boxHalfExtents, Contact& out) {
    Vec3 localSpherePos = RotateWorldToLocal(Subtract(spherePos, boxPos), boxRotation);
    Vec3 localClosest = ClosestPointOnBox(localSpherePos, {0.0f, 0.0f, 0.0f}, boxHalfExtents);
    Vec3 worldOffset = RotateLocalToWorld(localClosest, boxRotation);
    Vec3 closest = { boxPos.x + worldOffset.x, boxPos.y + worldOffset.y, boxPos.z + worldOffset.z };

    Vec3 delta = Subtract(spherePos, closest);
    float dist2 = Dot(delta, delta);
    if (dist2 >= sphereRadius * sphereRadius) return false;

    float dist = std::sqrt(dist2);
    out.normal = (dist > 0.0001f) ? Vec3{delta.x / dist, delta.y / dist, delta.z / dist} : Vec3{0.0f, 1.0f, 0.0f};
    out.penetration = sphereRadius - dist;
    out.point = closest; // already the closest point on the box's (rotated) surface -- a solid approximation of the true contact point
    return true;
}

// Oriented-box vs oriented-box, via the standard 15-axis separating-axis
// test (Ericson, "Real-Time Collision Detection"): each box's own 3 face
// normals, plus the 9 cross products of one box's edge axes with the
// other's. This is what makes box collision actually respect rotation --
// a tipped-over or rolling box collides using its TRUE current footprint,
// not the box it would have if it had never rotated -- which is why
// corners used to visibly clip through things: the old AABB-only test
// kept using the ORIGINAL unrotated half-extents no matter how far the
// body had actually spun.
//
// Among the 15 axes, the one with the SMALLEST positive overlap is the
// true separating axis (the minimum-translation direction); that becomes
// the contact normal/penetration. The contact point is then approximated
// as the midpoint between each box's own support point toward the other
// (see SupportPoint) -- not a full clipped manifold, but a properly
// ON-THE-SURFACE point instead of the old "midpoint of the two centers"
// (which, e.g. for a small box resting on a large flat ground box, could
// land nowhere near the actual touching surface -- garbage input for the
// torque/rolling math, which is why tipping/rolling used to look wrong
// even when the box itself wasn't visibly clipping).
inline bool BoxVsBox(const Body& a, const Body& b, Contact& out) {
    Vec3 axA[3] = {
        RotateLocalToWorld({1.0f, 0.0f, 0.0f}, a.rotation),
        RotateLocalToWorld({0.0f, 1.0f, 0.0f}, a.rotation),
        RotateLocalToWorld({0.0f, 0.0f, 1.0f}, a.rotation)
    };
    Vec3 axB[3] = {
        RotateLocalToWorld({1.0f, 0.0f, 0.0f}, b.rotation),
        RotateLocalToWorld({0.0f, 1.0f, 0.0f}, b.rotation),
        RotateLocalToWorld({0.0f, 0.0f, 1.0f}, b.rotation)
    };
    float eA[3] = { a.shape.halfExtents.x, a.shape.halfExtents.y, a.shape.halfExtents.z };
    float eB[3] = { b.shape.halfExtents.x, b.shape.halfExtents.y, b.shape.halfExtents.z };

    Vec3 t = Subtract(b.position, a.position);

    float R[3][3], AbsR[3][3];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            R[i][j] = Dot(axA[i], axB[j]);
            AbsR[i][j] = std::fabs(R[i][j]) + 0.0001f; // epsilon guards near-parallel edge-cross axes below
        }
    }
    float tA[3] = { Dot(t, axA[0]), Dot(t, axA[1]), Dot(t, axA[2]) };

    float bestOverlap = 1e30f;
    Vec3 bestAxis{0.0f, 1.0f, 0.0f};
    bool anySeparating = false;

    // Standard SAT early-exit: the moment ANY of the 15 axes proves
    // separation, the boxes can't be overlapping and the caller bails out
    // immediately (see the `if (anySeparating) return false;` after each
    // considerAxis call below). Right up until that happens, bestOverlap/
    // bestAxis track the smallest overlap seen so far -- once all 15
    // axes have been checked without finding a separating one, whichever
    // axis had the smallest overlap IS the true minimum-translation axis.
    auto considerAxis = [&](const Vec3& axis, float signedDist, float ra, float rb) {
        float len2 = Dot(axis, axis);
        if (len2 < 0.000001f) return; // degenerate (near-parallel edge cross) -- skip, another axis will catch it
        float invLen = 1.0f / std::sqrt(len2);
        float dist = std::fabs(signedDist) * invLen;
        float radius = (ra + rb) * invLen;
        float overlap = radius - dist;
        if (overlap <= 0.0f) { anySeparating = true; return; }
        if (overlap < bestOverlap) {
            bestOverlap = overlap;
            float sign = signedDist < 0.0f ? -1.0f : 1.0f;
            bestAxis = { axis.x * invLen * sign, axis.y * invLen * sign, axis.z * invLen * sign };
        }
    };

    // A's 3 face axes.
    for (int i = 0; i < 3; ++i) {
        float ra = eA[i];
        float rb = eB[0] * AbsR[i][0] + eB[1] * AbsR[i][1] + eB[2] * AbsR[i][2];
        considerAxis(axA[i], tA[i], ra, rb);
        if (anySeparating) return false;
    }
    // B's 3 face axes.
    for (int j = 0; j < 3; ++j) {
        float tBj = Dot(t, axB[j]);
        float ra = eA[0] * AbsR[0][j] + eA[1] * AbsR[1][j] + eA[2] * AbsR[2][j];
        float rb = eB[j];
        considerAxis(axB[j], tBj, ra, rb);
        if (anySeparating) return false;
    }
    // 9 edge-cross axes.
    static const int ix[3] = {1, 2, 0};
    static const int iy[3] = {2, 0, 1};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            Vec3 axis = Cross(axA[i], axB[j]);
            float ra = eA[ix[i]] * AbsR[iy[i]][j] + eA[iy[i]] * AbsR[ix[i]][j];
            float rb = eB[ix[j]] * AbsR[i][iy[j]] + eB[iy[j]] * AbsR[i][ix[j]];
            float signedDist = Dot(t, axis);
            considerAxis(axis, signedDist, ra, rb);
            if (anySeparating) return false;
        }
    }

    out.normal = bestAxis;   // already built pointing roughly A -> B via the sign flip in considerAxis
    out.penetration = bestOverlap;

    Vec3 negNormal = { -bestAxis.x, -bestAxis.y, -bestAxis.z };
    Vec3 supportA = SupportPoint(a.position, axA[0], axA[1], axA[2], a.shape.halfExtents, bestAxis);
    Vec3 supportB = SupportPoint(b.position, axB[0], axB[1], axB[2], b.shape.halfExtents, negNormal);
    out.point = { (supportA.x + supportB.x) * 0.5f, (supportA.y + supportB.y) * 0.5f, (supportA.z + supportB.z) * 0.5f };
    return true;
}

// =========================================================================
// GENERAL CONVEX POLYTOPE COLLISION (Hull <-> Hull, Hull <-> Box)
// =========================================================================
// A world-space view of ANY convex polytope (a Box's 8 corners/3 face
// normals/3 edges, or a Hull's real vertices/faces/edges) -- letting
// HullVsBody below run the exact same SAT test regardless of which kind
// of body it's looking at. This is the direct generalization of
// BoxVsBox's 15-axis test above (same idea: face normals + edge cross
// products), just with variable axis counts and vertex-projection
// min/max instead of the box's closed-form half-extent radius, since an
// arbitrary polytope doesn't have a simple "radius along an axis"
// formula the way a box does.
struct Polytope {
    std::vector<Vec3> vertices;
    std::vector<Vec3> faceNormals;
    std::vector<Vec3> edgeDirs;
};

inline Polytope BuildPolytope(const Body& body) {
    Polytope p;
    if (body.shape.type == ShapeType::Box) {
        const Vec3& he = body.shape.halfExtents;
        for (int sx = -1; sx <= 1; sx += 2) {
            for (int sy = -1; sy <= 1; sy += 2) {
                for (int sz = -1; sz <= 1; sz += 2) {
                    Vec3 local = { he.x * sx, he.y * sy, he.z * sz };
                    Vec3 world = RotateLocalToWorld(local, body.rotation);
                    p.vertices.push_back({ body.position.x + world.x, body.position.y + world.y, body.position.z + world.z });
                }
            }
        }
        p.faceNormals.push_back(RotateLocalToWorld({1.0f, 0.0f, 0.0f}, body.rotation));
        p.faceNormals.push_back(RotateLocalToWorld({0.0f, 1.0f, 0.0f}, body.rotation));
        p.faceNormals.push_back(RotateLocalToWorld({0.0f, 0.0f, 1.0f}, body.rotation));
        p.edgeDirs = p.faceNormals; // a box's 3 face normals ARE its 3 unique edge directions
    } else if (body.shape.type == ShapeType::Hull) {
        p.vertices.reserve(body.shape.hull.vertices.size());
        for (const Vec3& local : body.shape.hull.vertices) {
            Vec3 world = RotateLocalToWorld(local, body.rotation);
            p.vertices.push_back({ body.position.x + world.x, body.position.y + world.y, body.position.z + world.z });
        }
        for (const Vec3& n : body.shape.hull.faceNormals) p.faceNormals.push_back(RotateLocalToWorld(n, body.rotation));
        for (const Vec3& e : body.shape.hull.edgeDirs) p.edgeDirs.push_back(RotateLocalToWorld(e, body.rotation));
    }
    return p;
}

// Vertex farthest along `dir` in a polytope's own world-space vertex list
// -- the true support mapping for an arbitrary convex polytope (unlike
// SupportPoint above, which only works for a box's closed-form corners).
inline Vec3 PolytopeSupport(const Polytope& p, const Vec3& dir) {
    Vec3 best = p.vertices.empty() ? Vec3{0.0f, 0.0f, 0.0f} : p.vertices[0];
    float bestDot = -1e30f;
    for (const Vec3& v : p.vertices) {
        float d = Dot(v, dir);
        if (d > bestDot) { bestDot = d; best = v; }
    }
    return best;
}

// SAT across every face normal of both polytopes plus every pairwise
// cross product of their edge directions -- projecting ALL of each
// polytope's vertices onto each candidate axis (min/max) rather than a
// closed-form radius, since that's the only thing that works uniformly
// for a box's 8 corners AND an arbitrary mesh's N vertices. Same overall
// shape as BoxVsBox: track the axis with the smallest positive overlap
// as the true separating axis; bail out the moment any axis proves
// actual separation.
inline bool PolytopeVsPolytope(const Polytope& polyA, const Polytope& polyB, Vec3 centerA, Vec3 centerB, Contact& out) {
    if (polyA.vertices.empty() || polyB.vertices.empty()) return false;

    float bestOverlap = 1e30f;
    Vec3 bestAxis{0.0f, 1.0f, 0.0f};
    Vec3 t = Subtract(centerB, centerA);

    auto testAxis = [&](Vec3 axis) -> bool { // returns false if this axis proves separation
        float len2 = Dot(axis, axis);
        if (len2 < 0.000001f) return true; // degenerate (near-parallel edge cross) -- skip, another axis will catch it
        float invLen = 1.0f / std::sqrt(len2);
        axis = { axis.x * invLen, axis.y * invLen, axis.z * invLen };

        float minA = 1e30f, maxA = -1e30f;
        for (const Vec3& v : polyA.vertices) { float d = Dot(v, axis); minA = (std::min)(minA, d); maxA = (std::max)(maxA, d); }
        float minB = 1e30f, maxB = -1e30f;
        for (const Vec3& v : polyB.vertices) { float d = Dot(v, axis); minB = (std::min)(minB, d); maxB = (std::max)(maxB, d); }

        if (maxA < minB || maxB < minA) return false; // separated on this axis -- no collision, period
        float overlap = (std::min)(maxA, maxB) - (std::max)(minA, minB);
        if (overlap < bestOverlap) {
            bestOverlap = overlap;
            float sign = Dot(t, axis) < 0.0f ? -1.0f : 1.0f; // orient roughly A -> B
            bestAxis = { axis.x * sign, axis.y * sign, axis.z * sign };
        }
        return true;
    };

    for (const Vec3& n : polyA.faceNormals) if (!testAxis(n)) return false;
    for (const Vec3& n : polyB.faceNormals) if (!testAxis(n)) return false;
    for (const Vec3& ea : polyA.edgeDirs) {
        for (const Vec3& eb : polyB.edgeDirs) {
            if (!testAxis(Cross(ea, eb))) return false;
        }
    }

    out.normal = bestAxis;
    out.penetration = bestOverlap;
    Vec3 negNormal = { -bestAxis.x, -bestAxis.y, -bestAxis.z };
    Vec3 supportA = PolytopeSupport(polyA, bestAxis);
    Vec3 supportB = PolytopeSupport(polyB, negNormal);
    out.point = { (supportA.x + supportB.x) * 0.5f, (supportA.y + supportB.y) * 0.5f, (supportA.z + supportB.z) * 0.5f };
    return true;
}

inline bool HullVsBody(const Body& a, const Body& b, Contact& out) {
    Polytope polyA = BuildPolytope(a);
    Polytope polyB = BuildPolytope(b);
    return PolytopeVsPolytope(polyA, polyB, a.position, b.position, out);
}

// Sphere vs an arbitrary convex Hull. Exact when the sphere center ends
// up INSIDE the hull (pushes out along whichever face it's least deep
// past). When the center is OUTSIDE, this approximates using the
// closest HULL VERTEX rather than the true closest surface point (which
// could be on a face or edge) -- correct enough for a ball resting
// against a mesh, but can be slightly off very close to a face's
// interior far from any vertex. A full closest-point-on-convex-hull
// solve (GJK-style) would fix that; out of scope here.
inline bool SphereVsHull(const Vec3& spherePos, float sphereRadius, const Body& hullBody, Contact& out) {
    Vec3 localSphere = RotateWorldToLocal(Subtract(spherePos, hullBody.position), hullBody.rotation);
    const HullData& hull = hullBody.shape.hull;
    if (hull.vertices.empty() || hull.faceNormals.empty()) return false;

    // Inside test: for each face, check the sphere center against ANY
    // vertex on that face's plane. Faces don't store "a point on the
    // plane" directly, so approximate each face's plane point as the
    // hull's centroid-of-vertices projected appropriately -- simpler and
    // just as correct: use the vertex list's own support point for that
    // face normal (the vertex farthest along it) as a guaranteed point
    // on (or defining) that face's plane.
    bool inside = true;
    float leastPenetration = 1e30f;
    Vec3 pushNormal = hull.faceNormals[0];
    for (const Vec3& n : hull.faceNormals) {
        float maxDot = -1e30f;
        for (const Vec3& v : hull.vertices) maxDot = (std::max)(maxDot, Dot(v, n));
        float d = Dot(localSphere, n) - maxDot; // <=0 means on the inner side of this face
        if (d > 0.0f) { inside = false; break; }
        float penetration = -d; // how far inside this face's plane the center is
        if (penetration < leastPenetration) { leastPenetration = penetration; pushNormal = n; }
    }

    Vec3 localNormal;
    float penetration;
    if (inside) {
        localNormal = pushNormal;
        penetration = sphereRadius + leastPenetration;
    } else {
        Vec3 closestVertex = hull.vertices[0];
        float bestDist2 = 1e30f;
        for (const Vec3& v : hull.vertices) {
            Vec3 d = Subtract(localSphere, v);
            float dist2 = Dot(d, d);
            if (dist2 < bestDist2) { bestDist2 = dist2; closestVertex = v; }
        }
        float dist = std::sqrt(bestDist2);
        if (dist >= sphereRadius) return false;
        localNormal = (dist > 0.0001f) ? Vec3{ (localSphere.x - closestVertex.x) / dist, (localSphere.y - closestVertex.y) / dist, (localSphere.z - closestVertex.z) / dist } : hull.faceNormals[0];
        penetration = sphereRadius - dist;
    }

    Vec3 worldNormal = RotateLocalToWorld(localNormal, hullBody.rotation);
    float len = std::sqrt(Dot(worldNormal, worldNormal));
    if (len > 0.0001f) { worldNormal.x /= len; worldNormal.y /= len; worldNormal.z /= len; }
    out.normal = worldNormal;
    out.penetration = penetration;
    out.point = { spherePos.x - worldNormal.x * sphereRadius, spherePos.y - worldNormal.y * sphereRadius, spherePos.z - worldNormal.z * sphereRadius };
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
    if (a.shape.type == ShapeType::Hull && b.shape.type == ShapeType::Hull) {
        return HullVsBody(a, b, out);
    }
    if (a.shape.type == ShapeType::Hull && b.shape.type == ShapeType::Box) {
        return HullVsBody(a, b, out);
    }
    if (a.shape.type == ShapeType::Box && b.shape.type == ShapeType::Hull) {
        return HullVsBody(a, b, out);
    }
    if (a.shape.type == ShapeType::Hull && b.shape.type == ShapeType::Sphere) {
        // SphereVsHull naturally returns a hull->sphere normal, i.e. exactly A->B here.
        return SphereVsHull(b.position, b.shape.radius, a, out);
    }
    if (a.shape.type == ShapeType::Sphere && b.shape.type == ShapeType::Hull) {
        // a is the sphere, b is the hull: SphereVsHull gives hull->sphere (B->A) -- flip it.
        if (!SphereVsHull(a.position, a.shape.radius, b, out)) return false;
        out.normal = { -out.normal.x, -out.normal.y, -out.normal.z };
        return true;
    }
    if (a.shape.type == ShapeType::Box && b.shape.type == ShapeType::Sphere) {
        // SphereVsBox naturally returns a box->sphere normal, i.e. exactly A->B here.
        return SphereVsBox(b.position, b.shape.radius, a.position, a.rotation, a.shape.halfExtents, out);
    }
    // a is the sphere, b is the box: SphereVsBox gives box->sphere (B->A) -- flip it.
    if (!SphereVsBox(a.position, a.shape.radius, b.position, b.rotation, b.shape.halfExtents, out)) return false;
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

    // --- Touch tracking (for gameplay Collided/IsColliding queries) ---
    // touchingPairs reflects "who's overlapping whom" as of the most
    // recently completed Step() -- that's what IsBodyColliding reads.
    // newCollisions is the subset of those pairs that WEREN'T touching
    // before this Step (i.e. just started) -- the native binding drains
    // this once per Step() to fire each involved GameObject's Collided
    // event exactly once per new touch, not every frame while it lasts.
    std::unordered_set<uint64_t> touchingPairs;
    std::vector<std::pair<int, int>> newCollisions;
    std::unordered_set<uint64_t> _touchedThisStep; // scratch, rebuilt every Step()

    bool IsBodyColliding(int id) const {
        for (uint64_t key : touchingPairs) {
            if ((int)(key >> 32) == id || (int)(key & 0xFFFFFFFFu) == id) return true;
        }
        return false;
    }

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

        _touchedThisStep.clear();
        newCollisions.clear();

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

        // Diff this Step's touches against last Step's baseline to find
        // ones that just started, then adopt this Step's set as the new
        // baseline for both the next diff and IsBodyColliding.
        for (uint64_t key : _touchedThisStep) {
            if (touchingPairs.find(key) == touchingPairs.end()) {
                newCollisions.push_back({ (int)(key & 0xFFFFFFFFu), (int)(key >> 32) });
            }
        }
        touchingPairs = _touchedThisStep;

        // Runs once per Step (using the full dt, after every substep has
        // already moved each body's parent as far as it's going this
        // Step) -- see WeldTo/Body::weldParentId for what this is doing
        // and why.
        ApplyWelds(dt);
    }

    // Snaps every welded (kinematic) body onto its parent's CURRENT
    // position/rotation, offset by the relative transform captured at
    // weld time -- the actual mechanism that makes WeldTo feel like a
    // rigid bolt instead of just "immovable". velocity/angularVelocity
    // are back-derived via finite difference so anything else that
    // touches this body mid-ride (a third, non-welded body sliding
    // across it) still gets a physically sensible relative velocity for
    // friction/restitution, instead of reading a stale zero.
    //
    // Chained welds (child welded to a body that's itself welded to a
    // third) lag by one Step for the outermost link, since bodies here
    // are visited in arbitrary map order rather than parent-before-child
    // -- weld everything directly to one root body if that matters.
    void ApplyWelds(float dt) {
        if (dt <= 0.0f) return;
        for (auto& pair : bodies) {
            Body& child = pair.second;
            if (child.weldParentId < 0) continue;
            Body* parent = GetBody(child.weldParentId);
            if (!parent) { child.weldParentId = -1; continue; } // parent was destroyed -- release the weld instead of leaving it dangling

            Vec3 worldOffset = RotateLocalToWorld(child.weldLocalOffset, parent->rotation);
            Vec3 newPos = { parent->position.x + worldOffset.x, parent->position.y + worldOffset.y, parent->position.z + worldOffset.z };
            Vec3 newRot = {
                parent->rotation.x + child.weldLocalRotationOffset.x,
                parent->rotation.y + child.weldLocalRotationOffset.y,
                parent->rotation.z + child.weldLocalRotationOffset.z
            };

            child.velocity = { (newPos.x - child.position.x) / dt, (newPos.y - child.position.y) / dt, (newPos.z - child.position.z) / dt };
            child.angularVelocity = { (newRot.x - child.rotation.x) / dt, (newRot.y - child.rotation.y) / dt, (newRot.z - child.rotation.z) / dt };
            child.position = newPos;
            child.rotation = newRot;
        }
    }

    void Integrate(float dt) {
        for (auto& pair : bodies) {
            Body& b = pair.second;
            if (!b.isEnabled || b.isStatic || b.weldParentId >= 0) continue; // weldParentId >= 0 is kinematic -- ApplyWelds drives it, not forces/gravity

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

            // Belt-and-suspenders alongside ComputeInverseInertia's
            // rotationLocked check above: even if angularVelocity is
            // somehow nonzero (e.g. it was spinning right when locked
            // happened this same substep), rotation itself never moves.
            if (!b.rotationLocked) {
                b.rotation.x += b.angularVelocity.x * dt;
                b.rotation.y += b.angularVelocity.y * dt;
                b.rotation.z += b.angularVelocity.z * dt;
            }
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
                if (!ShouldCollide(*a, *b, ids[i], ids[j])) continue;

                Contact c;
                if (!ComputeContact(*a, *b, c)) continue;

                _touchedThisStep.insert(PairKey(ids[i], ids[j]));

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
