#pragma once
#include "PanelHelpers.h"
#include "../../modules/SaturatorModule.h"

class SaturatorPanel : public juce::Component
{
  public:
    explicit SaturatorPanel (SaturatorModule& m) : mod (m)
    {
        bindKnob (drive, m.drive, 1.0, 20.0, 0.1, "x");
        bindKnob (tone, m.tone, 0.0, 1.0, 0.01, "");
        bindKnob (mix, m.mix, 0.0, 1.0, 0.01, "");
        drive.slider.setDoubleClickReturnValue (true, 1.0);
        tone.slider.setDoubleClickReturnValue (true, 0.5);
        mix.slider.setDoubleClickReturnValue (true, 0.5);

        for (auto* k : {&drive, &tone, &mix})
            k->addTo (*this);

        typeLabel.setText ("Type", juce::dontSendNotification);
        typeLabel.setColour (juce::Label::textColourId, textLo ());
        typeLabel.setFont (juce::FontOptions (11.f));
        typeLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (typeLabel);

        for (auto [i, name] :
             std::array<std::pair<int, const char*>, 3>{{{0, "Soft"}, {1, "Tape"}, {2, "Tube"}}})
        {
            auto& b = typeButtons[static_cast<size_t> (i)];
            b.setButtonText (name);
            b.setColour (juce::TextButton::buttonColourId, panelCol ());
            b.setColour (juce::TextButton::buttonOnColourId, accent ());
            b.onClick = [this, i, &m]
            {
                m.satType.store (i);
                updateTypeButtons ();
            };
            addAndMakeVisible (b);
        }
        updateTypeButtons ();
    }

    void resized () override
    {
        auto b = getLocalBounds ().reduced (16, 24);
        auto typeRow = b.removeFromBottom (28);
        typeLabel.setBounds (typeRow.removeFromLeft (50));
        const int bw = typeRow.getWidth () / 3;
        for (auto& btn : typeButtons)
            btn.setBounds (typeRow.removeFromLeft (bw).reduced (2, 2));
        b.removeFromBottom (8);
        const int kw = b.getWidth () / 3;
        drive.layout (b.removeFromLeft (kw).reduced (4), "Drive");
        tone.layout (b.removeFromLeft (kw).reduced (4), "Tone");
        mix.layout (b.reduced (4), "Mix");
    }

  private:
    void updateTypeButtons ()
    {
        const int cur = mod.satType.load ();
        for (int i = 0; i < 3; ++i)
            typeButtons[static_cast<size_t> (i)].setColour (juce::TextButton::buttonColourId,
                                                            i == cur ? accent () : panelCol ());
    }

    SaturatorModule& mod;
    KnobGroup drive, tone, mix;
    juce::Label typeLabel;
    std::array<juce::TextButton, 3> typeButtons;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SaturatorPanel)
};
