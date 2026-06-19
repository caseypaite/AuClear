#pragma once
#include "../engine/RackModule.h"
#include <atomic>
#include <vector>

class DelayModule : public RackModule
{
  public:
    std::atomic<float> timeMs{250.f};
    std::atomic<float> feedback{0.4f};
    std::atomic<float> mix{0.5f};
    std::atomic<bool> pingPong{false};
    std::atomic<float> lpCutoff{6000.f};

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset () override;
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;
    int latencySamples () const override { return 0; }
    ModuleType type () const override { return ModuleType::Delay; }
    juce::String name () const override { return "Delay"; }
    void getState (juce::ValueTree& tree) const override;
    void setState (const juce::ValueTree& tree) override;

  private:
    static constexpr float kMaxTimeMs = 2000.f;
    std::vector<std::vector<float>> delayBuf; // [ch][sample]
    int bufLen{0}, writePos{0};
    // Simple one-pole LP for feedback roll-off
    std::array<float, 2> lpZ{0.f, 0.f};
    double sampleRate{44100.0};
};
