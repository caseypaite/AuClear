#pragma once
#include "../engine/RackModule.h"
#include "../dsp/EnvelopeFollower.h"
#include <atomic>
#include <array>

class GateModule : public RackModule
{
  public:
    std::atomic<float> thresholdDb{-40.f};
    std::atomic<float> rangeDb{-80.f};
    std::atomic<float> attackMs{1.f};
    std::atomic<float> holdMs{50.f};
    std::atomic<float> releaseMs{200.f};
    std::atomic<float> currentGR{0.f};

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset () override;
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;
    int latencySamples () const override { return 0; }
    ModuleType type () const override { return ModuleType::Gate; }
    juce::String name () const override { return "Gate"; }
    void getState (juce::ValueTree& tree) const override;
    void setState (const juce::ValueTree& tree) override;

  private:
    EnvelopeFollower env;
    float holdCounter{0.f};
    float smoothGain{1.f};
    float releaseCoeff{0.f};
    float attackCoeff{0.f};
    double sampleRate{44100.0};
    bool inHold{false};
};
