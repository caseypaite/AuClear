#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/AuClearLookAndFeel.h"
#include "ui/HeaderComponent.h"
#include "ui/RackColumn.h"
#include "ui/MainStage.h"
#include "ui/MeterBridge.h"
#include "ui/InspectorPanel.h"
#include "ui/MediaPlayerPanel.h"

class AuClearAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
  public:
    explicit AuClearAudioProcessorEditor (AuClearAudioProcessor&);
    ~AuClearAudioProcessorEditor () override;

    void paint (juce::Graphics&) override;
    void resized () override;

  private:
    void timerCallback () override;

    AuClearAudioProcessor& processorRef;

    AuClearLookAndFeel lookAndFeel;
    HeaderComponent header;
    RackColumn rackColumn;
    MainStage mainStage;
    MeterBridge meterBridge;
    InspectorPanel inspectorPanel;

    // Standalone-only: media-player strip along the bottom
    std::unique_ptr<MediaPlayerPanel> mediaPlayerPanel;

    static constexpr juce::uint32 kDivider = 0xff2a2e37;
    static constexpr int kPlayerHeight = 110;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuClearAudioProcessorEditor)
};
