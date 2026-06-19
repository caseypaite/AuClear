#pragma once
#include "PanelHelpers.h"
#include "../../modules/GateModule.h"
#include "../GRMeter.h"

class GatePanel : public juce::Component
{
  public:
    explicit GatePanel (GateModule& m) : mod (m), grMeter (m.currentGR, 40.f)
    {
        bindKnob (thresh, m.thresholdDb, -80.0, 0.0, 0.1, " dB");
        bindKnob (range, m.rangeDb, -96.0, 0.0, 0.1, " dB");
        bindKnob (attack, m.attackMs, 0.1, 200.0, 0.1, " ms");
        bindKnob (hold, m.holdMs, 0.0, 500.0, 1.0, " ms");
        bindKnob (release, m.releaseMs, 1.0, 3000.0, 1.0, " ms");
        thresh.slider.setDoubleClickReturnValue (true, -40.0);
        range.slider.setDoubleClickReturnValue (true, -80.0);
        attack.slider.setDoubleClickReturnValue (true, 1.0);
        hold.slider.setDoubleClickReturnValue (true, 50.0);
        release.slider.setDoubleClickReturnValue (true, 200.0);

        thresh.addTo (*this);
        range.addTo (*this);
        attack.addTo (*this);
        hold.addTo (*this);
        release.addTo (*this);
        addAndMakeVisible (grMeter);
    }

    void resized () override
    {
        auto b = getLocalBounds ().reduced (16, 24);
        grMeter.setBounds (b.removeFromRight (20).reduced (0, 4));
        b.removeFromRight (8);
        const int kw = b.getWidth () / 5;
        thresh.layout (b.removeFromLeft (kw).reduced (4), "Threshold");
        range.layout (b.removeFromLeft (kw).reduced (4), "Range");
        attack.layout (b.removeFromLeft (kw).reduced (4), "Attack");
        hold.layout (b.removeFromLeft (kw).reduced (4), "Hold");
        release.layout (b.reduced (4), "Release");
    }

  private:
    GateModule& mod;
    KnobGroup thresh, range, attack, hold, release;
    GRMeter grMeter;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GatePanel)
};
