#pragma once
#include <cmath>

/**
 * Lightweight transposed direct-form II biquad. Coefficients and state are plain
 * floats — no heap allocation, safe to update on the audio thread.
 *
 * Coefficient factory methods implement the Audio EQ Cookbook (R. Bristow-Johnson).
 * All frequencies in Hz, sampleRate in Hz.
 */
struct BiquadFilter
{
    struct Coeffs
    {
        float b0{1.f}, b1{0.f}, b2{0.f}, a1{0.f}, a2{0.f};
    };

    Coeffs c;
    float z1{0.f}, z2{0.f};

    void setCoeffs (Coeffs co) noexcept { c = co; }

    float process (float x) noexcept
    {
        float y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }

    void reset () noexcept { z1 = z2 = 0.f; }

    // ── Coefficient factories ──────────────────────────────────────────────
    static Coeffs makeLowPass (float sr, float freq, float Q) noexcept
    {
        const float w = 2.f * pi * freq / sr;
        const float sinW = std::sin (w), cosW = std::cos (w);
        const float alpha = sinW / (2.f * Q);
        const float a0inv = 1.f / (1.f + alpha);
        return {(1.f - cosW) * 0.5f * a0inv, (1.f - cosW) * a0inv, (1.f - cosW) * 0.5f * a0inv,
                -2.f * cosW * a0inv, (1.f - alpha) * a0inv};
    }

    static Coeffs makeHighPass (float sr, float freq, float Q) noexcept
    {
        const float w = 2.f * pi * freq / sr;
        const float sinW = std::sin (w), cosW = std::cos (w);
        const float alpha = sinW / (2.f * Q);
        const float a0inv = 1.f / (1.f + alpha);
        return {(1.f + cosW) * 0.5f * a0inv, -(1.f + cosW) * a0inv, (1.f + cosW) * 0.5f * a0inv,
                -2.f * cosW * a0inv, (1.f - alpha) * a0inv};
    }

    static Coeffs makeBell (float sr, float freq, float Q, float gainDb) noexcept
    {
        const float A = std::pow (10.f, gainDb / 40.f);
        const float w = 2.f * pi * freq / sr;
        const float sinW = std::sin (w), cosW = std::cos (w);
        const float alpha = sinW / (2.f * Q);
        const float a0inv = 1.f / (1.f + alpha / A);
        return {(1.f + alpha * A) * a0inv, -2.f * cosW * a0inv, (1.f - alpha * A) * a0inv,
                -2.f * cosW * a0inv, (1.f - alpha / A) * a0inv};
    }

    static Coeffs makeLowShelf (float sr, float freq, float gainDb) noexcept
    {
        const float A = std::pow (10.f, gainDb / 40.f);
        const float w = 2.f * pi * freq / sr;
        const float sinW = std::sin (w), cosW = std::cos (w);
        const float sqrtA = std::sqrt (A);
        const float alpha = sinW / 2.f * std::sqrt ((A + 1.f / A) + 1.f);
        const float a0inv = 1.f / ((A + 1.f) + (A - 1.f) * cosW + 2.f * sqrtA * alpha);
        return {A * ((A + 1.f) - (A - 1.f) * cosW + 2.f * sqrtA * alpha) * a0inv,
                2.f * A * ((A - 1.f) - (A + 1.f) * cosW) * a0inv,
                A * ((A + 1.f) - (A - 1.f) * cosW - 2.f * sqrtA * alpha) * a0inv,
                -2.f * ((A - 1.f) + (A + 1.f) * cosW) * a0inv,
                ((A + 1.f) + (A - 1.f) * cosW - 2.f * sqrtA * alpha) * a0inv};
    }

    static Coeffs makeHighShelf (float sr, float freq, float gainDb) noexcept
    {
        const float A = std::pow (10.f, gainDb / 40.f);
        const float w = 2.f * pi * freq / sr;
        const float sinW = std::sin (w), cosW = std::cos (w);
        const float sqrtA = std::sqrt (A);
        const float alpha = sinW / 2.f * std::sqrt ((A + 1.f / A) + 1.f);
        const float a0inv = 1.f / ((A + 1.f) - (A - 1.f) * cosW + 2.f * sqrtA * alpha);
        return {A * ((A + 1.f) + (A - 1.f) * cosW + 2.f * sqrtA * alpha) * a0inv,
                -2.f * A * ((A - 1.f) + (A + 1.f) * cosW) * a0inv,
                A * ((A + 1.f) + (A - 1.f) * cosW - 2.f * sqrtA * alpha) * a0inv,
                2.f * ((A - 1.f) - (A + 1.f) * cosW) * a0inv,
                ((A + 1.f) - (A - 1.f) * cosW - 2.f * sqrtA * alpha) * a0inv};
    }

    static Coeffs makeNotch (float sr, float freq, float Q) noexcept
    {
        const float w = 2.f * pi * freq / sr;
        const float sinW = std::sin (w), cosW = std::cos (w);
        const float alpha = sinW / (2.f * Q);
        const float a0inv = 1.f / (1.f + alpha);
        return {a0inv, -2.f * cosW * a0inv, a0inv, -2.f * cosW * a0inv, (1.f - alpha) * a0inv};
    }

    // K-weighting pre-filter (ITU-R BS.1770-4 Stage 1 — high-shelf +4 dB ≈ 1500 Hz)
    static Coeffs makeKWeightingStage1 (float sr) noexcept
    {
        return makeHighShelf (sr, 1500.f, 4.f);
    }

    // K-weighting high-pass (ITU-R BS.1770-4 Stage 2 — 38 Hz, Q≈0.5)
    static Coeffs makeKWeightingStage2 (float sr) noexcept
    {
        return makeHighPass (sr, 38.135f, 0.5f);
    }

    // Magnitude response |H(f)| for display (call on GUI thread)
    float magnitudeAt (float freq, float sr) const noexcept
    {
        const float w = 2.f * pi * freq / sr;
        const float cosW = std::cos (w), cos2W = std::cos (2.f * w);
        const float sinW = std::sin (w), sin2W = std::sin (2.f * w);
        const float numR = c.b0 + c.b1 * cosW + c.b2 * cos2W;
        const float numI = -(c.b1 * sinW + c.b2 * sin2W);
        const float denR = 1.f + c.a1 * cosW + c.a2 * cos2W;
        const float denI = -(c.a1 * sinW + c.a2 * sin2W);
        const float numMag2 = numR * numR + numI * numI;
        const float denMag2 = denR * denR + denI * denI;
        return denMag2 > 0.f ? std::sqrt (numMag2 / denMag2) : 1.f;
    }

    static constexpr float pi = 3.14159265358979f;
};
