// =========================================================================
// simd_sample.hpp
// =========================================================================
// Samples a texture at 8 (u,v) positions at once with AVX2, producing the
// SAME 32-bit results as Graphics::SampleTexture for each lane (the maths is
// the same integer arithmetic, only done 8 pixels per instruction).
//
// Only compiled where the compiler can emit AVX2 (a function-level target
// attribute), and only CALLED after a run-time CPU check, so one binary runs
// on any x86-64 machine. Everywhere else the scalar SampleTexture is used.
// =========================================================================
#ifndef SIMD_SAMPLE_HPP
#define SIMD_SAMPLE_HPP

#include <cstdint>
#include "texture_store.hpp"

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  #define SIMD_SAMPLE_AVX2 1
  #include <immintrin.h>
#endif

namespace SimdSample {

// True if this CPU can run the AVX2 path. Checked once at run time.
inline bool HasAvx2() {
#if defined(SIMD_SAMPLE_AVX2)
    static const bool ok = __builtin_cpu_supports("avx2");
    return ok;
#else
    return false;
#endif
}

#if defined(SIMD_SAMPLE_AVX2)

#define SS_AVX2 __attribute__((target("avx2"))) static inline

SS_AVX2 __m256 WrapCoord(__m256 t, Graphics::TextureWrap mode) {
    switch (mode) {
        case Graphics::TextureWrap::Repeat: return _mm256_sub_ps(t, _mm256_floor_ps(t));
        case Graphics::TextureWrap::Mirror: {
            const __m256 h = _mm256_mul_ps(t, _mm256_set1_ps(0.5f));
            const __m256 f = _mm256_sub_ps(t, _mm256_mul_ps(_mm256_set1_ps(2.0f), _mm256_floor_ps(h)));      // t mod 2
            const __m256 m = _mm256_cmp_ps(f, _mm256_set1_ps(1.0f), _CMP_LE_OQ);
            return _mm256_blendv_ps(_mm256_sub_ps(_mm256_set1_ps(2.0f), f), f, m);
        }
        default: return _mm256_min_ps(_mm256_max_ps(t, _mm256_setzero_ps()), _mm256_set1_ps(1.0f));
    }
}

SS_AVX2 __m256i ClampI(__m256i x, __m256i hi) { return _mm256_min_epi32(_mm256_max_epi32(x, _mm256_setzero_si256()), hi); }

// LerpColorFixed for 8 lanes at once: two channels per 32-bit multiply, exactly as the scalar version.
SS_AVX2 __m256i Lerp8(__m256i a, __m256i bb, __m256i t) {
    const __m256i m00FF = _mm256_set1_epi32(0x00FF00FF), r80 = _mm256_set1_epi32(0x00800080), mFF00 = _mm256_set1_epi32((int)0xFF00FF00u);
    const __m256i inv = _mm256_sub_epi32(_mm256_set1_epi32(256), t);
    const __m256i rb = _mm256_and_si256(_mm256_srli_epi32(_mm256_add_epi32(_mm256_add_epi32(_mm256_mullo_epi32(_mm256_and_si256(a, m00FF), inv), _mm256_mullo_epi32(_mm256_and_si256(bb, m00FF), t)), r80), 8), m00FF);
    const __m256i ag = _mm256_and_si256(_mm256_add_epi32(_mm256_add_epi32(_mm256_mullo_epi32(_mm256_and_si256(_mm256_srli_epi32(a, 8), m00FF), inv), _mm256_mullo_epi32(_mm256_and_si256(_mm256_srli_epi32(bb, 8), m00FF), t)), r80), mFF00);
    return _mm256_or_si256(rb, ag);
}

// u,v: 8 texture coordinates (already scaled/offset by the binding). out: 8 AARRGGBB texels.
__attribute__((target("avx2")))
inline void Sample8(const Graphics::ImageBuffer& img, const Graphics::TextureBinding& b, const float* uIn, const float* vIn, uint32_t* out) {
    const int W = img.width, H = img.height;
    const __m256 u = WrapCoord(_mm256_loadu_ps(uIn), b.wrap), v = WrapCoord(_mm256_loadu_ps(vIn), b.wrap);
    const __m256 fw = _mm256_set1_ps((float)W), fh = _mm256_set1_ps((float)H);
    const __m256i maxX = _mm256_set1_epi32(W - 1), maxY = _mm256_set1_epi32(H - 1), vW = _mm256_set1_epi32(W);
    const int* base = reinterpret_cast<const int*>(img.pixels.data());

    if (b.filter == Graphics::TextureFilter::Nearest) {
        // (int)(u*W): truncation toward zero, as the scalar code does
        const __m256i x = ClampI(_mm256_cvttps_epi32(_mm256_mul_ps(u, fw)), maxX), y = ClampI(_mm256_cvttps_epi32(_mm256_mul_ps(v, fh)), maxY);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(out), _mm256_i32gather_epi32(base, _mm256_add_epi32(_mm256_mullo_epi32(y, vW), x), 4));
        return;
    }

    // ---- bilinear ----
    const __m256 half = _mm256_set1_ps(0.5f), c256f = _mm256_set1_ps(256.0f);
    const __m256 fx = _mm256_sub_ps(_mm256_mul_ps(u, fw), half), fy = _mm256_sub_ps(_mm256_mul_ps(v, fh), half);
    const __m256 flx = _mm256_floor_ps(fx), fly = _mm256_floor_ps(fy);
    const __m256i x0 = _mm256_cvttps_epi32(flx), y0 = _mm256_cvttps_epi32(fly);
    const __m256i tx = _mm256_cvttps_epi32(_mm256_add_ps(_mm256_mul_ps(_mm256_sub_ps(fx, flx), c256f), half));
    const __m256i ty = _mm256_cvttps_epi32(_mm256_add_ps(_mm256_mul_ps(_mm256_sub_ps(fy, fly), c256f), half));
    const __m256i one = _mm256_set1_epi32(1);
    const __m256i xa = ClampI(x0, maxX), xb = ClampI(_mm256_add_epi32(x0, one), maxX);
    const __m256i rowA = _mm256_mullo_epi32(ClampI(y0, maxY), vW), rowB = _mm256_mullo_epi32(ClampI(_mm256_add_epi32(y0, one), maxY), vW);
    const __m256i c00 = _mm256_i32gather_epi32(base, _mm256_add_epi32(rowA, xa), 4);
    const __m256i c10 = _mm256_i32gather_epi32(base, _mm256_add_epi32(rowA, xb), 4);
    const __m256i c01 = _mm256_i32gather_epi32(base, _mm256_add_epi32(rowB, xa), 4);
    const __m256i c11 = _mm256_i32gather_epi32(base, _mm256_add_epi32(rowB, xb), 4);
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(out), Lerp8(Lerp8(c00, c10, tx), Lerp8(c01, c11, tx), ty));
}

#endif // SIMD_SAMPLE_AVX2

} // namespace SimdSample

#endif // SIMD_SAMPLE_HPP
