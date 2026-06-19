#pragma once
#include "../engine/RackModule.h"
#include <atomic>

class ReverbModule : public RackModule
{
  public:
    std::atomic<float> roomSize{0.5f};
    std::atomic<float> damping{0.5f};
    std::atomic<float> width{1.0f};
    std::atomic<float> preDelay{0.f};
    std::atomic<float> wetDryMix{0.3f};

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset () override;
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;
    int latencySamples () const override { return 0; }
    ModuleType type () const override { return ModuleType::Reverb; }
    juce::String name () const override { return "Reverb"; }
    void getState (juce::ValueTree& tree) const override;
    void setState (const juce::ValueTree& tree) override;

  private:
    juce::dsp::Reverb reverb;
    juce::AudioBuffer<float> wetBuf;
    // Pre-delay line
    std::vector<std::vector<float>> preDelayBuf;
    int pdWritePos{0};
    int pdSamples{0};
    double sampleRate{44100.0};
};
