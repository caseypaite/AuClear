#include "PluginEditor.h"

//==============================================================================
AuClearAudioProcessorEditor::AuClearAudioProcessorEditor (AuClearAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setResizable (true, true);
    setResizeLimits (480, 320, 2560, 1600);
    setSize (900, 560);
}

AuClearAudioProcessorEditor::~AuClearAudioProcessorEditor() = default;

//==============================================================================
void AuClearAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.fillAll (bg);

    // Header bar.
    auto header = bounds.removeFromTop (48);
    g.setColour (panel);
    g.fillRect (header);

    auto brandArea = header.reduced (16, 0);
    g.setColour (accent);
    g.setFont (juce::Font (juce::FontOptions (22.0f).withStyle ("Bold")));
    g.drawText ("AuClear", brandArea, juce::Justification::centredLeft, false);

    g.setColour (textLo);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("v" JucePlugin_VersionString "  ·  Phase 0",
                brandArea, juce::Justification::centredRight, false);

    // Centre placeholder.
    g.setColour (textHi);
    g.setFont (juce::FontOptions (18.0f));
    g.drawText ("AI audio repair, restoration & stem-remix rack",
                bounds, juce::Justification::centred, false);

    auto sub = bounds.withTrimmedTop (bounds.getHeight() / 2 + 18);
    g.setColour (textLo);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("Rack engine, AI modules and visualizations arrive in Phase 1+",
                sub, juce::Justification::centredTop, false);
}

void AuClearAudioProcessorEditor::resized()
{
    // Phase 1: lay out header / rack / main-stage / inspector / meter-bridge.
}
