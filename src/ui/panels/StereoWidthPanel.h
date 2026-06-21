#pragma once
#include "PanelHelpers.h"
#include "../../modules/StereoWidthModule.h"

/**
 * Stereo Width panel.
 * Width knob (0–200%), Bass Mono cutoff (0–400 Hz), Pan (-1 to +1).
 * A Lissajous-style width indicator shows the current stereo field.
 */
class StereoWidthPanel : public juce::Component, private juce::Timer
{
  public:
    explicit StereoWidthPanel (StereoWidthModule& m) : mod (m)
    {
        bindKnob (widthKnob,     m.width,      0.0,   200.0, 1.0,   " %");
        bindKnob (monoKnob,      m.monoBelow,  0.0,   400.0, 1.0,   " Hz",
                  "Bass mono below this frequency (0 = off)");
        bindKnob (panKnob,       m.pan,       -1.0,     1.0, 0.01,  "");

        widthKnob.addTo (*this);
        monoKnob.addTo  (*this);
        panKnob.addTo   (*this);

        startTimerHz (10);
    }

    ~StereoWidthPanel () override { stopTimer (); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (AP::kBgBase));

        // Width visualiser: a horizontal bar spanning from mono (left) to hyper-wide (right)
        const auto vis = getVisBounds ();
        g.setColour (juce::Colour (AP::kBgDeep));
        g.fillRoundedRectangle (vis.toFloat (), 3.f);

        const float w = juce::jlimit (0.f, 200.f, mod.width.load ());
        const float wFrac = w / 200.f;

        // Draw a mirrored fill growing from the centre (100% is centred)
        const float cx     = vis.getCentreX ();
        const float halfW  = vis.getWidth () * 0.5f;
        // At 100% the bar fills half the total width on each side (50% of full)
        // At 0% nothing, at 200% full bar
        const float fillHalf = halfW * wFrac;
        g.setColour (juce::Colour (AP::kAccentBr).withAlpha (0.55f));
        g.fillRoundedRectangle (cx - fillHalf, static_cast<float> (vis.getY ()),
                                fillHalf * 2.f, static_cast<float> (vis.getHeight ()), 3.f);

        // Centre marker at 100%
        g.setColour (juce::Colour (AP::kDiv));
        g.fillRect (cx - 0.5f, static_cast<float> (vis.getY ()),
                    1.f, static_cast<float> (vis.getHeight ()));

        // Mono marker at 0% and hyper at 200%
        g.setFont (juce::FontOptions (9.f));
        g.setColour (juce::Colour (AP::kTxtLo));
        auto labelRow = vis;
        g.drawText ("Mono", labelRow.removeFromLeft (36), juce::Justification::centredLeft);
        g.drawText ("Wide", labelRow.removeFromRight (36), juce::Justification::centredRight);

        // Value readout
        const juce::String txt = juce::String (static_cast<int> (std::round (w))) + " %";
        g.setColour (juce::Colour (AP::kAccentBr));
        g.setFont (juce::FontOptions (11.f));
        g.drawText (txt, vis, juce::Justification::centred);
    }

    void resized () override
    {
        auto b = getLocalBounds ().reduced (12, 16);
        b.removeFromBottom (36); // visualiser strip

        const int kw = b.getWidth () / 3;
        widthKnob.layout (b.removeFromLeft (kw).reduced (4), "Width");
        monoKnob.layout  (b.removeFromLeft (kw).reduced (4), "Bass Mono");
        panKnob.layout   (b.reduced (4), "Pan");
    }

  private:
    void timerCallback () override
    {
        widthKnob.updateValue (mod.width.load (),     " %");
        monoKnob.updateValue  (mod.monoBelow.load (), " Hz");
        panKnob.updateValue   (mod.pan.load (),       "");
        repaint (getVisBounds ());
    }

    juce::Rectangle<int> getVisBounds () const
    {
        return getLocalBounds ().reduced (16, 8).removeFromBottom (24);
    }

    StereoWidthModule& mod;
    KnobGroup widthKnob, monoKnob, panKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereoWidthPanel)
};
