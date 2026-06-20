#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "engine/ModuleFactory.h"

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
    formatManager.registerBasicFormats ();
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

    transportSource.prepareToPlay (samplesPerBlock, sampleRate);
}

void AuClearAudioProcessor::releaseResources ()
{
    transportSource.releaseResources ();
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

    const int nSamples = buffer.getNumSamples ();
    const int nCh = buffer.getNumChannels ();

    // When a media file is loaded, replace the device-input buffer with transport audio.
    // AudioTransportSource outputs silence when paused, file audio when playing.
    if (mediaFileLoaded.load (std::memory_order_acquire))
    {
        buffer.clear ();
        juce::AudioSourceChannelInfo info (&buffer, 0, nSamples);
        transportSource.getNextAudioBlock (info);
        // Expand mono source to stereo so rack modules always see two channels.
        if (nCh > 1 && sourceIsMono.load (std::memory_order_relaxed))
            buffer.copyFrom (1, 0, buffer, 0, 0, nSamples);
    }
    else
    {
        for (auto i = getTotalNumInputChannels (); i < nCh; ++i)
            buffer.clear (i, 0, nSamples);
    }

    const bool globalBypass = *apvts.getRawParameterValue ("global_bypass") > 0.5f;

    if (! globalBypass)
        rack.processBlock (buffer, midiMessages);

    // Keep host PDC in sync if latency changed (e.g., module added/removed).
    const int newLat = rack.totalLatencySamples ();
    if (newLat != getLatencySamples ())
        setLatencySamples (newLat);
}

bool AuClearAudioProcessor::loadMediaFile (const juce::File& file)
{
    auto reader =
        std::unique_ptr<juce::AudioFormatReader> (formatManager.createReaderFor (file));
    if (! reader)
        return false;

    const double sourceSr = reader->sampleRate;
    const int sourceCh = (int)reader->numChannels;
    const bool mono = sourceCh == 1;

    // Disconnect before changing the readerSource pointer.
    mediaFileLoaded.store (false, std::memory_order_release);
    transportSource.setSource (nullptr);
    readerSource.reset ();

    readerSource =
        std::make_unique<juce::AudioFormatReaderSource> (reader.release (), true /* ownsReader */);
    // setSource is thread-safe (uses an internal SpinLock).
    transportSource.setSource (readerSource.get (), 0, nullptr, sourceSr,
                               mono ? 1 : std::min (sourceCh, 2));
    transportSource.setPosition (0.0);

    sourceIsMono.store (mono, std::memory_order_relaxed);
    mediaFileLoaded.store (true, std::memory_order_release);
    return true;
}

void AuClearAudioProcessor::unloadMediaFile ()
{
    mediaFileLoaded.store (false, std::memory_order_release);
    transportSource.stop ();
    transportSource.setSource (nullptr);
    readerSource.reset ();
    sourceIsMono.store (false, std::memory_order_relaxed);
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
            rack.setState (rackTree, makeModule);
        }
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter ()
{
    return new AuClearAudioProcessor ();
}
