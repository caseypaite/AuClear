#pragma once
#include "../engine/RackModule.h"
#include "../dsp/BiquadFilter.h"
#include <array>
#include <atomic>

/**
 * 8-band parametric EQ. All parameters are atomics written by the GUI and read
 * on the audio thread at the start of each block.
 */
class ParametricEQModule : public RackModule
{
  public:
    static constexpr int kNumBands = 8;

    enum class BandType : int
    {
        Off = 0,
        Bell,
        LowShelf,
        HighShelf,
        LowPass,
        HighPass,
        Notch
    };

    struct BandParams
    {
        std::atomic<int> type{0};
        std::atomic<float> freq{1000.f};
        std::atomic<float> gain{0.f};
        std::atomic<float> q{0.707f};
    };

    std::array<BandParams, kNumBands> bands;

    ParametricEQModule ();

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset () override;
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;
    int latencySamples () const override { return 0; }
    ModuleType type () const override { return ModuleType::ParametricEQ; }
    juce::String name () const override { return "Parametric EQ"; }
    void getState (juce::ValueTree& tree) const override;
    void setState (const juce::ValueTree& tree) override;

  private:
    void updateCoefficients ();

    // Two biquads per band per channel for 12 dB/oct HP/LP (cascade two 6 dB/oct)
    // For simplicity: one biquad per band, 12 dB/oct HP/LP uses single 2nd-order section
    std::array<std::array<BiquadFilter, kNumBands>, 2> filters; // [ch][band]
    double sampleRate{44100.0};
};
