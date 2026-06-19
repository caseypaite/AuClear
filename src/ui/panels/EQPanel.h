#pragma once
#include "PanelHelpers.h"
#include "../../modules/ParametricEQModule.h"
#include "../EQCurveDisplay.h"

class EQPanel : public juce::Component
{
  public:
    explicit EQPanel (ParametricEQModule& m) : mod (m), curveDisplay (m)
    {
        addAndMakeVisible (curveDisplay);

        static const char* bandTypeNames[] = {"Off", "Bell", "L.Shelf", "H.Shelf",
                                              "LP",  "HP",   "Notch"};
        static constexpr int kNumTypes = 7;

        for (int b = 0; b < ParametricEQModule::kNumBands; ++b)
        {
            auto& bs = bandStrips[static_cast<size_t> (b)];
            const auto idx = static_cast<size_t> (b);

            // Enable button
            bs.enableBtn.setButtonText ("B" + juce::String (b + 1));
            bs.enableBtn.setClickingTogglesState (true);
            const auto bt = static_cast<ParametricEQModule::BandType> (m.bands[idx].type.load ());
            bs.enableBtn.setToggleState (bt != ParametricEQModule::BandType::Off,
                                         juce::dontSendNotification);
            bs.enableBtn.setColour (juce::TextButton::buttonColourId, panelCol ());
            bs.enableBtn.setColour (juce::TextButton::buttonOnColourId, accent ());
            bs.enableBtn.onClick = [this, b, &m, &bs]
            {
                const auto idx2 = static_cast<size_t> (b);
                const bool on = bs.enableBtn.getToggleState ();
                if (!on)
                {
                    m.bands[idx2].type.store ((int)ParametricEQModule::BandType::Off);
                }
                else
                {
                    // Restore to previously selected type (combo)
                    const int typeIdx = bs.typeCombo.getSelectedItemIndex ();
                    m.bands[idx2].type.store (typeIdx + 1); // +1 because Off=0
                }
                updateBandEnabled (b);
            };
            addAndMakeVisible (bs.enableBtn);

            // Type combo (excluding Off)
            for (int t = 1; t < kNumTypes; ++t)
                bs.typeCombo.addItem (bandTypeNames[t], t);
            const int savedType = juce::jmax (1, (int)m.bands[idx].type.load ());
            bs.typeCombo.setSelectedId (savedType, juce::dontSendNotification);
            bs.typeCombo.onChange = [this, b, &m, &bs]
            {
                const auto idx2 = static_cast<size_t> (b);
                if (bs.enableBtn.getToggleState ())
                    m.bands[idx2].type.store (bs.typeCombo.getSelectedId ());
            };
            addAndMakeVisible (bs.typeCombo);

            // Freq knob
            bindKnob (bs.freq, m.bands[idx].freq, 20.0, 20000.0, 1.0, " Hz");
            bs.freq.slider.setSkewFactorFromMidPoint (1000.0);
            bs.freq.addTo (*this);

            // Gain knob (N/A for LP/HP/Notch — still shown but grayed)
            bindKnob (bs.gain, m.bands[idx].gain, -24.0, 24.0, 0.1, " dB");
            bs.gain.slider.setDoubleClickReturnValue (true, 0.0);
            bs.gain.addTo (*this);

            // Q knob
            bindKnob (bs.q, m.bands[idx].q, 0.1, 18.0, 0.01, "");
            bs.q.slider.setSkewFactorFromMidPoint (1.0);
            bs.q.slider.setDoubleClickReturnValue (true, 0.707);
            bs.q.addTo (*this);

            updateBandEnabled (b);
        }
    }

    void resized () override
    {
        auto b = getLocalBounds ();
        const int curveH = juce::jmin (b.getHeight () / 2, 200);
        curveDisplay.setBounds (b.removeFromTop (curveH).reduced (4));

        // Band strips in a horizontal scrollable row
        const int bandW = juce::jmax (70, b.getWidth () / ParametricEQModule::kNumBands);
        int x = b.getX ();
        for (int i = 0; i < ParametricEQModule::kNumBands; ++i)
        {
            auto& bs = bandStrips[static_cast<size_t> (i)];
            juce::Rectangle<int> col (x, b.getY (), bandW, b.getHeight ());
            x += bandW;

            bs.enableBtn.setBounds (col.removeFromTop (22).reduced (2, 2));
            bs.typeCombo.setBounds (col.removeFromTop (22).reduced (2, 2));
            const int kh = (col.getHeight () - 4) / 3;
            bs.freq.layout (col.removeFromTop (kh).reduced (2), "Freq");
            bs.gain.layout (col.removeFromTop (kh).reduced (2), "Gain");
            bs.q.layout (col.reduced (2), "Q");
        }
    }

  private:
    void updateBandEnabled (int b)
    {
        auto& bs = bandStrips[static_cast<size_t> (b)];
        const bool on = bs.enableBtn.getToggleState ();
        bs.typeCombo.setEnabled (on);
        bs.freq.slider.setEnabled (on);
        bs.gain.slider.setEnabled (on);
        bs.q.slider.setEnabled (on);
    }

    struct BandStrip
    {
        juce::TextButton enableBtn;
        juce::ComboBox typeCombo;
        KnobGroup freq, gain, q;
    };

    ParametricEQModule& mod;
    EQCurveDisplay curveDisplay;
    std::array<BandStrip, ParametricEQModule::kNumBands> bandStrips;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQPanel)
};
