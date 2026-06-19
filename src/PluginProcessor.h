#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    Phase 0 processor: a transparent stereo passthrough.

    It exists to prove the build/host plumbing (all four plugin formats load,
    state round-trips, PDC is reported) before any DSP or rack engine lands.
    The modular ProcessorRack arrives in Phase 1 — see docs/01-architecture.md.
*/
class AuClearAudioProcessor : public juce::AudioProcessor
{
public:
    AuClearAudioProcessor();
    ~AuClearAudioProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==========================================================================
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==========================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuClearAudioProcessor)
};
