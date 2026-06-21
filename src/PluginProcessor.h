#pragma once

#include <JuceHeader.h>
#include "engine/ProcessorRack.h"
#include "engine/RealtimeStemProcessor.h"

class AuClearAudioProcessor : public juce::AudioProcessor
{
  public:
    AuClearAudioProcessor ();
    ~AuClearAudioProcessor () override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources () override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor () override;
    bool hasEditor () const override;

    const juce::String getName () const override;
    bool acceptsMidi () const override;
    bool producesMidi () const override;
    bool isMidiEffect () const override;
    double getTailLengthSeconds () const override;

    int getNumPrograms () override;
    int getCurrentProgram () override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    ProcessorRack& getRack ()    { return rack; }
    float getCpuLoad () const    { return loadMeasurer.getLoadAsProportion (); }

    // ─── Media player (standalone mode) ──────────────────────────────────────
    bool loadMediaFile (const juce::File& file);
    void unloadMediaFile ();
    bool isMediaFileLoaded () const noexcept
    {
        return mediaFileLoaded.load (std::memory_order_relaxed);
    }
    juce::AudioTransportSource& getTransportSource () { return transportSource; }
    juce::AudioFormatManager&   getFormatManager ()   { return formatManager; }

    // ─── Real-time stem processor (standalone + plugin mode, Phase 4b) ────────
    // loadStemModel / unloadStemModel are message-thread.
    bool                   loadStemModel (const juce::File& onnxPath);
    void                   unloadStemModel ();
    RealtimeStemProcessor& getRealtimeStemProcessor () { return realtimeStemProc; }

  private:
    ProcessorRack rack;
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessLoadMeasurer loadMeasurer;

    juce::AudioFormatManager           formatManager;
    juce::AudioTransportSource         transportSource;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::atomic<bool> mediaFileLoaded{false};
    std::atomic<bool> sourceIsMono{false};

    RealtimeStemProcessor realtimeStemProc;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout ();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuClearAudioProcessor)
};
