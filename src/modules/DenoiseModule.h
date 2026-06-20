#pragma once
#include <JuceHeader.h>
#include <atomic>
#include "../engine/RackModule.h"
#include "../ai/AIEngine.h"

/**
 * Real-time neural denoiser / dereverber.
 *
 * Uses AIEngine (ONNX Runtime + ReBlocker) to run a DeepFilterNet3-compatible
 * model at 48 kHz with any host sample rate.  Falls through cleanly when no
 * model is loaded so the rack chain is never broken.
 *
 * Parameters (atomic, safe from both threads):
 *   strength        — wet/dry blend (0 = dry, 1 = full denoise)
 *   listenToRemoved — output = original – enhanced (audition removed noise)
 *
 * UI notes: model path is set via setModelFile() on the message thread.
 * modelStatusChanged is called after each load attempt so the panel can update.
 */
class DenoiseModule : public RackModule
{
  public:
    DenoiseModule ();
    ~DenoiseModule () override = default;

    // -----------------------------------------------------------------------
    // RackModule interface
    // -----------------------------------------------------------------------
    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset () override;
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;
    int latencySamples () const override { return engine.latencySamples (); }

    ModuleType type () const override { return ModuleType::Denoise; }
    juce::String name () const override { return "Denoise"; }
    void getState (juce::ValueTree& tree) const override;
    void setState (const juce::ValueTree& tree) override;

    // -----------------------------------------------------------------------
    // Model management (message thread)
    // -----------------------------------------------------------------------
    void setModelFile (const juce::File& file);
    juce::File getModelFile () const { return currentModelFile; }
    juce::String getStatusString () const { return engine.getStatusString (); }
    bool isModelLoaded () const { return engine.isLoaded (); }
    float getCpuLoad () const { return engine.cpuLoad (); }

    std::function<void ()> modelStatusChanged; // called on message thread after load

    // -----------------------------------------------------------------------
    // Parameters (atomic — safe to write from GUI, read from audio thread)
    // -----------------------------------------------------------------------
    std::atomic<float> strength{1.0f};
    std::atomic<bool> listenToRemoved{false};

  private:
    AIEngine engine;
    juce::File currentModelFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DenoiseModule)
};
