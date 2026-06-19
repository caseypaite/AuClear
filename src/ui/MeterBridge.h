#pragma once
#include <JuceHeader.h>
#include "../engine/ProcessorRack.h"

/**
 * Bottom meter strip: stereo L/R peak meters for input and output with peak hold,
 * plus LUFS (M/S/I) and true-peak readout from the rack's LoudnessMeter.
 */
class MeterBridge : public juce::Component, private juce::Timer
{
  public:
    explicit MeterBridge (ProcessorRack& rack);
    ~MeterBridge () override;

    void paint (juce::Graphics& g) override;
    void resized () override;

  private:
    void timerCallback () override;

    void drawMeterBar (juce::Graphics& g, juce::Rectangle<float> bounds, float peakDb,
                       float holdDb) const;

    ProcessorRack& rack;

    MeterValues displayIn{};
    MeterValues displayOut{};
    float inPeakHoldL{-100.f}, inPeakHoldR{-100.f};
    float outPeakHoldL{-100.f}, outPeakHoldR{-100.f};
    int peakHoldTick{0};

    // LUFS display values (updated on GUI thread from atomics)
    float lufsM{-70.f}, lufsS{-70.f}, lufsI{-70.f}, tpDb{-100.f};

    static constexpr float kFloor = -60.f;
    static constexpr float kCeil = 6.f;
    static constexpr int kHz = 30;
    static constexpr int kHoldFrames = 90; // 3 s @ 30 Hz

    static constexpr juce::uint32 kBg = 0xff16181d;
    static constexpr juce::uint32 kPanel = 0xff1e2128;
    static constexpr juce::uint32 kGreen = 0xff2eb872;
    static constexpr juce::uint32 kAmber = 0xffd4a020;
    static constexpr juce::uint32 kRed = 0xffe0402a;
    static constexpr juce::uint32 kHold = 0xffe8eaed;
    static constexpr juce::uint32 kAccent = 0xff28e0c8;
    static constexpr juce::uint32 kTextLo = 0xff9aa0ab;
    static constexpr juce::uint32 kDivider = 0xff2a2e37;
};
