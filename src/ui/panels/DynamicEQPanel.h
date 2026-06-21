#pragma once
#include "PanelHelpers.h"
#include "../../modules/DynamicEQModule.h"
#include "../GRMeter.h"
#include <array>
#include <memory>

// Each band is shown as a column: enable toggle, static knobs (Freq/Q/Gain),
// divider, dynamic knobs (Thresh/Range/Attack/Release), GR meter.

class DynamicEQPanel : public juce::Component, private juce::Timer
{
  public:
    explicit DynamicEQPanel (DynamicEQModule& m) : mod (m)
    {
        static const juce::Colour kBandCols[4] = {
            juce::Colour (0xff4a9eff),
            juce::Colour (0xff28e0c8),
            juce::Colour (0xffffcf00),
            juce::Colour (0xffff7043),
        };

        for (int b = 0; b < DynamicEQModule::kNumBands; ++b)
        {
            auto& band = m.bands[(size_t)b];
            auto& col  = cols[(size_t)b];

            col.grMeter = std::make_unique<GRMeter> (band.currentGR, 18.f);
            addAndMakeVisible (*col.grMeter);

            // Enable toggle
            col.enableBtn.setButtonText ("");
            col.enableBtn.setClickingTogglesState (true);
            col.enableBtn.setToggleState (band.enabled.load (), juce::dontSendNotification);
            const auto bandCol = kBandCols[b];
            col.enableBtn.setColour (juce::TextButton::buttonColourId,
                                     juce::Colour (0xff2a2e37));
            col.enableBtn.setColour (juce::TextButton::buttonOnColourId,
                                     bandCol.withAlpha (0.4f));
            col.enableBtn.onClick = [b, &band, &col, bandCol]
            {
                const bool on = col.enableBtn.getToggleState ();
                band.enabled.store (on, std::memory_order_relaxed);
            };
            addAndMakeVisible (col.enableBtn);

            col.bandLabel.setText ("Band " + juce::String (b + 1), juce::dontSendNotification);
            col.bandLabel.setFont (juce::Font (juce::FontOptions (10.f).withStyle ("Bold")));
            col.bandLabel.setColour (juce::Label::textColourId, bandCol);
            col.bandLabel.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (col.bandLabel);

            bindKnob (col.freq,    band.freq,      20.0, 20000.0, 1.0,   " Hz");
            bindKnob (col.q,       band.q,          0.1,    10.0, 0.01,  "");
            bindKnob (col.gain,    band.gainDb,   -18.0,    18.0, 0.1,   " dB");
            bindKnob (col.thresh,  band.threshDb, -60.0,     0.0, 0.1,   " dB");
            bindKnob (col.range,   band.rangeDb,    0.0,    18.0, 0.1,   " dB");
            bindKnob (col.attack,  band.attackMs,   0.1,   100.0, 0.1,   " ms");
            bindKnob (col.release, band.releaseMs,  1.0,  2000.0, 1.0,   " ms");

            for (auto* k : {&col.freq, &col.q, &col.gain, &col.thresh, &col.range,
                             &col.attack, &col.release})
                k->addTo (*this);
        }

        // Static/Dynamic section labels
        for (auto& lbl : staticLbl)
        {
            lbl.setText ("EQ", juce::dontSendNotification);
            lbl.setFont (juce::FontOptions (9.f));
            lbl.setColour (juce::Label::textColourId, textLo ());
            lbl.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (lbl);
        }
        for (auto& lbl : dynLbl)
        {
            lbl.setText ("DYN", juce::dontSendNotification);
            lbl.setFont (juce::FontOptions (9.f));
            lbl.setColour (juce::Label::textColourId, textLo ());
            lbl.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (lbl);
        }

        startTimerHz (10);
    }

    ~DynamicEQPanel () override { stopTimer (); }

    void paint (juce::Graphics& g) override
    {
        // Column dividers
        const int cw = getWidth () / DynamicEQModule::kNumBands;
        g.setColour (juce::Colour (0xff2a2e37));
        for (int i = 1; i < DynamicEQModule::kNumBands; ++i)
            g.fillRect (i * cw - 1, 0, 1, getHeight ());

        // EQ / DYN section separator per column
        // (drawn as a light horizontal line between gain and thresh knobs)
        const float sepY = getHeight () * 0.52f;
        g.setColour (juce::Colour (0xff2a2e37));
        g.fillRect (0, (int)sepY, getWidth (), 1);
    }

    void resized () override
    {
        const int cw   = getWidth () / DynamicEQModule::kNumBands;
        const int h    = getHeight ();
        const int hdr  = 26;

        for (int b = 0; b < DynamicEQModule::kNumBands; ++b)
        {
            auto& col = cols[(size_t)b];
            auto bnd = juce::Rectangle<int> (b * cw, 0, cw, h);

            // Header row: band label + enable dot
            auto hdrRow = bnd.removeFromTop (hdr);
            col.enableBtn.setBounds (hdrRow.removeFromRight (24).reduced (3, 4));
            col.bandLabel.setBounds (hdrRow.reduced (4, 0));

            // GR meter on right edge of remaining area
            col.grMeter->setBounds (bnd.removeFromRight (18).reduced (2, 8));
            bnd.removeFromRight (2);

            const int half = (int)(bnd.getHeight () * 0.52f);
            auto topArea = bnd.removeFromTop (half);
            auto botArea = bnd;

            // Section tags
            const int tagH = 14;
            staticLbl[(size_t)b].setBounds (topArea.removeFromTop (tagH));
            dynLbl[(size_t)b].setBounds    (botArea.removeFromTop (tagH));

            // Top: Freq / Q / Gain
            const int kw3 = topArea.getWidth () / 3;
            col.freq.layout  (topArea.removeFromLeft (kw3).reduced (3), "Freq");
            col.q.layout     (topArea.removeFromLeft (kw3).reduced (3), "Q");
            col.gain.layout  (topArea.reduced (3), "Gain");

            // Bottom: Thresh / Range / Attack / Release
            const int kw4 = botArea.getWidth () / 4;
            col.thresh.layout  (botArea.removeFromLeft (kw4).reduced (3), "Thr");
            col.range.layout   (botArea.removeFromLeft (kw4).reduced (3), "Rng");
            col.attack.layout  (botArea.removeFromLeft (kw4).reduced (3), "Atk");
            col.release.layout (botArea.reduced (3), "Rel");
        }
    }

  private:
    void timerCallback () override
    {
        for (int b = 0; b < DynamicEQModule::kNumBands; ++b)
        {
            auto& band = mod.bands[(size_t)b];
            auto& col  = cols[(size_t)b];
            col.freq.updateValue    (band.freq.load (),      " Hz");
            col.q.updateValue       (band.q.load (),         "");
            col.gain.updateValue    (band.gainDb.load (),    " dB");
            col.thresh.updateValue  (band.threshDb.load (),  " dB");
            col.range.updateValue   (band.rangeDb.load (),   " dB");
            col.attack.updateValue  (band.attackMs.load (),  " ms");
            col.release.updateValue (band.releaseMs.load (), " ms");
        }
    }

    struct BandColumn
    {
        juce::TextButton        enableBtn;
        juce::Label             bandLabel;
        KnobGroup               freq, q, gain;
        KnobGroup               thresh, range, attack, release;
        std::unique_ptr<GRMeter> grMeter;
    };

    DynamicEQModule& mod;
    std::array<BandColumn, DynamicEQModule::kNumBands> cols;
    std::array<juce::Label, DynamicEQModule::kNumBands> staticLbl;
    std::array<juce::Label, DynamicEQModule::kNumBands> dynLbl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicEQPanel)
};
