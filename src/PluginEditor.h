#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
    Phase 0 editor: a resizable, on-brand placeholder using the AuClear dark
    palette from docs/04-ui-design.md. The header / rack / main-stage / inspector
    / meter-bridge layout arrives in Phase 1.
*/
class AuClearAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit AuClearAudioProcessorEditor (AuClearAudioProcessor&);
    ~AuClearAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AuClearAudioProcessor& processorRef;

    // Brand palette (see docs/04-ui-design.md).
    const juce::Colour bg      { 0xff16181d };
    const juce::Colour panel   { 0xff1e2128 };
    const juce::Colour accent  { 0xff28e0c8 };
    const juce::Colour textHi  { 0xffe8eaed };
    const juce::Colour textLo  { 0xff9aa0ab };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuClearAudioProcessorEditor)
};
