#include "PluginEditor.h"

AuClearAudioProcessorEditor::AuClearAudioProcessorEditor (AuClearAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), rackColumn (p.getRack ()),
      meterBridge (p.getRack ()), inspectorPanel (p.getRack ().spectrumFifo (), p.getSampleRate ())
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (header);
    addAndMakeVisible (rackColumn);
    addAndMakeVisible (mainStage);
    addAndMakeVisible (meterBridge);
    addAndMakeVisible (inspectorPanel);

    rackColumn.onModuleSelected = [this] (RackModule* m) { mainStage.showModule (m); };

    setResizable (true, true);
    setResizeLimits (600, 400, 2560, 1600);
    setSize (1000, 620);

    startTimerHz (10);
}

AuClearAudioProcessorEditor::~AuClearAudioProcessorEditor ()
{
    stopTimer ();
    setLookAndFeel (nullptr);
}

void AuClearAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff16181d));
}

void AuClearAudioProcessorEditor::resized ()
{
    auto bounds = getLocalBounds ();

    header.setBounds (bounds.removeFromTop (48));
    meterBridge.setBounds (bounds.removeFromBottom (48));

    const int inspW = 220;
    inspectorPanel.setBounds (bounds.removeFromRight (inspW));

    // Left divider between inspector and main stage
    bounds.removeFromRight (1);

    rackColumn.setBounds (bounds.removeFromLeft (210));
    mainStage.setBounds (bounds);
}

void AuClearAudioProcessorEditor::timerCallback ()
{
    processorRef.getRack ().retireOldModules ();

    const double latMs =
        (double)processorRef.getLatencySamples () / processorRef.getSampleRate () * 1000.0;
    header.setCpuLoad (processorRef.getCpuLoad ());
    header.setLatencyMs (latMs);

    // Update inspector sample rate if it changed (e.g., after host changes it)
    inspectorPanel.setSampleRate (processorRef.getSampleRate ());
}
