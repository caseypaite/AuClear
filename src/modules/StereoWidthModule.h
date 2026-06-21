#pragma once
#include "../engine/RackModule.h"
#include <atomic>

/**
 * M/S-based stereo width controller.
 *
 * width  0 % = mono (kill Side), 100 % = unchanged, 200 % = double width
 * monoBelow  = low-frequency mono cutoff (bass sticking to the centre)
 *              uses a 1-pole HP on the Side signal
 * pan        = overall stereo balance (-1 L … +1 R)
 */
class StereoWidthModule : public RackModule
{
  public:
    std::atomic<float> width{100.f};      // 0–200 %
    std::atomic<float> monoBelow{0.f};    // 0–400 Hz (0 = disabled)
    std::atomic<float> pan{0.f};          // -1..+1

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset () override;
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;
    int latencySamples () const override { return 0; }
    ModuleType type () const override { return ModuleType::StereoWidth; }
    juce::String name () const override { return "Stereo Width"; }
    void getState (juce::ValueTree& tree) const override;
    void setState (const juce::ValueTree& tree) override;

  private:
    double sampleRate{44100.0};

    // 1-pole HP state for the Side signal (bass mono)
    float hpZ{0.f};
    float hpCoef{0.f};   // 0 = no HP (full bass in Side)

    void updateHpCoef (float cutHz);
};
