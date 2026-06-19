#pragma once
#include "PanelHelpers.h"
#include "../../modules/GainModule.h"

class GainPanel : public juce::Component
{
  public:
    explicit GainPanel (GainModule& m) : mod (m)
    {
        knob.addTo (*this);
        bindKnob (knob, m.gainDb, -60.0, 12.0, 0.1, " dB");
        knob.slider.setDoubleClickReturnValue (true, 0.0);
    }

    void resized () override
    {
        const int cx = getWidth () / 2, cy = getHeight () / 2;
        knob.layout (juce::Rectangle<int> (cx - 60, cy - 70, 120, 148), "Gain");
    }

  private:
    GainModule& mod;
    KnobGroup knob;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GainPanel)
};
