// =========================================================================
// near_clip.hpp
// =========================================================================
// Near-plane clipping for the software 3D pipeline.
//
// THE BUG THIS FIXES
//   Project3DPoint() divides by w (which equals view-space z) and checks
//   nothing else. A vertex BEHIND the camera has z < 0, so the divide
//   mirrors it to the wrong side of the screen; one with z ~ 0 lands
//   thousands of pixels away. Any polygon touching such a vertex is filled
//   with a corner in the wrong place, which draws as huge wedges across the
//   screen. It only happens when geometry straddles the camera plane (a
//   camera moved low, or into/through a mesh), which is why a camera that
//   sees everything in front of it never shows it.
//
// THE FIX
//   Clip in VIEW space, before projecting, against the plane z = nearPlane.
//   Anything in front of the plane is kept; anything behind is cut off and
//   replaced by the points where the polygon's edges cross the plane. Only
//   the surviving vertices are projected. This is the standard
//   Sutherland-Hodgman step for a single plane, and it is exact for convex
//   polygons (all of this engine's faces are convex).
//
// WHAT IT HANDLES
//   ClipPolygonNear   a convex polygon with optional per-vertex UVs
//   ClipSegmentNear   a line segment (wireframe edges)
//   PointInFrontOfNear a single point (point mode)
//
// NO ALLOCATION IN THE HOT PATH
//   A CONVEX polygon clipped by one plane gains at most one vertex (input+1).
//   A non-convex or self-crossing polygon (a bad user mesh) can alternate in
//   and out of the plane; each "in" run then costs up to two extra points, so
//   the worst case is input + input/2. kMaxClipVertices is sized for that
//   worst case at the largest accepted input, so a bad face can produce an
//   ugly result but can never overflow the output buffer.
// =========================================================================
#ifndef NEAR_CLIP_HPP
#define NEAR_CLIP_HPP

#include <cstddef>
#include <cmath>

#ifndef TEXTURE_STORE_STANDALONE
#include "renderer.hpp"
#endif

namespace Graphics {

// One vertex while clipping: view-space position plus its UV.
struct ClipVertex {
    Vec3 pos;      // view space
    float u = 0.0f;
    float v = 0.0f;
};

// Largest polygon ClipPolygonNear accepts. Bigger faces must be fanned first.
constexpr size_t kMaxClipInput = 24;

// Output capacity: the worst case for ANY polygon (even a bad, non-convex one)
// is input + input/2, because each in/out alternation can add up to two points
// while consuming at least one. Rounded up with headroom.
constexpr size_t kMaxClipVertices = kMaxClipInput + kMaxClipInput / 2 + 4;

// Smallest depth a vertex may have and still be projected safely. The near
// plane itself is the natural bound, but it can be set to 0 or tiny by the
// caller, and dividing by ~0 is the original bug. Clamping to this floor makes
// the projection finite for ANY near plane value.
constexpr float kMinProjectableDepth = 1e-4f;

inline float EffectiveNear(float nearPlane) {
    if (!(nearPlane > kMinProjectableDepth)) return kMinProjectableDepth;  // also catches NaN
    return nearPlane;
}

inline bool PointInFrontOfNear(const Vec3& viewPos, float nearPlane) {
    return viewPos.z >= EffectiveNear(nearPlane);
}

// Linear interpolation between two clip vertices at parameter t.
inline ClipVertex LerpClipVertex(const ClipVertex& a, const ClipVertex& b, float t) {
    ClipVertex r;
    r.pos.x = a.pos.x + (b.pos.x - a.pos.x) * t;
    r.pos.y = a.pos.y + (b.pos.y - a.pos.y) * t;
    r.pos.z = a.pos.z + (b.pos.z - a.pos.z) * t;
    r.u = a.u + (b.u - a.u) * t;
    r.v = a.v + (b.v - a.v) * t;
    return r;
}

// Clips a convex polygon against z >= nearPlane.
//   in/inCount   the polygon, in order (either winding)
//   out          receives the clipped polygon; must hold kMaxClipVertices
// Returns the number of output vertices: 0 if everything is behind the plane
// (nothing to draw), inCount if nothing needed clipping, otherwise 3..inCount+1.
inline size_t ClipPolygonNear(const ClipVertex* in, size_t inCount, float nearPlane, ClipVertex* out) {
    if (inCount < 3 || inCount > kMaxClipInput) return 0;
    const float nz = EffectiveNear(nearPlane);

    size_t outCount = 0;
    for (size_t i = 0; i < inCount; ++i) {
        const ClipVertex& cur = in[i];
        const ClipVertex& nxt = in[(i + 1) % inCount];
        const bool curIn = cur.pos.z >= nz;
        const bool nxtIn = nxt.pos.z >= nz;

        if (curIn) out[outCount++] = cur;

        if (curIn != nxtIn) {
            // The edge crosses the plane: emit the crossing point. The
            // denominator cannot be zero because one end is in and one is out.
            const float t = (nz - cur.pos.z) / (nxt.pos.z - cur.pos.z);
            ClipVertex x = LerpClipVertex(cur, nxt, t);
            x.pos.z = nz;                       // land exactly on the plane, not near it
            out[outCount++] = x;
        }
    }
    // A degenerate sliver can clip down to fewer than 3 points: nothing to fill.
    return outCount >= 3 ? outCount : 0;
}

// Clips a segment against z >= nearPlane. Returns false if it is entirely
// behind the plane; otherwise a and b are replaced by the visible portion.
inline bool ClipSegmentNear(ClipVertex& a, ClipVertex& b, float nearPlane) {
    const float nz = EffectiveNear(nearPlane);
    const bool aIn = a.pos.z >= nz;
    const bool bIn = b.pos.z >= nz;
    if (!aIn && !bIn) return false;
    if (aIn && bIn) return true;

    const float t = (nz - a.pos.z) / (b.pos.z - a.pos.z);
    ClipVertex x = LerpClipVertex(a, b, t);
    x.pos.z = nz;
    if (aIn) b = x; else a = x;
    return true;
}

// Projects a CLIPPED view-space point to the screen. Same maths as
// Project3DPoint, but the caller guarantees z >= EffectiveNear, so w is
// strictly positive and the divide is always safe.
inline Vec2 ProjectClipped(const Vec3& viewPos, const Matrix4x4& matProj, int screenW, int screenH) {
    return Project3DPoint(viewPos, matProj, screenW, screenH);
}

// True if the screen position is finite and not absurdly far off screen.
// A last line of defence: even a correctly clipped vertex can land far
// outside the window (a very steep angle), and the scan-converter should not
// be handed values that overflow its integer math.
inline bool ScreenPointSane(const Vec2& p, int screenW, int screenH) {
    if (!std::isfinite(p.x) || !std::isfinite(p.y)) return false;
    const float lim = 1.0e6f;   // far beyond any window, well inside int range
    (void)screenW; (void)screenH;
    return std::fabs(p.x) < lim && std::fabs(p.y) < lim;
}

} // namespace Graphics

#endif // NEAR_CLIP_HPP
