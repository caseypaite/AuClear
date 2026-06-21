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

    if (juce::JUCEApplicationBase::isStandaloneApp ())
    {
        mediaPlayerPanel = std::make_unique<MediaPlayerPanel> (p);
        addAndMakeVisible (*mediaPlayerPanel);

        stemRemixPanel = std::make_unique<StemRemixPanel> (p);
        addAndMakeVisible (*stemRemixPanel);
    }

    setResizable (true, true);
    const int h = juce::JUCEApplicationBase::isStandaloneApp ()
                      ? 620 + kPlayerHeight + kStemPanelHeight
                      : 620;
    setResizeLimits (600, 500, 2560, 1800);
    setSize (1000, h);

    startTimerHz (10);
}

AuClearAudioProcessorEditor::~AuClearAudioProcessorEditor ()
{
    stopTimer ();
    stemRemixPanel.reset ();   // cancel any running jobs before other teardown
    mediaPlayerPanel.reset (); // unloads transport before anything else tears down
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

    if (stemRemixPanel)
        stemRemixPanel->setBounds (bounds.removeFromBottom (kStemPanelHeight));

    if (mediaPlayerPanel)
        mediaPlayerPanel->setBounds (bounds.removeFromBottom (kPlayerHeight));

    meterBridge.setBounds (bounds.removeFromBottom (48));

    const int inspW = 220;
    inspectorPanel.setBounds (bounds.removeFromRight (inspW));

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

    inspectorPanel.setSampleRate (processorRef.getSampleRate ());
}
