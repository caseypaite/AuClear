#pragma once
#include "PanelHelpers.h"
#include "../../modules/ReverbModule.h"

class ReverbPanel : public juce::Component
{
  public:
    explicit ReverbPanel (ReverbModule& m) : mod (m)
    {
        bindKnob (roomSize, m.roomSize, 0.0, 1.0, 0.01, "");
        bindKnob (damping, m.damping, 0.0, 1.0, 0.01, "");
        bindKnob (width, m.width, 0.0, 1.0, 0.01, "");
        bindKnob (preDelay, m.preDelay, 0.0, 500.0, 1.0, " ms");
        bindKnob (wet, m.wetDryMix, 0.0, 1.0, 0.01, "");

        roomSize.slider.setDoubleClickReturnValue (true, 0.5);
        damping.slider.setDoubleClickReturnValue (true, 0.5);
        width.slider.setDoubleClickReturnValue (true, 1.0);
        preDelay.slider.setDoubleClickReturnValue (true, 0.0);
        wet.slider.setDoubleClickReturnValue (true, 0.3);

        for (auto* k : {&roomSize, &damping, &width, &preDelay, &wet})
            k->addTo (*this);
    }

    void resized () override
    {
        auto b = getLocalBounds ().reduced (16, 24);
        const int kw = b.getWidth () / 5;
        roomSize.layout (b.removeFromLeft (kw).reduced (4), "Room Size");
        damping.layout (b.removeFromLeft (kw).reduced (4), "Damping");
        width.layout (b.removeFromLeft (kw).reduced (4), "Width");
        preDelay.layout (b.removeFromLeft (kw).reduced (4), "Pre-Delay");
        wet.layout (b.reduced (4), "Wet/Dry");
    }

  private:
    ReverbModule& mod;
    KnobGroup roomSize, damping, width, preDelay, wet;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverbPanel)
};
