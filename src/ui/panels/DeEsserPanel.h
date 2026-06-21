#pragma once
#include "PanelHelpers.h"
#include "../../modules/DeEsserModule.h"
#include "../GRMeter.h"

class DeEsserPanel : public juce::Component, private juce::Timer
{
  public:
    explicit DeEsserPanel (DeEsserModule& m) : mod (m), grMeter (m.currentGR, 24.f)
    {
        bindKnob (freq,    m.freq,        200.0,  18000.0, 10.0,  " Hz");
        bindKnob (thresh,  m.thresholdDb, -60.0,    0.0,   0.1,  " dB");
        bindKnob (range,   m.rangeDb,       0.0,   24.0,   0.1,  " dB");
        bindKnob (attack,  m.attackMs,      0.1,   50.0,   0.1,  " ms");
        bindKnob (release, m.releaseMs,     5.0,  500.0,   1.0,  " ms");

        for (auto* k : {&freq, &thresh, &range, &attack, &release})
            k->addTo (*this);
        addAndMakeVisible (grMeter);

        listenBtn.setButtonText ("Listen");
        listenBtn.setClickingTogglesState (true);
        listenBtn.setColour (juce::TextButton::buttonColourId,  panelCol ());
        listenBtn.setColour (juce::TextButton::buttonOnColourId, accent ().withAlpha (0.35f));
        listenBtn.setColour (juce::TextButton::textColourOffId,  textLo ());
        listenBtn.setColour (juce::TextButton::textColourOnId,   accent ());
        listenBtn.onClick = [&m, this]
        { m.listen.store (listenBtn.getToggleState (), std::memory_order_relaxed); };
        addAndMakeVisible (listenBtn);

        freqLbl.setText ("Frequency", juce::dontSendNotification);
        freqLbl.setFont (juce::FontOptions (10.f));
        freqLbl.setColour (juce::Label::textColourId, textLo ());
        freqLbl.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (freqLbl);

        startTimerHz (10);
    }

    ~DeEsserPanel () override { stopTimer (); }

    void resized () override
    {
        auto b = getLocalBounds ().reduced (12, 16);

        grMeter.setBounds (b.removeFromRight (20).reduced (0, 4));
        b.removeFromRight (8);

        auto bottomRow = b.removeFromBottom (28);
        freqLbl.setBounds (bottomRow.removeFromLeft (70));
        listenBtn.setBounds (bottomRow.removeFromLeft (64).reduced (2, 4));

        const int kw = b.getWidth () / 5;
        freq.layout    (b.removeFromLeft (kw).reduced (4), "Freq");
        thresh.layout  (b.removeFromLeft (kw).reduced (4), "Thresh");
        range.layout   (b.removeFromLeft (kw).reduced (4), "Range");
        attack.layout  (b.removeFromLeft (kw).reduced (4), "Attack");
        release.layout (b.reduced (4), "Release");
    }

  private:
    void timerCallback () override
    {
        freq.updateValue    (mod.freq.load (),        " Hz");
        thresh.updateValue  (mod.thresholdDb.load (), " dB");
        range.updateValue   (mod.rangeDb.load (),     " dB");
        attack.updateValue  (mod.attackMs.load (),    " ms");
        release.updateValue (mod.releaseMs.load (),   " ms");
    }

    DeEsserModule& mod;
    KnobGroup      freq, thresh, range, attack, release;
    GRMeter        grMeter;
    juce::TextButton listenBtn{"Listen"};
    juce::Label    freqLbl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeEsserPanel)
};
