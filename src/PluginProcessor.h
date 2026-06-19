#pragma once

#include <JuceHeader.h>
#include "engine/ProcessorRack.h"

class AuClearAudioProcessor : public juce::AudioProcessor
{
public:
    AuClearAudioProcessor();
    ~AuClearAudioProcessor() override;

    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    ProcessorRack& getRack() { return rack; }

    // Rough CPU load fraction — updated each processBlock from the load measurer.
    float getCpuLoad() const { return loadMeasurer.getLoadAsProportion(); }

private:
    ProcessorRack rack;
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessLoadMeasurer loadMeasurer;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuClearAudioProcessor)
};
