#include "PluginEditor.h"
#include "ui/AnalogPalette.h"

AuClearAudioProcessorEditor::AuClearAudioProcessorEditor (AuClearAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), rackColumn (p.getRack ()),
      meterBridge (p.getRack ()), inspectorPanel (p.getRack ().spectrumFifo (), p.getRack ().goniometerFifo (), p.getSampleRate ())
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (header);
    addAndMakeVisible (rackColumn);
    addAndMakeVisible (mainStage);
    addAndMakeVisible (meterBridge);
    addAndMakeVisible (inspectorPanel);

    rackColumn.onModuleSelected = [this] (RackModule* m) { mainStage.showModule (m); };

    header.onUndoClicked = [this]
    {
        if (processorRef.undo ())
            rackColumn.syncWithRack ();
    };

    rackColumn.onBeforeStructuralChange = [this] { processorRef.snapshotForUndo (); };

    header.onThemeChanged = [this] (int idx)
    {
        AP::setTheme (idx);
        repaint ();              // repaint the whole editor
        rackColumn.repaint ();
        mainStage.repaint ();
        meterBridge.repaint ();
        inspectorPanel.repaint ();
        header.repaint ();
    };

    // Restore saved theme before first paint
    AP::loadSavedTheme ();

    header.onPresetChosen = [this] (const juce::String& choice)
    {
        handlePresetChosen (choice);
    };

    // Populate preset browser with factory + user preset names
    {
        const auto lists = p.getPresetLists ();
        header.setPresetList (lists.factory, lists.user);
        header.setCurrentPreset ("Init");
    }

    if (juce::JUCEApplicationBase::isStandaloneApp ())
    {
        mediaPlayerPanel = std::make_unique<MediaPlayerPanel> (p);
        addAndMakeVisible (*mediaPlayerPanel);
    }

    stemRemixPanel = std::make_unique<StemRemixPanel> (p);
    addAndMakeVisible (*stemRemixPanel);

    setResizable (true, true);
    const int h = 620 + kStemPanelHeight + (juce::JUCEApplicationBase::isStandaloneApp () ? kPlayerHeight : 0);
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
    g.fillAll (juce::Colour (AP::kBgBase));
}

void AuClearAudioProcessorEditor::handlePresetChosen (const juce::String& choice)
{
    if (choice.startsWith ("__save__:"))
    {
        const juce::String name = choice.fromFirstOccurrenceOf ("__save__:", false, false);
        processorRef.savePreset (name);
        header.setCurrentPreset (name);
        const auto info = processorRef.getPresetLists ();
        header.setPresetList (info.factory, info.user);
    }
    else if (choice.startsWith ("__delete__:"))
    {
        const juce::String name = choice.fromFirstOccurrenceOf ("__delete__:", false, false);
        processorRef.deletePreset (name);
        header.setCurrentPreset ("Init");
        const auto info = processorRef.getPresetLists ();
        header.setPresetList (info.factory, info.user);
    }
    else
    {
        processorRef.loadPreset (choice);
        header.setCurrentPreset (choice);
        rackColumn.syncWithRack ();
    }
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

    if (stemRemixPanel != nullptr)
        stemRemixPanel->syncWithProcessor ();
}
