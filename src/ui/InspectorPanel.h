#pragma once
#include <JuceHeader.h>
#include "SpectrumAnalyzer.h"
#include "GoniometerDisplay.h"
#include "../dsp/SpectrumFifo.h"
#include "../dsp/GoniometerFifo.h"
#include "AnalogPalette.h"

/**
 * Right-hand inspector panel: spectrum analyzer (top) + Lissajous goniometer
 * with stereo correlation meter (bottom).
 */
class InspectorPanel : public juce::Component
{
  public:
    InspectorPanel (SpectrumFifo& specFifo, GoniometerFifo& gonioFifo, double sampleRate)
        : spectrumAnalyzer (specFifo, sampleRate), goniometer (gonioFifo)
    {
        addAndMakeVisible (spectrumAnalyzer);
        addAndMakeVisible (goniometer);
        addAndMakeVisible (titleLabel);
        titleLabel.setText ("Inspector", juce::dontSendNotification);
        titleLabel.setColour (juce::Label::textColourId, juce::Colour (AP::kTxtLo));
        titleLabel.setFont (juce::FontOptions (11.f));
        titleLabel.setJustificationType (juce::Justification::centred);
    }

    void setSampleRate (double newSr) { spectrumAnalyzer.setSampleRate (newSr); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (AP::kBgPanel));

        // Separator between spectrum and goniometer
        const int sepY = titleH + specH ();
        g.setColour (juce::Colour (AP::kDiv));
        g.fillRect (0, sepY, getWidth (), 1);
    }

    void resized () override
    {
        auto b = getLocalBounds ();
        titleLabel.setBounds (b.removeFromTop (titleH));
        spectrumAnalyzer.setBounds (b.removeFromTop (specH ()).reduced (4, 2));
        goniometer.setBounds (b);
    }

  private:
    int specH () const
    {
        // Give spectrum ~55% of non-title area, goniometer the rest
        return static_cast<int> ((getHeight () - titleH) * 0.55f);
    }

    static constexpr int titleH = 22;

    SpectrumAnalyzer  spectrumAnalyzer;
    GoniometerDisplay goniometer;
    juce::Label       titleLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InspectorPanel)
};
