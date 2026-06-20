#pragma once
#include <JuceHeader.h>
#include <memory>
#include <vector>
#include "OnnxSession.h"

/**
 * Real-time AI inference engine.
 *
 * Wraps an OnnxSession with:
 *   - ReBlocker: adapts host buffer sizes to the model's fixed frame size.
 *   - LagrangeInterpolator pair: resamples to/from the model's 48 kHz SR.
 *   - Dry delay line: synchronises original audio with the processing delay
 *     so "listen to removed" (dry – wet) is phase-accurate.
 *   - Stereo support: two independent channel states share one OnnxSession.
 *
 * Thread model:
 *   loadModel / unloadModel  → message thread
 *   prepare / reset          → audio thread (before processing starts)
 *   process                  → audio thread
 */
class AIEngine
{
  public:
    AIEngine ();
    ~AIEngine ();

    // -----------------------------------------------------------------------
    // Model management (message thread)
    // -----------------------------------------------------------------------
    bool loadModel (const juce::File& modelFile);
    void unloadModel ();
    bool isLoaded () const;

    OnnxSession::Status getStatus () const;
    juce::String        getStatusString () const;

    // -----------------------------------------------------------------------
    // Audio thread
    // -----------------------------------------------------------------------
    void prepare (double sampleRate, int maxBlockSize);
    void reset ();

    // Process stereo (or mono) buffer in-place.
    //   strength       : wet mix in [0, 1]
    //   listenToRemoved: output = original – enhanced (hear just the noise)
    void process (juce::AudioBuffer<float>& buffer, float strength, bool listenToRemoved);

    // Latency in host samples (re-blocking + resampler padding).
    int latencySamples () const { return cachedLatency; }

    // CPU load as a fraction of the available buffer budget (0–∞, where >1 = overload).
    // Written on the audio thread; read on the message thread via relaxed atomic.
    float cpuLoad () const { return cpuLoadFraction.load (std::memory_order_relaxed); }

  private:
    static constexpr double kModelSR    = 48000.0;
    static constexpr int    kModelFrame = 480; // 10 ms at 48 kHz

    void processChannel (int ch, juce::AudioBuffer<float>& buf,
                         float strength, bool listen);

    std::unique_ptr<OnnxSession> session;

    // Per-channel state (heap-allocated to avoid large stack objects)
    struct ChannelState;
    std::vector<std::unique_ptr<ChannelState>> channels;

    double hostSR         = 48000.0;
    int    maxBlock       = 512;
    int    cachedLatency  = kModelFrame; // updated in prepare()
    bool   prepared       = false;

    std::atomic<float> cpuLoadFraction{0.f};
};
