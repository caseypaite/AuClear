#include "PluginEditor.h"

// Thin DocumentWindow subclass so closeButtonPressed() hides rather than deletes.
struct FileProcessorDocWindow : public juce::DocumentWindow
{
    explicit FileProcessorDocWindow (AuClearAudioProcessor& p)
        : juce::DocumentWindow ("AuClear \xe2\x80\x94 Process File", juce::Colour (0xff1a1d24),
                                juce::DocumentWindow::closeButton)
    {
        setContentOwned (new FileProcessorPanel (p), true);
        setResizable (true, false);
        setUsingNativeTitleBar (true);
        centreWithSize (560, 330);
        setVisible (true);
    }

    void closeButtonPressed () override { setVisible (false); }
};

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
        addAndMakeVisible (fileProcessorButton);
        fileProcessorButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2e37));
        fileProcessorButton.setColour (juce::TextButton::textColourOffId,
                                       juce::Colour (0xff28e0c8));
        fileProcessorButton.onClick = [this] { toggleFileProcessorWindow (); };
    }

    setResizable (true, true);
    setResizeLimits (600, 400, 2560, 1600);
    setSize (1000, 620);

    startTimerHz (10);
}

AuClearAudioProcessorEditor::~AuClearAudioProcessorEditor ()
{
    stopTimer ();
    fileProcessorWindow.reset ();
    setLookAndFeel (nullptr);
}

void AuClearAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff16181d));
}

void AuClearAudioProcessorEditor::resized ()
{
    auto bounds = getLocalBounds ();

    auto headerBounds = bounds.removeFromTop (48);
    if (juce::JUCEApplicationBase::isStandaloneApp ())
    {
        fileProcessorButton.setBounds (headerBounds.removeFromRight (130).reduced (6, 8));
    }
    header.setBounds (headerBounds);

    meterBridge.setBounds (bounds.removeFromBottom (48));

    const int inspW = 220;
    inspectorPanel.setBounds (bounds.removeFromRight (inspW));

    bounds.removeFromRight (1);

    rackColumn.setBounds (bounds.removeFromLeft (210));
    mainStage.setBounds (bounds);
}

void AuClearAudioProcessorEditor::toggleFileProcessorWindow ()
{
    if (fileProcessorWindow != nullptr && fileProcessorWindow->isVisible ())
    {
        fileProcessorWindow->setVisible (false);
        return;
    }

    if (fileProcessorWindow == nullptr)
        fileProcessorWindow = std::make_unique<FileProcessorDocWindow> (processorRef);
    else
    {
        fileProcessorWindow->setVisible (true);
        fileProcessorWindow->toFront (true);
    }
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
