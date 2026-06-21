#pragma once
#include "../engine/RackModule.h"
#include "../dsp/BiquadFilter.h"
#include "../dsp/EnvelopeFollower.h"
#include <atomic>
#include <array>

// Four-band Linkwitz-Riley 4th-order crossover compressor.
//
// Split tree (3 crossover stages):
//   Stage 0 at xo1: LP → Band1,  HP → s1
//   Stage 1 at xo2: LP → Band2,  HP → s2   (applied to s1)
//   Stage 2 at xo3: LP → Band3,  HP → Band4 (applied to s2)
//
// LR4 = two cascaded 2nd-order Butterworth biquads (Q = 0.7071).
// Each band has an independent compressor; bands sum to the output.
// No all-pass correction — crossover summing has slight comb coloration
// at the frequencies where adjacent bands have very different GR, which
// is expected and typical for this compressor topology.

class MultibandCompressorModule : public RackModule
{
  public:
    static constexpr int kNumBands = 4;

    struct Band
    {
        std::atomic<bool>  enabled{true};
        std::atomic<float> thresholdDb{-20.f};
        std::atomic<float> ratio{4.f};
        std::atomic<float> attackMs{10.f};
        std::atomic<float> releaseMs{100.f};
        std::atomic<float> makeupDb{0.f};
        std::atomic<float> currentGR{0.f};

        Band () = default;
        Band (const Band&) = delete;
        Band& operator= (const Band&) = delete;
    };

    std::array<Band, kNumBands> bands;

    std::atomic<float> xo1{250.f};    // Hz — low / lo-mid crossover
    std::atomic<float> xo2{1000.f};   // Hz — lo-mid / hi-mid crossover
    std::atomic<float> xo3{4000.f};   // Hz — hi-mid / high crossover
    std::atomic<float> outputDb{0.f}; // global makeup

    MultibandCompressorModule ();

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset   () override;
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;
    int  latencySamples () const override { return 0; }

    ModuleType   type () const override { return ModuleType::MultibandComp; }
    juce::String name () const override { return "Multiband Comp"; }
    void getState (juce::ValueTree& tree) const override;
    void setState (const juce::ValueTree& tree) override;

  private:
    // Two cascaded Butterworth biquads = LR4 per channel (stereo)
    struct LR4
    {
        std::array<std::array<BiquadFilter, 2>, 2> f; // f[ch][stage]

        void setLP (float sr, float fc) noexcept
        {
            const auto c = BiquadFilter::makeLowPass (sr, fc, 0.7071f);
            for (auto& ch : f) { ch[0].setCoeffs (c); ch[1].setCoeffs (c); }
        }
        void setHP (float sr, float fc) noexcept
        {
            const auto c = BiquadFilter::makeHighPass (sr, fc, 0.7071f);
            for (auto& ch : f) { ch[0].setCoeffs (c); ch[1].setCoeffs (c); }
        }
        float process (float x, int ch) noexcept
        {
            return f[(size_t)ch][1].process (f[(size_t)ch][0].process (x));
        }
        void reset () noexcept
        {
            for (auto& ch : f) for (auto& flt : ch) flt.reset ();
        }
    };

    // 3 crossover stages; each stage has an LP and HP filter pair
    struct Stage { LR4 lp, hp; };
    std::array<Stage, 3> stages;

    // Per-band compression
    std::array<EnvelopeFollower, kNumBands> envs;
    std::array<float, kNumBands> smoothGR{};

    // Scratch buffers for the 4 band signals
    juce::AudioBuffer<float> bandBufs[kNumBands];

    double sampleRate{44100.0};
    float  lastXo[3]{-1.f, -1.f, -1.f}; // detect crossover changes
};
