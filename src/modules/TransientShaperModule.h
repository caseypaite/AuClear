#pragma once
#include "../engine/RackModule.h"
#include <atomic>
#include <array>

/**
 * Two-band transient shaper using fast/slow envelope followers.
 * Boosts or cuts attack transients independently of sustain body.
 *
 * Algorithm (SPL Transient Designer-style):
 *   fast_env = fast_follower(abs(input))   // ~3 ms attack
 *   slow_env = slow_follower(abs(input))   // ~150 ms attack
 *   transient = fast_env - slow_env        // positive when hit detected
 *   gain_db = attackSens * transient_norm + sustainSens * slow_norm
 */
class TransientShaperModule : public RackModule
{
  public:
    std::atomic<float> attackSens{0.f};  // -12..+12 dB transient sensitivity
    std::atomic<float> sustainSens{0.f}; // -12..+12 dB sustain sensitivity
    std::atomic<float> outputGain{0.f};  // -12..+12 dB output trim

    // Read from the GUI thread to draw a gain-ride indicator
    std::atomic<float> gainRide{0.f};    // smoothed gain in dB (signed)

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset () override;
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;
    int latencySamples () const override { return 0; }
    ModuleType type () const override { return ModuleType::TransientShaper; }
    juce::String name () const override { return "Transient Shaper"; }
    void getState (juce::ValueTree& tree) const override;
    void setState (const juce::ValueTree& tree) override;

  private:
    double sampleRate{44100.0};

    // Per-channel fast and slow envelope states
    std::array<float, 2> fastEnv{0.f, 0.f};
    std::array<float, 2> slowEnv{0.f, 0.f};

    // Coefficients (recalculated in prepare)
    float fastAttCoef{0.f}, fastRelCoef{0.f};
    float slowAttCoef{0.f}, slowRelCoef{0.f};

    // Smoothed display gain for the GUI meter
    float displayGainDb{0.f};

    static float dbToLinear (float db) noexcept
    {
        return std::pow (10.f, db / 20.f);
    }

    static float linearToDb (float lin) noexcept
    {
        return lin > 1e-6f ? 20.f * std::log10 (lin) : -120.f;
    }

    static float envCoef (float timeMs, double sr) noexcept
    {
        if (timeMs <= 0.f || sr <= 0.0) return 0.f;
        return std::exp (-1.f / (static_cast<float> (sr) * timeMs * 0.001f));
    }
};
