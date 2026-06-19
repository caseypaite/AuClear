#include "PluginEditor.h"

AuClearAudioProcessorEditor::AuClearAudioProcessorEditor (AuClearAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), rackColumn (p.getRack ()),
      meterBridge (p.getRack ())
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (header);
    addAndMakeVisible (rackColumn);
    addAndMakeVisible (mainStage);
    addAndMakeVisible (meterBridge);

    // Wire rack column selection to main stage
    rackColumn.onModuleSelected = [this] (RackModule* m) { mainStage.showModule (m); };

    setResizable (true, true);
    setResizeLimits (560, 380, 2560, 1600);
    setSize (960, 600);

    // Slow timer for rack retirement + header stats
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

    // Inspector placeholder (right column)
    const int inspW = 200;
    auto inspR = getLocalBounds ().removeFromTop (getHeight () - 48 - 48).removeFromRight (inspW);
    g.setColour (juce::Colour (0xff1e2128));
    g.fillRect (inspR);
    g.setColour (juce::Colour (kDivider));
    g.fillRect (inspR.getX (), inspR.getY (), 1, inspR.getHeight ());
    g.setColour (juce::Colour (0xff9aa0ab));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("Analyzer", inspR, juce::Justification::centred);
}

void AuClearAudioProcessorEditor::resized ()
{
    auto bounds = getLocalBounds ();

    header.setBounds (bounds.removeFromTop (48));
    meterBridge.setBounds (bounds.removeFromBottom (48));

    // Right inspector (placeholder)
    bounds.removeFromRight (200);

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
}
