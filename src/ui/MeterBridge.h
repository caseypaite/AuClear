#pragma once
#include <JuceHeader.h>
#include "../engine/ProcessorRack.h"

/**
 * Bottom meter strip. Drains MeterBus FIFOs at 30 Hz and draws stereo L/R bars
 * for input and output with peak hold and colour banding (green → amber → red).
 */
class MeterBridge : public juce::Component, private juce::Timer
{
public:
    explicit MeterBridge (ProcessorRack& rack);
    ~MeterBridge() override;

    void paint   (juce::Graphics& g) override;
    void resized () override;

private:
    void timerCallback() override;

    void drawMeterBar (juce::Graphics& g, juce::Rectangle<float> bounds,
                       float peakDb, float holdDb) const;

    ProcessorRack& rack;

    MeterValues displayIn  {};
    MeterValues displayOut {};
    float inPeakHoldL  { -100.0f }, inPeakHoldR  { -100.0f };
    float outPeakHoldL { -100.0f }, outPeakHoldR { -100.0f };
    int   peakHoldTick  { 0 };

    static constexpr float kFloor = -60.0f;
    static constexpr float kCeil  =   6.0f;
    static constexpr int   kHz    =  30;
    static constexpr int   kHoldFrames = 90; // 3 s

    static constexpr juce::uint32 kBg      = 0xff16181d;
    static constexpr juce::uint32 kPanel   = 0xff1e2128;
    static constexpr juce::uint32 kGreen   = 0xff2eb872;
    static constexpr juce::uint32 kAmber   = 0xffd4a020;
    static constexpr juce::uint32 kRed     = 0xffe0402a;
    static constexpr juce::uint32 kHold    = 0xffe8eaed;
    static constexpr juce::uint32 kTextLo  = 0xff9aa0ab;
    static constexpr juce::uint32 kDivider = 0xff2a2e37;
};
