#pragma once
#include <JuceHeader.h>
#include "../engine/RackModule.h"

/**
 * Center panel: shows the selected module's full control panel.
 * Phase 1: only knows about GainModule; extended in Phase 2+ per module type.
 */
class MainStage : public juce::Component
{
public:
    MainStage();

    void paint   (juce::Graphics& g) override;
    void resized () override;

    void showModule (RackModule* module);

private:
    void showGainPanel();
    void showEmptyPanel();

    RackModule* currentModule { nullptr };

    // Gain panel controls
    juce::Slider gainSlider;
    juce::Label  gainLabel;
    juce::Label  gainValueLabel;

    static constexpr juce::uint32 kBg      = 0xff16181d;
    static constexpr juce::uint32 kPanel   = 0xff1e2128;
    static constexpr juce::uint32 kAccent  = 0xff28e0c8;
    static constexpr juce::uint32 kTextHi  = 0xffe8eaed;
    static constexpr juce::uint32 kTextLo  = 0xff9aa0ab;
    static constexpr juce::uint32 kDivider = 0xff2a2e37;
};
