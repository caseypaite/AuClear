#pragma once
#include "PanelHelpers.h"
#include "../../modules/CompressorModule.h"
#include "../GRMeter.h"

class CompressorPanel : public juce::Component
{
  public:
    explicit CompressorPanel (CompressorModule& m) : mod (m), grMeter (m.currentGR, 40.f)
    {
        bindKnob (thresh, m.thresholdDb, -60.0, 0.0, 0.1, " dB");
        bindKnob (ratio_, m.ratio, 1.0, 20.0, 0.1, ":1");
        bindKnob (knee, m.kneeDb, 0.0, 24.0, 0.1, " dB");
        bindKnob (attack, m.attackMs, 0.1, 200.0, 0.1, " ms");
        bindKnob (release, m.releaseMs, 1.0, 5000.0, 1.0, " ms");
        bindKnob (makeup, m.makeupDb, -12.0, 24.0, 0.1, " dB");
        bindKnob (mixK, m.mix, 0.0, 1.0, 0.01, "");

        for (auto* k : {&thresh, &ratio_, &knee, &attack, &release, &makeup, &mixK})
            k->addTo (*this);
        addAndMakeVisible (grMeter);

        charLabel.setText ("Character", juce::dontSendNotification);
        charLabel.setColour (juce::Label::textColourId, textLo ());
        charLabel.setFont (juce::FontOptions (11.f));
        charLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (charLabel);

        for (auto [i, name] : std::array<std::pair<int, const char*>, 4>{
                 {{0, "Clean"}, {1, "VCA"}, {2, "FET"}, {3, "Opto"}}})
        {
            auto& b = charButtons[static_cast<size_t> (i)];
            b.setButtonText (name);
            b.setClickingTogglesState (false);
            b.setColour (juce::TextButton::buttonColourId, panelCol ());
            b.setColour (juce::TextButton::buttonOnColourId, accent ());
            b.onClick = [this, i, &m]
            {
                m.character.store (i);
                updateCharButtons ();
            };
            addAndMakeVisible (b);
        }
        updateCharButtons ();
    }

    void resized () override
    {
        auto b = getLocalBounds ().reduced (12, 16);

        // GR meter on right
        grMeter.setBounds (b.removeFromRight (20).reduced (0, 4));
        b.removeFromRight (8);

        // Character buttons row at bottom
        auto charRow = b.removeFromBottom (28);
        charLabel.setBounds (charRow.removeFromLeft (70));
        const int bw = charRow.getWidth () / 4;
        for (auto& btn : charButtons)
            btn.setBounds (charRow.removeFromLeft (bw).reduced (2, 2));

        // Knob row
        const int kw = b.getWidth () / 7;
        thresh.layout (b.removeFromLeft (kw).reduced (4), "Thresh");
        ratio_.layout (b.removeFromLeft (kw).reduced (4), "Ratio");
        knee.layout (b.removeFromLeft (kw).reduced (4), "Knee");
        attack.layout (b.removeFromLeft (kw).reduced (4), "Attack");
        release.layout (b.removeFromLeft (kw).reduced (4), "Release");
        makeup.layout (b.removeFromLeft (kw).reduced (4), "Makeup");
        mixK.layout (b.reduced (4), "Mix");
    }

  private:
    void updateCharButtons ()
    {
        const int cur = mod.character.load ();
        for (int i = 0; i < 4; ++i)
            charButtons[static_cast<size_t> (i)].setColour (juce::TextButton::buttonColourId,
                                                            i == cur ? accent () : panelCol ());
    }

    CompressorModule& mod;
    KnobGroup thresh, ratio_, knee, attack, release, makeup, mixK;
    GRMeter grMeter;
    juce::Label charLabel;
    std::array<juce::TextButton, 4> charButtons;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompressorPanel)
};
