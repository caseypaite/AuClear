#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <array>
#include "../engine/RackModule.h"
#include "../dsp/BiquadFilter.h"

/**
 * Adaptive hum / mains-noise remover.
 *
 * Cascades up to kMaxHarmonics second-order IIR notch filters at the
 * fundamental (50 or 60 Hz) and its integer harmonics.  Filters are
 * rebuilt only when parameters change (dirty flag) to keep the audio
 * thread free of trigonometry.
 *
 * Parameters:
 *   fundamental — 50 (EU/UK) or 60 (US/JP) Hz
 *   depth       — notch depth in dB (10–60 dB)
 *   harmonics   — number of harmonics to notch (1–kMaxHarmonics)
 */
class HumRemoverModule : public RackModule
{
  public:
    HumRemoverModule ();
    ~HumRemoverModule () override = default;

    static constexpr int kMaxHarmonics = 8;

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset () override;
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

    ModuleType type () const override { return ModuleType::HumRemover; }
    juce::String name () const override { return "Hum Remover"; }
    void getState (juce::ValueTree& tree) const override;
    void setState (const juce::ValueTree& tree) override;

    // Parameters (atomic)
    std::atomic<float> fundamental{50.f}; // Hz
    std::atomic<float> depth{30.f};       // dB
    std::atomic<float> harmonics{4.f};    // count (stored as float for uniform binding)

  private:
    void rebuildFilters ();

    // Two channels (L/R), up to kMaxHarmonics notches each
    std::array<std::array<BiquadFilter, kMaxHarmonics>, 2> notchFilters;

    double sampleRate{44100.0};
    float lastFundamental{0.f};
    float lastDepth{0.f};
    int lastHarmonics{0};
    bool dirty{true};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HumRemoverModule)
};
