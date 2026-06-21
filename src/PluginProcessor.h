#pragma once

#include <JuceHeader.h>
#include "engine/ProcessorRack.h"
#include "engine/StemPlayer.h"
#include <array>

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

    ProcessorRack& getRack () { return rack; }
    float getCpuLoad () const { return loadMeasurer.getLoadAsProportion (); }

    // ─── Media player (standalone mode) ──────────────────────────────────────
    bool loadMediaFile (const juce::File& file);
    void unloadMediaFile ();

    bool isMediaFileLoaded () const noexcept
    {
        return mediaFileLoaded.load (std::memory_order_relaxed);
    }

    juce::AudioTransportSource& getTransportSource () { return transportSource; }
    juce::AudioFormatManager&   getFormatManager ()   { return formatManager; }

    // ─── Stem player (standalone mode, Phase 4b) ──────────────────────────────
    // Call on the message thread after Demucs separation finishes.
    // Files must be in stem order: 0=drums, 1=bass, 2=other, 3=vocals.
    bool loadStems (const std::array<juce::File, 4>& files);
    void unloadStems ();
    StemPlayer& getStemPlayer () { return stemPlayer; }

  private:
    ProcessorRack rack;
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessLoadMeasurer loadMeasurer;

    juce::AudioFormatManager           formatManager;
    juce::AudioTransportSource         transportSource;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::atomic<bool> mediaFileLoaded{false};
    std::atomic<bool> sourceIsMono{false};

    StemPlayer              stemPlayer;
    juce::AudioBuffer<float> dryWorkBuffer; // pre-allocated scratch for stem dry blend

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout ();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuClearAudioProcessor)
};
