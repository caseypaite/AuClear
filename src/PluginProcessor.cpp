#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AuClearAudioProcessor::AuClearAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

AuClearAudioProcessor::~AuClearAudioProcessor() = default;

//==============================================================================
void AuClearAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (sampleRate, samplesPerBlock);
    // Phase 1+: prepare the ProcessorRack here and report summed latency.
    setLatencySamples (0);
}

void AuClearAudioProcessor::releaseResources()
{
}

bool AuClearAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Accept matching mono->mono or stereo->stereo layouts only.
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono()
        && mainOut != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == mainOut;
}

void AuClearAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    // Clear any output channels that have no matching input (defensive).
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Phase 0: transparent passthrough. The rack chain processes here in Phase 1.
}

//==============================================================================
juce::AudioProcessorEditor* AuClearAudioProcessor::createEditor()
{
    return new AuClearAudioProcessorEditor (*this);
}

bool AuClearAudioProcessor::hasEditor() const            { return true; }

//==============================================================================
const juce::String AuClearAudioProcessor::getName() const { return JucePlugin_Name; }
bool AuClearAudioProcessor::acceptsMidi() const           { return false; }
bool AuClearAudioProcessor::producesMidi() const          { return false; }
bool AuClearAudioProcessor::isMidiEffect() const          { return false; }
double AuClearAudioProcessor::getTailLengthSeconds() const { return 0.0; }

//==============================================================================
int AuClearAudioProcessor::getNumPrograms()                            { return 1; }
int AuClearAudioProcessor::getCurrentProgram()                         { return 0; }
void AuClearAudioProcessor::setCurrentProgram (int index)              { juce::ignoreUnused (index); }
const juce::String AuClearAudioProcessor::getProgramName (int index)   { juce::ignoreUnused (index); return {}; }
void AuClearAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void AuClearAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Phase 1: serialize the APVTS + rack ValueTree here. Stubbed for now so
    // host save/restore is well-defined (empty state) from day one.
    juce::ValueTree state ("AuClearState");
    state.setProperty ("version", JucePlugin_VersionString, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AuClearAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        juce::ignoreUnused (state); // Phase 1: apply to APVTS + rack.
    }
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AuClearAudioProcessor();
}
