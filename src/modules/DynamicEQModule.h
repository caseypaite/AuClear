#pragma once
#include "../engine/RackModule.h"
#include "../dsp/BiquadFilter.h"
#include "../dsp/EnvelopeFollower.h"
#include <atomic>
#include <array>

// Four-band dynamic equalizer.
//
// Each band is a bell filter whose gain is the sum of a static gain and a
// dynamic gain-reduction component computed from a sidechain envelope follower:
//
//   totalGain = staticGain + dynamicGR
//
// where dynamicGR ∈ [0, -rangeDb] dB, determined by a compressor-style gain
// computer acting on the band's own energy (detected with a matched bell filter
// at the same freq/Q, boosted by +12 dB to extract the band).
//
// When rangeDb == 0 the band behaves as a static EQ band.
// When staticGain == 0 the band becomes a dynamic notch suppressor.
//
// Filter coefficients are recomputed once per block (not per-sample).
// Detection is summed-mono to keep the sidechain stable for stereo material.

class DynamicEQModule : public RackModule
{
  public:
    static constexpr int kNumBands = 4;

    struct Band
    {
        std::atomic<bool>  enabled{true};
        std::atomic<float> freq{500.f};       // Hz
        std::atomic<float> q{1.0f};           // bandwidth
        std::atomic<float> gainDb{0.f};       // static gain, dB
        std::atomic<float> threshDb{0.f};     // dynamic threshold, dB (0 = off when rangeDb==0)
        std::atomic<float> rangeDb{0.f};      // max dynamic reduction, dB (0 = static only)
        std::atomic<float> attackMs{10.f};
        std::atomic<float> releaseMs{200.f};
        std::atomic<float> currentGR{0.f};    // GR this block (0 = no reduction)

        Band () = default;
        Band (const Band&) = delete;
        Band& operator= (const Band&) = delete;
    };

    std::array<Band, kNumBands> bands;

    DynamicEQModule ();

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset   () override;
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;
    int  latencySamples () const override { return 0; }

    ModuleType   type () const override { return ModuleType::DynamicEQ; }
    juce::String name () const override { return "Dynamic EQ"; }
    void getState (juce::ValueTree& tree) const override;
    void setState (const juce::ValueTree& tree) override;

  private:
    struct BandDsp
    {
        // Detection: mono bell with +12 dB boost at band freq/Q
        BiquadFilter detect;
        EnvelopeFollower env;
        float smoothGR{0.f};   // in dB (0 = no reduction)

        // Processing: one filter per channel
        std::array<BiquadFilter, 2> proc;

        void reset () noexcept
        {
            detect.reset ();
            env.reset ();
            smoothGR = 0.f;
            for (auto& f : proc) f.reset ();
        }
    };

    std::array<BandDsp, kNumBands> dsp;
    double sampleRate{44100.0};
};
