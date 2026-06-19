#pragma once
#include "../engine/RackModule.h"
#include "../dsp/EnvelopeFollower.h"
#include <atomic>

class CompressorModule : public RackModule
{
  public:
    enum class Character : int
    {
        Clean = 0,
        VCA,
        FET,
        Opto
    };

    std::atomic<float> thresholdDb{-20.f};
    std::atomic<float> ratio{4.f};
    std::atomic<float> kneeDb{6.f};
    std::atomic<float> attackMs{10.f};
    std::atomic<float> releaseMs{100.f};
    std::atomic<float> makeupDb{0.f};
    std::atomic<float> mix{1.f};
    std::atomic<int> character{0};
    std::atomic<float> currentGR{0.f};

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset () override;
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;
    int latencySamples () const override { return 0; }
    ModuleType type () const override { return ModuleType::Compressor; }
    juce::String name () const override { return "Compressor"; }
    void getState (juce::ValueTree& tree) const override;
    void setState (const juce::ValueTree& tree) override;

  private:
    EnvelopeFollower env;
    float smoothGR{0.f};
    double sampleRate{44100.0};
    juce::AudioBuffer<float> dryBuf;
};
