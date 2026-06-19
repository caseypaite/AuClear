#pragma once
#include "PanelHelpers.h"
#include "../../modules/DelayModule.h"

class DelayPanel : public juce::Component
{
  public:
    explicit DelayPanel (DelayModule& m) : mod (m)
    {
        bindKnob (time, m.timeMs, 1.0, 2000.0, 1.0, " ms");
        bindKnob (feedback, m.feedback, 0.0, 0.99, 0.01, "");
        bindKnob (mix, m.mix, 0.0, 1.0, 0.01, "");
        bindKnob (lpCut, m.lpCutoff, 200.0, 20000.0, 10.0, " Hz");
        time.slider.setDoubleClickReturnValue (true, 250.0);
        feedback.slider.setDoubleClickReturnValue (true, 0.4);
        mix.slider.setDoubleClickReturnValue (true, 0.5);
        lpCut.slider.setDoubleClickReturnValue (true, 6000.0);

        for (auto* k : {&time, &feedback, &mix, &lpCut})
            k->addTo (*this);

        pingButton.setButtonText ("Ping-Pong");
        pingButton.setClickingTogglesState (true);
        pingButton.setToggleState (m.pingPong.load (), juce::dontSendNotification);
        pingButton.setColour (juce::TextButton::buttonColourId, panelCol ());
        pingButton.setColour (juce::TextButton::buttonOnColourId, accent ());
        pingButton.onClick = [&m, this] { m.pingPong.store (pingButton.getToggleState ()); };
        addAndMakeVisible (pingButton);
    }

    void resized () override
    {
        auto b = getLocalBounds ().reduced (16, 24);
        pingButton.setBounds (b.removeFromBottom (28).reduced (4, 0));
        b.removeFromBottom (8);
        const int kw = b.getWidth () / 4;
        time.layout (b.removeFromLeft (kw).reduced (4), "Time");
        feedback.layout (b.removeFromLeft (kw).reduced (4), "Feedback");
        mix.layout (b.removeFromLeft (kw).reduced (4), "Mix");
        lpCut.layout (b.reduced (4), "LP Cutoff");
    }

  private:
    DelayModule& mod;
    KnobGroup time, feedback, mix, lpCut;
    juce::TextButton pingButton;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayPanel)
};
