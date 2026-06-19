#pragma once
#include <JuceHeader.h>
#include <atomic>

enum class ModuleType : int
{
    Gain = 0,
    ParametricEQ = 1,
    Gate = 2,
    Compressor = 3,
    Limiter = 4,
    Reverb = 5,
    Delay = 6,
    Saturator = 7,
    Utility = 8,
};

/**
 * Abstract base for every processing unit in the rack chain.
 * Instance lifecycle: created and destroyed on the message thread.
 * prepare() and process() are called on the audio thread after handoff via CommandQueue.
 * Atomic members (bypassed, wetDry) are safe to read/write from either thread.
 */
class RackModule
{
  public:
    virtual ~RackModule () = default;

    virtual void prepare (const juce::dsp::ProcessSpec& spec) = 0;
    virtual void reset () = 0;
    virtual void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) = 0;
    virtual int latencySamples () const { return 0; }

    virtual ModuleType type () const = 0;
    virtual juce::String name () const = 0;
    virtual void getState (juce::ValueTree& tree) const = 0;
    virtual void setState (const juce::ValueTree& tree) = 0;

    // Written by GUI thread, read by audio thread — both via relaxed atomics.
    std::atomic<bool> bypassed{false};
    std::atomic<float> wetDry{1.0f};

    // Immutable after construction — safe to read from any thread.
    const juce::String uid = juce::Uuid ().toString ();
};
