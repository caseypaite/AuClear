#pragma once
#include "../engine/RackModule.h"

class GainModule : public RackModule
{
public:
    GainModule();

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    ModuleType   type() const override { return ModuleType::Gain; }
    juce::String name() const override { return "Gain"; }
    void getState (juce::ValueTree& tree) const override;
    void setState (const juce::ValueTree& tree) override;

    // GUI writes, audio thread reads — atomic float in dB.
    std::atomic<float> gainDb { 0.0f };

private:
    juce::dsp::Gain<float> gain;
};
