#pragma once
#include "../engine/RackModule.h"
#include <atomic>

class SaturatorModule : public RackModule
{
  public:
    enum class SatType : int
    {
        Soft = 0,
        Tape,
        Tube
    };

    std::atomic<float> drive{1.f};
    std::atomic<float> tone{0.5f};
    std::atomic<float> mix{0.5f};
    std::atomic<int> satType{0};

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset () override;
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;
    int latencySamples () const override { return 0; }
    ModuleType type () const override { return ModuleType::Saturator; }
    juce::String name () const override { return "Saturator"; }
    void getState (juce::ValueTree& tree) const override;
    void setState (const juce::ValueTree& tree) override;

  private:
    static float softClip (float x, float drv) noexcept;
    static float tapeClip (float x, float drv) noexcept;
    static float tubeClip (float x, float drv) noexcept;

    // Tone filter states (simple high-shelf -/+ boost)
    std::array<float, 2> toneZ{0.f, 0.f};
    juce::AudioBuffer<float> dryBuf;
    double sampleRate{44100.0};
};
