#pragma once
#include <JuceHeader.h>
#include "SpectrumAnalyzer.h"
#include "../dsp/SpectrumFifo.h"

/**
 * Right-hand inspector panel: spectrum analyzer + correlation/goniometer placeholder.
 */
class InspectorPanel : public juce::Component
{
  public:
    explicit InspectorPanel (SpectrumFifo& specFifo, double sampleRate)
        : spectrumAnalyzer (specFifo, sampleRate)
    {
        addAndMakeVisible (spectrumAnalyzer);
        addAndMakeVisible (titleLabel);
        titleLabel.setText ("Inspector", juce::dontSendNotification);
        titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0ab));
        titleLabel.setFont (juce::FontOptions (11.f));
        titleLabel.setJustificationType (juce::Justification::centred);
    }

    void setSampleRate (double newSr) { spectrumAnalyzer.setSampleRate (newSr); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1e2128));

        // Correlation placeholder below spectrum
        auto b = getLocalBounds ();
        const int corrH = 60;
        const auto corrArea = b.removeFromBottom (corrH);

        g.setColour (juce::Colour (0xff16181d));
        g.fillRect (corrArea);
        g.setColour (juce::Colour (0xff9aa0ab));
        g.setFont (10.f);
        g.drawText ("Goniometer / Correlation (Phase 5)", corrArea.toFloat (),
                    juce::Justification::centred, false);
    }

    void resized () override
    {
        auto b = getLocalBounds ();
        titleLabel.setBounds (b.removeFromTop (22));
        b.removeFromBottom (60); // correlation placeholder
        spectrumAnalyzer.setBounds (b.reduced (4));
    }

  private:
    SpectrumAnalyzer spectrumAnalyzer;
    juce::Label titleLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InspectorPanel)
};
