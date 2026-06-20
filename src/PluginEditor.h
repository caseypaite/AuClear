#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/AuClearLookAndFeel.h"
#include "ui/HeaderComponent.h"
#include "ui/RackColumn.h"
#include "ui/MainStage.h"
#include "ui/MeterBridge.h"
#include "ui/InspectorPanel.h"
#include "ui/FileProcessorPanel.h"

class AuClearAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
  public:
    explicit AuClearAudioProcessorEditor (AuClearAudioProcessor&);
    ~AuClearAudioProcessorEditor () override;

    void paint (juce::Graphics&) override;
    void resized () override;

  private:
    void timerCallback () override;
    void toggleFileProcessorWindow ();

    AuClearAudioProcessor& processorRef;

    AuClearLookAndFeel lookAndFeel;
    HeaderComponent header;
    RackColumn rackColumn;
    MainStage mainStage;
    MeterBridge meterBridge;
    InspectorPanel inspectorPanel;

    // Standalone-only: file processing button + floating window
    juce::TextButton fileProcessorButton{"Process File\xe2\x80\xa6"}; // "Process File…"
    std::unique_ptr<juce::DocumentWindow> fileProcessorWindow;

    static constexpr juce::uint32 kDivider = 0xff2a2e37;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuClearAudioProcessorEditor)
};
