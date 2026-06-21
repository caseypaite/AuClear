#pragma once
#include "../engine/RackModule.h"
#include "../dsp/BiquadFilter.h"
#include "../dsp/EnvelopeFollower.h"
#include <atomic>
#include <array>

// Broadband sidechain de-esser.
//
// Detection chain:  input → HP filter at `freq` → peak envelope follower.
// Gain computer:    soft-knee compressor with infinite ratio, clamped to `range`.
// Apply:            GR multiplier on all channels when above threshold.
// Listen mode:      routes the HP-filtered sidechain to the output (for tuning `freq`).
//
// A separate HP filter state is kept per channel so the stereo sidechain is
// the per-channel absolute max — no mono folding artifacts.

class DeEsserModule : public RackModule
{
  public:
    std::atomic<float> freq{7500.f};        // Hz   — HP detection centre
    std::atomic<float> thresholdDb{-20.f};  // dB   — onset level
    std::atomic<float> rangeDb{12.f};       // dB   — maximum gain reduction
    std::atomic<float> attackMs{1.f};       // ms
    std::atomic<float> releaseMs{60.f};     // ms
    std::atomic<bool>  listen{false};       // monitor sidechain

    std::atomic<float> currentGR{0.f};      // most-negative GR this block (for meter)

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset   () override;
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;
    int  latencySamples () const override { return 0; }

    ModuleType   type () const override { return ModuleType::DeEsser; }
    juce::String name () const override { return "De-Esser"; }
    void getState (juce::ValueTree& tree) const override;
    void setState (const juce::ValueTree& tree) override;

  private:
    // Per-channel HP for sidechain detection
    std::array<BiquadFilter, 2> detectHp;
    EnvelopeFollower             env;
    double                       sampleRate{44100.0};
    float                        smoothGR{0.f};   // in dB (0 = no reduction)
};
