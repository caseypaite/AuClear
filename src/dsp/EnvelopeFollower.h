#pragma once
#include <cmath>
#include <algorithm>

/**
 * Simple peak envelope follower with independent attack and release times.
 * Called once per sample on the audio thread.
 */
struct EnvelopeFollower
{
    float attackCoeff{0.f};
    float releaseCoeff{0.f};
    float env{0.f};

    void prepare (float sampleRate, float attackMs, float releaseMs) noexcept
    {
        attackCoeff = computeCoeff (sampleRate, attackMs);
        releaseCoeff = computeCoeff (sampleRate, releaseMs);
    }

    void setAttack (float sampleRate, float ms) noexcept
    {
        attackCoeff = computeCoeff (sampleRate, ms);
    }
    void setRelease (float sampleRate, float ms) noexcept
    {
        releaseCoeff = computeCoeff (sampleRate, ms);
    }

    float process (float x) noexcept
    {
        const float absX = std::abs (x);
        const float coeff = absX > env ? attackCoeff : releaseCoeff;
        env += coeff * (absX - env);
        return env;
    }

    float processRMS (float rms) noexcept
    {
        const float coeff = rms > env ? attackCoeff : releaseCoeff;
        env += coeff * (rms - env);
        return env;
    }

    void reset () noexcept { env = 0.f; }

  private:
    static float computeCoeff (float sr, float ms) noexcept
    {
        return (ms <= 0.f) ? 1.f : 1.f - std::exp (-1.f / (sr * ms * 0.001f));
    }
};
