#pragma once
#include <JuceHeader.h>
#include "StemState.h"
#include <array>
#include <atomic>
#include <memory>
#include <vector>

// Real-time Demucs stem separator.
//
// Feeds the live audio stream (from processBlock) through a Demucs ONNX model
// running on a dedicated background thread.  The audio thread and inference
// thread communicate through lock-free AbstractFifo ring buffers:
//
//   Audio thread: process()
//     • pushes the current block into inputRing
//     • wakes the inference thread when a full segment is ready
//     • if outputRing has enough samples: mixes the 4 stems (gain/pan/mute/solo)
//       into 'buffer' with a dry blend; returns true
//     • if not enough output (still buffering or inference running): leaves
//       'buffer' unchanged (the original dry signal plays through); returns false
//
//   Inference thread:
//     • waits for a full segment in inputRing
//     • calls DemucsSession::runSegment()
//     • writes the 4-stem result into outputRing
//
// Latency = segmentSamples() of the loaded model (typically ~7.8 s).  This is
// inherent to the overlap-add source-separation algorithm and is not reported to
// the host to avoid DAW PDC confusion; the status label informs the user.
//
// Thread safety:
//   loadModel() / unloadModel() — message thread; stop the inference thread first
//   prepare() / releaseResources() — message thread
//   process() — audio thread only
//   all other public methods — any thread

#if AUCLEAR_HAS_ONNXRUNTIME

#include "../offline/DemucsSession.h"

class RealtimeStemProcessor : private juce::Thread
{
  public:
    // Per-stem mix params — write from message thread (UI), read from audio thread.
    std::array<StemState, 4> stems;
    std::atomic<float>       dryMix{0.f}; // 0 = pure stems, 1 = pure dry

    enum class Status { Idle, Buffering, Processing, Active, Overrun };

    RealtimeStemProcessor ();
    ~RealtimeStemProcessor () override;

    // ── Message-thread ────────────────────────────────────────────────────────
    bool loadModel (const juce::File& onnxPath);
    void unloadModel ();
    bool isModelLoaded () const noexcept { return modelLoaded.load (std::memory_order_relaxed); }
    juce::File getModelFile () const { return currentModelFile; }

    void setEnabled (bool e);
    bool isEnabled  () const noexcept { return enabled.load (std::memory_order_relaxed); }
    bool isActive   () const noexcept { return isModelLoaded () && isEnabled (); }

    void prepare         (int maxBlockSize, double sampleRate);
    void releaseResources ();

    // Latency introduced by the model segment (informational only — not reported to host).
    int latencySamples () const noexcept { return segLen; }

    juce::String getStatusString () const;
    int          getSourceCount  () const noexcept { return nSrc; }

    std::function<void ()> modelStatusChanged; // called on message thread after load

    // ── Audio-thread ──────────────────────────────────────────────────────────
    // Push the current block as input and (if stem output is available) mix the
    // separated stems into 'buffer'.  Returns true when stems are playing.
    // When false, 'buffer' is left unchanged so the dry signal passes through.
    bool process (juce::AudioBuffer<float>& buffer, int numSamples);

  private:
    void run        () override;
    void resetFifos ();

    juce::CriticalSection            modelLock;
    std::unique_ptr<DemucsSession>   session;
    std::atomic<bool>                modelLoaded{false};
    std::atomic<bool>                enabled{false};
    std::atomic<Status>              status{Status::Idle};
    juce::File                       currentModelFile;

    // Input ring — audio thread writes, inference thread reads.
    juce::AbstractFifo         inputFifo{2};
    juce::AudioBuffer<float>   inputRing;   // [2, inputFifo.getTotalSize()]

    // Output ring — inference thread writes, audio thread reads.
    // Row layout: [src0_L, src0_R, src1_L, src1_R, ..., src3_L, src3_R]
    juce::AbstractFifo         outputFifo{2};
    juce::AudioBuffer<float>   outputRing;  // [nSrc*2, outputFifo.getTotalSize()]

    // Inference thread's private buffers (no concurrency needed).
    std::vector<float> inBuf;   // [2 * segLen]   stereo planar
    std::vector<float> outBuf;  // [nSrc*2*segLen] stems planar

    juce::WaitableEvent wakeSignal;

    // Pre-allocated dry scratch for process() — sized in prepare().
    juce::AudioBuffer<float> dryBuf;

    double sr{48000.0};
    int    maxBlock{512};
    int    segLen{0};
    int    nSrc{4};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RealtimeStemProcessor)
};

#else  // ── Stub when ORT is not available ─────────────────────────────────────

class RealtimeStemProcessor
{
  public:
    std::array<StemState, 4> stems;
    std::atomic<float>       dryMix{0.f};

    enum class Status { Idle, Buffering, Processing, Active, Overrun };

    bool         loadModel (const juce::File&) { return false; }
    void         unloadModel ()                {}
    bool         isModelLoaded () const noexcept { return false; }
    juce::File   getModelFile () const { return currentModelFile; }
    void         setEnabled (bool)             {}
    bool         isEnabled  () const noexcept { return false; }
    bool         isActive   () const noexcept { return false; }
    void         prepare    (int, double)      {}
    void         releaseResources ()           {}
    int          latencySamples () const noexcept { return 0; }
    juce::String getStatusString () const      { return "No ONNX Runtime"; }
    int          getSourceCount  () const noexcept { return 0; }
    bool         process (juce::AudioBuffer<float>&, int) { return false; }

    std::function<void ()> modelStatusChanged;

  private:
    juce::File   currentModelFile;
};

#endif
