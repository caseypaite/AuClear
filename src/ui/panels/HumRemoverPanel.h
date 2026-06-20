#pragma once
#include "PanelHelpers.h"
#include "../../modules/HumRemoverModule.h"

/**
 * Control panel for HumRemoverModule.
 *
 * Layout:
 *   [50 Hz] [60 Hz]  ← fundamental selector buttons
 *   [Depth knob]  [Harmonics knob]
 */
class HumRemoverPanel : public juce::Component, public juce::Timer
{
  public:
    explicit HumRemoverPanel (HumRemoverModule& m) : mod (m)
    {
        bindKnob (depth, m.depth, 1.0, 60.0, 0.5, " dB");
        bindKnob (harmonics, m.harmonics, 1.0, (double)HumRemoverModule::kMaxHarmonics, 1.0, "");

        for (auto* k : {&depth, &harmonics})
        {
            addAndMakeVisible (k->slider);
            addAndMakeVisible (k->label);
            addAndMakeVisible (k->value);
        }

        btn50.setButtonText ("50 Hz");
        btn60.setButtonText ("60 Hz");
        btn50.setClickingTogglesState (true);
        btn60.setClickingTogglesState (true);
        btn50.setRadioGroupId (1);
        btn60.setRadioGroupId (1);

        const bool is50 = m.fundamental.load () < 55.f;
        btn50.setToggleState (is50, juce::dontSendNotification);
        btn60.setToggleState (!is50, juce::dontSendNotification);

        btn50.onClick = [this] { mod.fundamental.store (50.f); };
        btn60.onClick = [this] { mod.fundamental.store (60.f); };

        addAndMakeVisible (btn50);
        addAndMakeVisible (btn60);

        startTimerHz (4);
    }

    ~HumRemoverPanel () override { stopTimer (); }

    void resized () override
    {
        auto b = getLocalBounds ().reduced (16, 20);

        // Frequency buttons
        auto btnRow = b.removeFromTop (28);
        btn50.setBounds (btnRow.removeFromLeft (80).reduced (0, 2));
        btnRow.removeFromLeft (8);
        btn60.setBounds (btnRow.removeFromLeft (80).reduced (0, 2));
        b.removeFromTop (16);

        // Knobs
        const int kw = b.getWidth () / 2;
        depth.layout (b.removeFromLeft (kw).reduced (4), "Depth");
        harmonics.layout (b.reduced (4), "Harmonics");
    }

    void timerCallback () override
    {
        depth.updateValue (mod.depth.load (), " dB");
        harmonics.updateValue ((float)mod.harmonics.load ());

        const bool is50 = mod.fundamental.load () < 55.f;
        btn50.setToggleState (is50, juce::dontSendNotification);
        btn60.setToggleState (!is50, juce::dontSendNotification);
    }

  private:
    HumRemoverModule& mod;
    KnobGroup depth, harmonics;
    juce::TextButton btn50, btn60;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HumRemoverPanel)
};
