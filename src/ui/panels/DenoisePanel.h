#pragma once
#include "PanelHelpers.h"
#include "../../modules/DenoiseModule.h"

/**
 * Control panel for the DenoiseModule.
 *
 * Layout:
 *   [Status bar — model path / error]
 *   [Load Model...] [Listen to Removed]
 *   [Strength knob]
 */
class DenoisePanel : public juce::Component, public juce::Timer
{
  public:
    explicit DenoisePanel (DenoiseModule& m) : mod (m)
    {
        bindKnob (strength, m.strength, 0.0, 1.0, 0.01, "");

        addAndMakeVisible (strength.slider);
        addAndMakeVisible (strength.label);
        addAndMakeVisible (strength.value);

        loadBtn.setButtonText ("Load Model…");
        loadBtn.onClick = [this]
        {
            auto chooser = std::make_shared<juce::FileChooser> (
                "Select ONNX Model",
                juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                    .getChildFile (".auclear/models"),
                "*.onnx");

            chooser->launchAsync (juce::FileBrowserComponent::openMode |
                                      juce::FileBrowserComponent::canSelectFiles,
                                  [this, chooser] (const juce::FileChooser& fc)
                                  {
                                      auto results = fc.getResults ();
                                      if (results.isEmpty ())
                                          return;
                                      mod.setModelFile (results.getFirst ());
                                      updateStatus ();
                                  });
        };
        addAndMakeVisible (loadBtn);

        listenBtn.setButtonText ("Listen to Removed");
        listenBtn.setClickingTogglesState (true);
        listenBtn.setToggleState (m.listenToRemoved.load (), juce::dontSendNotification);
        listenBtn.onStateChange = [this]
        { mod.listenToRemoved.store (listenBtn.getToggleState ()); };
        addAndMakeVisible (listenBtn);

        statusLabel.setFont (juce::FontOptions (12.f));
        statusLabel.setColour (juce::Label::textColourId, textLo ());
        statusLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (statusLabel);

        cpuLabel.setFont (juce::FontOptions (11.f));
        cpuLabel.setJustificationType (juce::Justification::centredRight);
        cpuLabel.setColour (juce::Label::textColourId, textLo ());
        addAndMakeVisible (cpuLabel);

        // Refresh status when model loads
        mod.modelStatusChanged = [this] { updateStatus (); };

        updateStatus ();
        startTimerHz (4); // refresh value readout
    }

    ~DenoisePanel () override
    {
        stopTimer ();
        mod.modelStatusChanged = nullptr;
    }

    void resized () override
    {
        auto b = getLocalBounds ().reduced (16, 12);

        // Status bar
        auto statusRow = b.removeFromTop (20);
        cpuLabel.setBounds (statusRow.removeFromRight (80));
        statusLabel.setBounds (statusRow);
        b.removeFromTop (6);

        // Buttons row
        auto btnRow = b.removeFromTop (28);
        loadBtn.setBounds (btnRow.removeFromLeft (130).reduced (0, 2));
        btnRow.removeFromLeft (10);
        listenBtn.setBounds (btnRow.removeFromLeft (160).reduced (0, 2));
        b.removeFromTop (16);

        // Strength knob
        const int kw = 80;
        strength.layout (b.removeFromLeft (kw).reduced (4), "Strength");
    }

    void timerCallback () override
    {
        strength.updateValue (mod.strength.load ());

        const float load = mod.getCpuLoad ();
        if (load > 0.f)
        {
            const int pct = juce::roundToInt (load * 100.f);
            cpuLabel.setText (juce::String (pct) + "% CPU", juce::dontSendNotification);
            const juce::Colour col = load > 1.f    ? juce::Colour (0xffff4444)
                                     : load > 0.7f ? juce::Colour (0xffffaa00)
                                                   : textLo ();
            cpuLabel.setColour (juce::Label::textColourId, col);
        }
        else
        {
            cpuLabel.setText ("", juce::dontSendNotification);
        }
    }

  private:
    void updateStatus ()
    {
        const juce::String s = mod.getStatusString ();
        statusLabel.setText (s, juce::dontSendNotification);

        const bool ready = mod.isModelLoaded ();
        statusLabel.setColour (juce::Label::textColourId, ready ? accent () : textLo ());
        repaint ();
    }

    DenoiseModule& mod;
    KnobGroup strength;
    juce::TextButton loadBtn, listenBtn;
    juce::Label statusLabel, cpuLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DenoisePanel)
};
