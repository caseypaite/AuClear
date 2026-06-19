#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "modules/GainModule.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout AuClearAudioProcessor::createParameterLayout ()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    // Phase 1: global bypass only. Per-module params are managed by each module's
    // own state and atomic members, not by APVTS, to support dynamic chains.
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID{"global_bypass", 1},
                                                            "Global Bypass", false));
    return layout;
}

AuClearAudioProcessor::AuClearAudioProcessor ()
    : AudioProcessor (BusesProperties ()
                          .withInput ("Input", juce::AudioChannelSet::stereo (), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo (), true)),
      apvts (*this, nullptr, "AuClearState", createParameterLayout ())
{
}

AuClearAudioProcessor::~AuClearAudioProcessor () = default;

//==============================================================================
void AuClearAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    loadMeasurer.reset (sampleRate, samplesPerBlock);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32)samplesPerBlock;
    spec.numChannels = (juce::uint32)getTotalNumOutputChannels ();

    rack.prepare (spec);
    setLatencySamples (rack.totalLatencySamples ());
}

void AuClearAudioProcessor::releaseResources ()
{
    rack.reset ();
}

bool AuClearAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet ();
    if (out != juce::AudioChannelSet::mono () && out != juce::AudioChannelSet::stereo ())
        return false;
    return layouts.getMainInputChannelSet () == out;
}

void AuClearAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::AudioProcessLoadMeasurer::ScopedTimer loadTimer (loadMeasurer);

    for (auto i = getTotalNumInputChannels (); i < getTotalNumOutputChannels (); ++i)
        buffer.clear (i, 0, buffer.getNumSamples ());

    const bool globalBypass = *apvts.getRawParameterValue ("global_bypass") > 0.5f;

    if (!globalBypass)
        rack.processBlock (buffer, midiMessages);

    // Keep host PDC in sync if latency changed (e.g., module added/removed).
    const int newLat = rack.totalLatencySamples ();
    if (newLat != getLatencySamples ())
        setLatencySamples (newLat);
}

//==============================================================================
juce::AudioProcessorEditor* AuClearAudioProcessor::createEditor ()
{
    return new AuClearAudioProcessorEditor (*this);
}

bool AuClearAudioProcessor::hasEditor () const
{
    return true;
}

//==============================================================================
const juce::String AuClearAudioProcessor::getName () const
{
    return JucePlugin_Name;
}
bool AuClearAudioProcessor::acceptsMidi () const
{
    return false;
}
bool AuClearAudioProcessor::producesMidi () const
{
    return false;
}
bool AuClearAudioProcessor::isMidiEffect () const
{
    return false;
}
double AuClearAudioProcessor::getTailLengthSeconds () const
{
    return 0.0;
}

//==============================================================================
int AuClearAudioProcessor::getNumPrograms ()
{
    return 1;
}
int AuClearAudioProcessor::getCurrentProgram ()
{
    return 0;
}
void AuClearAudioProcessor::setCurrentProgram (int) {}
const juce::String AuClearAudioProcessor::getProgramName (int)
{
    return {};
}
void AuClearAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void AuClearAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState ();

    juce::ValueTree rackTree ("Rack");
    rack.getState (rackTree);
    state.appendChild (rackTree, nullptr);

    if (auto xml = state.createXml ())
        copyXmlToBinary (*xml, destData);
}

void AuClearAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (!state.isValid ())
            return;

        apvts.replaceState (state);

        auto rackTree = state.getChildWithName ("Rack");
        if (rackTree.isValid ())
        {
            rack.setState (rackTree,
                           [] (ModuleType t) -> std::unique_ptr<RackModule>
                           {
                               switch (t)
                               {
                               case ModuleType::Gain:
                                   return std::make_unique<GainModule> ();
                               default:
                                   return nullptr;
                               }
                           });
        }
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter ()
{
    return new AuClearAudioProcessor ();
}
