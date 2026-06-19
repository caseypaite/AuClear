#pragma once
#include "../engine/RackModule.h"
#include <atomic>

class UtilityModule : public RackModule
{
  public:
    std::atomic<float> gainDb{0.f};
    std::atomic<float> pan{0.f}; // -1 = full left, +1 = full right
    std::atomic<bool> invertPhaseL{false};
    std::atomic<bool> invertPhaseR{false};
    std::atomic<bool> monoSum{false};

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset () override;
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;
    int latencySamples () const override { return 0; }
    ModuleType type () const override { return ModuleType::Utility; }
    juce::String name () const override { return "Utility"; }
    void getState (juce::ValueTree& tree) const override;
    void setState (const juce::ValueTree& tree) override;

  private:
    float smoothGainL{1.f}, smoothGainR{1.f};
    double sampleRate{44100.0};
};
