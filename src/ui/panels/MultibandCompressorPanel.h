#pragma once
#include <JuceHeader.h>
#include "PanelHelpers.h"
#include "../../modules/MultibandCompressorModule.h"
#include "../GRMeter.h"

class MultibandCompressorPanel : public juce::Component, private juce::Timer
{
  public:
    explicit MultibandCompressorPanel (MultibandCompressorModule& m) : module (m)
    {
        // Crossover + output knobs
        xo1Knob.addTo (*this);
        xo2Knob.addTo (*this);
        xo3Knob.addTo (*this);
        outKnob.addTo (*this);
        bindKnob (xo1Knob, m.xo1,    20.0, 2000.0, 1.0,  " Hz");
        bindKnob (xo2Knob, m.xo2,   100.0, 8000.0, 1.0,  " Hz");
        bindKnob (xo3Knob, m.xo3,  1000.0,18000.0, 1.0,  " Hz");
        bindKnob (outKnob,  m.outputDb, -12.0, 12.0, 0.1, " dB");

        for (int b = 0; b < MultibandCompressorModule::kNumBands; ++b)
        {
            auto& bc = bandCols[static_cast<size_t> (b)];
            auto& band = m.bands[static_cast<size_t> (b)];

            bc.enableBtn.setButtonText ("ON");
            bc.enableBtn.setClickingTogglesState (true);
            bc.enableBtn.setToggleState (band.enabled.load (), juce::dontSendNotification);
            bc.enableBtn.onStateChange = [&band, &bc] {
                band.enabled.store (bc.enableBtn.getToggleState ());
            };
            addAndMakeVisible (bc.enableBtn);
            addAndMakeVisible (bc.bandLabel);

            bc.threshKnob.addTo (*this);
            bc.ratioKnob.addTo  (*this);
            bc.atkKnob.addTo    (*this);
            bc.relKnob.addTo    (*this);
            bc.makeupKnob.addTo (*this);

            bindKnob (bc.threshKnob, band.thresholdDb, -60.0, 0.0,   0.5, " dB");
            bindKnob (bc.ratioKnob,  band.ratio,        1.0,  20.0,  0.1,  ":1");
            bindKnob (bc.atkKnob,    band.attackMs,     0.1,  100.0, 0.1,  " ms");
            bindKnob (bc.relKnob,    band.releaseMs,    5.0,  500.0, 1.0,  " ms");
            bindKnob (bc.makeupKnob, band.makeupDb,    -12.0,  24.0, 0.5, " dB");

            bc.grMeter = std::make_unique<GRMeter> (band.currentGR, 24.f);
            addAndMakeVisible (*bc.grMeter);
        }

        startTimerHz (10);
    }

    ~MultibandCompressorPanel () override { stopTimer (); }

    void resized () override
    {
        auto b = getLocalBounds ().reduced (8, 6);

        // Top row: crossover + output knobs
        auto topRow = b.removeFromTop (76);
        const int kW = topRow.getWidth () / 4;
        xo1Knob.layout (topRow.removeFromLeft (kW), "XO 1");
        xo2Knob.layout (topRow.removeFromLeft (kW), "XO 2");
        xo3Knob.layout (topRow.removeFromLeft (kW), "XO 3");
        outKnob.layout (topRow,                     "Output");

        b.removeFromTop (4); // gap

        // 4 band columns
        const int colW  = b.getWidth () / MultibandCompressorModule::kNumBands;
        for (int bi = 0; bi < MultibandCompressorModule::kNumBands; ++bi)
        {
            auto col = b.removeFromLeft (bi < MultibandCompressorModule::kNumBands - 1 ? colW : b.getWidth ());
            auto& bc = bandCols[static_cast<size_t> (bi)];

            // Header
            auto hdr = col.removeFromTop (28);
            bc.enableBtn.setBounds (hdr.removeFromRight (36).reduced (2));
            bc.bandLabel.setBounds (hdr.reduced (2, 0));

            // GR meter strip on right
            bc.grMeter->setBounds (col.removeFromRight (14));
            col.removeFromRight (2);

            // 5 knobs in 3 rows
            const int knobW = col.getWidth () / 2;
            const int knobH = juce::jmin (72, col.getHeight () / 3);

            auto row1 = col.removeFromTop (knobH);
            bc.threshKnob.layout (row1.removeFromLeft (knobW), "Thresh");
            bc.ratioKnob.layout  (row1,                        "Ratio");

            auto row2 = col.removeFromTop (knobH);
            bc.atkKnob.layout (row2.removeFromLeft (knobW), "Attack");
            bc.relKnob.layout (row2,                        "Release");

            auto row3 = col.removeFromTop (knobH);
            // Makeup centred in row
            bc.makeupKnob.layout (row3.withSizeKeepingCentre (knobW, knobH), "Makeup");
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (panelBg ());

        // Vertical band dividers
        const int top  = 88;
        const int colW = (getWidth () - 16) / MultibandCompressorModule::kNumBands;
        g.setColour (divider ());
        for (int i = 1; i < MultibandCompressorModule::kNumBands; ++i)
            g.fillRect (8 + i * colW, top, 1, getHeight () - top - 6);

        // Band colour headers
        const juce::Colour bandColours[4] = {
            juce::Colour (0xff4a9eff),
            juce::Colour (0xff28e0c8),
            juce::Colour (0xffffcf00),
            juce::Colour (0xffff7043),
        };
        const juce::String bandNames[4] = { "Low", "Lo-Mid", "Hi-Mid", "High" };
        g.setFont (juce::FontOptions (11.f));
        for (int i = 0; i < MultibandCompressorModule::kNumBands; ++i)
        {
            const int bx = 8 + i * colW;
            g.setColour (bandColours[i].withAlpha (0.12f));
            g.fillRect (bx + 1, top, colW - 2, 28);
            g.setColour (bandColours[i]);
            g.drawText (bandNames[i], bx + 4, top + 7, colW - 44, 14,
                        juce::Justification::centredLeft);
        }

        // Horizontal separator under top row
        g.setColour (divider ());
        g.fillRect (0, 86, getWidth (), 1);
    }

  private:
    void timerCallback () override
    {
        xo1Knob.updateValue (module.xo1.load (),      " Hz");
        xo2Knob.updateValue (module.xo2.load (),      " Hz");
        xo3Knob.updateValue (module.xo3.load (),      " Hz");
        outKnob.updateValue (module.outputDb.load (),  " dB");

        for (int b = 0; b < MultibandCompressorModule::kNumBands; ++b)
        {
            auto& band = module.bands[static_cast<size_t> (b)];
            auto& bc   = bandCols[static_cast<size_t> (b)];
            bc.threshKnob.updateValue (band.thresholdDb.load (), " dB");
            bc.ratioKnob.updateValue  (band.ratio.load (),       ":1");
            bc.atkKnob.updateValue    (band.attackMs.load (),    " ms");
            bc.relKnob.updateValue    (band.releaseMs.load (),   " ms");
            bc.makeupKnob.updateValue (band.makeupDb.load (),    " dB");
        }
    }

    MultibandCompressorModule& module;

    KnobGroup xo1Knob, xo2Knob, xo3Knob, outKnob;

    struct BandCol
    {
        juce::TextButton enableBtn;
        juce::Label      bandLabel;
        KnobGroup        threshKnob, ratioKnob, atkKnob, relKnob, makeupKnob;
        std::unique_ptr<GRMeter> grMeter;
    };
    std::array<BandCol, MultibandCompressorModule::kNumBands> bandCols;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultibandCompressorPanel)
};
