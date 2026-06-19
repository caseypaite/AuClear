#pragma once
#include "PanelHelpers.h"
#include "../../modules/UtilityModule.h"

class UtilityPanel : public juce::Component
{
  public:
    explicit UtilityPanel (UtilityModule& m) : mod (m)
    {
        bindKnob (gain, m.gainDb, -40.0, 24.0, 0.1, " dB");
        bindKnob (pan, m.pan, -1.0, 1.0, 0.01, "");
        gain.slider.setDoubleClickReturnValue (true, 0.0);
        pan.slider.setDoubleClickReturnValue (true, 0.0);
        gain.addTo (*this);
        pan.addTo (*this);

        auto makeToggle = [this] (juce::TextButton& b, const juce::String& text)
        {
            b.setButtonText (text);
            b.setClickingTogglesState (true);
            b.setColour (juce::TextButton::buttonColourId, panelCol ());
            b.setColour (juce::TextButton::buttonOnColourId, accent ());
            addAndMakeVisible (b);
        };

        makeToggle (phaseL, "Φ L");
        phaseL.setToggleState (m.invertPhaseL.load (), juce::dontSendNotification);
        phaseL.onClick = [&m, this] { m.invertPhaseL.store (phaseL.getToggleState ()); };

        makeToggle (phaseR, "Φ R");
        phaseR.setToggleState (m.invertPhaseR.load (), juce::dontSendNotification);
        phaseR.onClick = [&m, this] { m.invertPhaseR.store (phaseR.getToggleState ()); };

        makeToggle (mono, "Mono");
        mono.setToggleState (m.monoSum.load (), juce::dontSendNotification);
        mono.onClick = [&m, this] { m.monoSum.store (mono.getToggleState ()); };
    }

    void resized () override
    {
        auto b = getLocalBounds ().reduced (16, 24);
        auto toggleRow = b.removeFromBottom (32);
        const int bw = toggleRow.getWidth () / 3;
        phaseL.setBounds (toggleRow.removeFromLeft (bw).reduced (4, 2));
        phaseR.setBounds (toggleRow.removeFromLeft (bw).reduced (4, 2));
        mono.setBounds (toggleRow.reduced (4, 2));
        b.removeFromBottom (8);
        const int kw = b.getWidth () / 2;
        gain.layout (b.removeFromLeft (kw).reduced (4), "Gain");
        pan.layout (b.reduced (4), "Pan");
    }

  private:
    UtilityModule& mod;
    KnobGroup gain, pan;
    juce::TextButton phaseL, phaseR, mono;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UtilityPanel)
};
