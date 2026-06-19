#pragma once
#include "../engine/RackModule.h"
#include <atomic>
#include <vector>

/**
 * Brickwall lookahead limiter.
 * Lookahead delay: 2 ms at current sample rate.
 * Uses a circular buffer + smoothed gain to prevent peaks above ceiling.
 */
class LimiterModule : public RackModule
{
  public:
    std::atomic<float> ceilingDb{-0.3f};
    std::atomic<float> releaseMs{50.f};
    std::atomic<float> lookaheadMs{2.f};
    std::atomic<float> currentGR{0.f};

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset () override;
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;
    int latencySamples () const override { return latSamples; }
    ModuleType type () const override { return ModuleType::Limiter; }
    juce::String name () const override { return "Limiter"; }
    void getState (juce::ValueTree& tree) const override;
    void setState (const juce::ValueTree& tree) override;

  private:
    int latSamples{0};
    int lookaheadBuf{0};
    std::vector<std::vector<float>> delayLines; // [channel][sample]
    std::vector<float> gainLine;
    int writePos{0};
    float smoothGain{1.f};
    float releaseCoeff{0.f};
    double sampleRate{44100.0};
    int numChannels{2};
};
