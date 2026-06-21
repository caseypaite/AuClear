#pragma once
#include "PanelHelpers.h"
#include "../../modules/TransientShaperModule.h"

/**
 * Transient Shaper panel.
 * Three knobs: Attack sensitivity, Sustain sensitivity, Output trim.
 * A bidirectional gain-ride bar visualises the dynamic gain being applied.
 */
class TransientShaperPanel : public juce::Component, private juce::Timer
{
  public:
    explicit TransientShaperPanel (TransientShaperModule& m) : mod (m)
    {
        bindKnob (attackKnob,  m.attackSens,  -12.0, 12.0, 0.1, " dB");
        bindKnob (sustainKnob, m.sustainSens, -12.0, 12.0, 0.1, " dB");
        bindKnob (outputKnob,  m.outputGain,  -12.0, 12.0, 0.1, " dB");

        attackKnob.addTo  (*this);
        sustainKnob.addTo (*this);
        outputKnob.addTo  (*this);

        startTimerHz (30);
    }

    ~TransientShaperPanel () override { stopTimer (); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (AP::kBgBase));

        // Gain-ride bar (bidirectional, ±12 dB)
        const auto barArea = getRideBarBounds ();
        g.setColour (juce::Colour (AP::kBgDeep));
        g.fillRoundedRectangle (barArea.toFloat (), 3.f);

        const float db = juce::jlimit (-12.f, 12.f, displayGainDb);
        const float cx = barArea.getCentreX ();
        const float fracAbs = std::abs (db) / 12.f;
        const float barW = fracAbs * (barArea.getWidth () * 0.5f);

        if (barW > 0.5f)
        {
            const juce::Colour barCol = db >= 0.f
                ? juce::Colour (AP::kAccentBr)
                : juce::Colour (AP::kAccentDm).brighter (0.3f);

            const float barX = db >= 0.f ? cx : cx - barW;
            g.setColour (barCol);
            g.fillRoundedRectangle (barX, static_cast<float> (barArea.getY ()),
                                    barW, static_cast<float> (barArea.getHeight ()), 3.f);
        }

        // Centre line
        g.setColour (juce::Colour (AP::kDiv));
        g.fillRect (cx - 0.5f, static_cast<float> (barArea.getY ()),
                    1.f, static_cast<float> (barArea.getHeight ()));

        // Labels
        g.setFont (juce::FontOptions (9.f));
        g.setColour (juce::Colour (AP::kTxtLo));
        auto labelRow = barArea;
        g.drawText ("-12 dB", labelRow.removeFromLeft (40), juce::Justification::centredLeft);
        g.drawText ("+12 dB", labelRow.removeFromRight (40), juce::Justification::centredRight);

        // Gain readout
        const juce::String txt = (std::abs (db) < 0.1f)
                                     ? juce::String ("0.0 dB")
                                     : juce::String (db, 1) + " dB";
        g.setColour (juce::Colour (AP::kAccentBr));
        g.setFont (juce::FontOptions (11.f));
        g.drawText (txt, getRideBarBounds (), juce::Justification::centred);
    }

    void resized () override
    {
        auto b = getLocalBounds ().reduced (12, 16);
        b.removeFromBottom (36); // ride bar at bottom

        const int kw = b.getWidth () / 3;
        attackKnob.layout  (b.removeFromLeft (kw).reduced (4), "Attack");
        sustainKnob.layout (b.removeFromLeft (kw).reduced (4), "Sustain");
        outputKnob.layout  (b.reduced (4), "Output");
    }

  private:
    void timerCallback () override
    {
        const float newGain = mod.gainRide.load ();
        displayGainDb = 0.7f * displayGainDb + 0.3f * newGain;
        attackKnob.updateValue  (mod.attackSens.load (),  " dB");
        sustainKnob.updateValue (mod.sustainSens.load (), " dB");
        outputKnob.updateValue  (mod.outputGain.load (),  " dB");
        repaint (getRideBarBounds ());
    }

    juce::Rectangle<int> getRideBarBounds () const
    {
        return getLocalBounds ().reduced (16, 8).removeFromBottom (24);
    }

    TransientShaperModule& mod;
    KnobGroup attackKnob, sustainKnob, outputKnob;
    float displayGainDb{0.f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransientShaperPanel)
};
