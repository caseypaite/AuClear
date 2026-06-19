#pragma once
#include <JuceHeader.h>
#include <vector>
#include <memory>
#include <array>
#include <functional>
#include "RackModule.h"
#include "CommandQueue.h"
#include "MeterBus.h"

/**
 * The rack chain. Owns all RackModule instances; provides lock-free structural
 * editing from the message thread and RT-safe processing on the audio thread.
 *
 * Threading rules:
 *   insertModule / removeModule / moveModule / getModule / numModules / retireOldModules
 *     → message thread only
 *   prepare / reset / processBlock / totalLatencySamples
 *     → audio thread only
 *   inputMeters / outputMeters
 *     → push from audio, drain from GUI
 */
class ProcessorRack
{
public:
    ProcessorRack();
    ~ProcessorRack() = default;

    // -----------------------------------------------------------------------
    // Message thread: structural edits
    // -----------------------------------------------------------------------
    void insertModule (int index, std::unique_ptr<RackModule> module);
    void removeModule (int index);
    void moveModule   (int from, int to);

    int         numModules() const;
    RackModule* getModule   (int index) const;   // raw pointer, read atomics only

    void getState (juce::ValueTree& tree) const;
    void setState (const juce::ValueTree& tree,
                   std::function<std::unique_ptr<RackModule>(ModuleType)> factory);

    // Call periodically on message thread (e.g., editor timer) to free retired modules.
    void retireOldModules();

    // -----------------------------------------------------------------------
    // Audio thread
    // -----------------------------------------------------------------------
    void prepare      (const juce::dsp::ProcessSpec& spec);
    void reset();
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);
    int  totalLatencySamples() const { return cachedLatency.load (std::memory_order_relaxed); }

    // -----------------------------------------------------------------------
    // Metering (audio thread pushes, GUI thread drains)
    // -----------------------------------------------------------------------
    MeterBus& inputMeters()  { return inMeters;  }
    MeterBus& outputMeters() { return outMeters; }

private:
    void drainCommandQueue();

    // Message-thread state
    std::vector<std::unique_ptr<RackModule>> ownedModules; // owns lifetime
    std::vector<RackModule*>                 guiOrder;     // GUI's view of order

    // Audio-thread state (raw pointers borrow from ownedModules)
    std::vector<RackModule*> audioChain;
    juce::dsp::ProcessSpec   currentSpec {};
    juce::AudioBuffer<float> dryBuffer;  // pre-allocated for wet/dry blend

    // Lock-free queues
    CommandQueue commandQueue; // message → audio

    static constexpr int kRetireCap = 64;
    juce::AbstractFifo retireFifo { kRetireCap };      // audio → message
    std::array<RackModule*, kRetireCap> retireBuffer {};

    std::atomic<int> cachedLatency { 0 };

    // Metering
    MeterBus inMeters;
    MeterBus outMeters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorRack)
};
