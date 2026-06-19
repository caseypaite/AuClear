#pragma once
#include "PanelHelpers.h"
#include "../../modules/LimiterModule.h"
#include "../GRMeter.h"

class LimiterPanel : public juce::Component, private juce::Timer
{
  public:
    explicit LimiterPanel (LimiterModule& m) : mod (m), grMeter (m.currentGR, 24.f)
    {
        bindKnob (ceiling, m.ceilingDb, -18.0, 0.0, 0.1, " dB");
        bindKnob (release_, m.releaseMs, 1.0, 2000.0, 1.0, " ms");
        bindKnob (lookahead, m.lookaheadMs, 0.1, 20.0, 0.1, " ms");
        ceiling.slider.setDoubleClickReturnValue (true, -0.3);
        release_.slider.setDoubleClickReturnValue (true, 50.0);
        lookahead.slider.setDoubleClickReturnValue (true, 2.0);

        ceiling.addTo (*this);
        release_.addTo (*this);
        lookahead.addTo (*this);
        addAndMakeVisible (grMeter);

        tpLabel.setColour (juce::Label::textColourId, textLo ());
        tpLabel.setFont (juce::FontOptions (11.f));
        tpLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (tpLabel);

        startTimerHz (10);
    }

    ~LimiterPanel () override { stopTimer (); }

    void resized () override
    {
        auto b = getLocalBounds ().reduced (12, 16);
        grMeter.setBounds (b.removeFromRight (20).reduced (0, 4));
        b.removeFromRight (8);
        tpLabel.setBounds (b.removeFromBottom (20));
        const int kw = b.getWidth () / 3;
        ceiling.layout (b.removeFromLeft (kw).reduced (4), "Ceiling");
        release_.layout (b.removeFromLeft (kw).reduced (4), "Release");
        lookahead.layout (b.reduced (4), "Lookahead");
    }

  private:
    void timerCallback () override
    {
        tpLabel.setText ("TP: " + juce::String (mod.currentGR.load (), 1) + " dB",
                         juce::dontSendNotification);
    }

    LimiterModule& mod;
    KnobGroup ceiling, release_, lookahead;
    GRMeter grMeter;
    juce::Label tpLabel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LimiterPanel)
};
