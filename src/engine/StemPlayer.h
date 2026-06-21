#pragma once
#include <JuceHeader.h>
#include "StemState.h"
#include <array>
#include <atomic>
#include <memory>

// Plays back 4 pre-separated stem WAV files in lock-step, mixing them
// with per-stem gain / pan / mute / solo and an overall dry blend.
//
// Thread model:
//   Message thread  — loadStems(), unloadStems(), prepare(), releaseResources()
//   Audio thread    — fillNextAudioBlock(), setPlaying(), syncPosition()
//   Either thread   — isActive(), getPositionSeconds()
//
// The internal CriticalSection serialises load/unload against audio-thread reads.
// The audio thread uses ScopedTryLock so it never blocks; it outputs silence for
// the one block that overlaps a load operation.

class StemPlayer
{
  public:
    // Per-stem mix parameters — write from message thread, read from audio thread.
    std::array<StemState, 4> stems;
    std::atomic<float>       dryMix{0.0f};  // 0 = pure stems, 1 = pure dry

    StemPlayer () = default;
    ~StemPlayer () { releaseResources (); }

    // ── Message-thread API ────────────────────────────────────────────────────

    // Opens stem WAVs and marks the player active. Must be called on the message
    // thread; can be called while the audio thread is running (uses the lock).
    // Returns false if any file cannot be opened.
    bool loadStems (const std::array<juce::File, 4>& files,
                    juce::AudioFormatManager& fmt);

    void unloadStems ();

    // Called by prepareToPlay — must be called before the first audio block.
    void prepare (int samplesPerBlock, double sampleRate);
    void releaseResources ();

    // ── Audio-thread API ──────────────────────────────────────────────────────

    // Gate: when false, fillNextAudioBlock returns silence without advancing readers.
    void setPlaying (bool playing) noexcept { isPlaying.store (playing, std::memory_order_relaxed); }

    // Detects position drift > kSyncThresholdSec and seeks all readers to match.
    // Call at the top of each processBlock before fillNextAudioBlock.
    void syncPosition (double transportPositionSeconds) noexcept;

    // Mixes the 4 stems into 'buffer' (which must already be cleared) and blends
    // with 'dry'. 'numSamples' must be <= the samplesPerBlock passed to prepare().
    void fillNextAudioBlock (juce::AudioBuffer<float>& buffer,
                             const juce::AudioBuffer<float>& dry,
                             int numSamples) noexcept;

    // ── Thread-safe queries ───────────────────────────────────────────────────
    bool   isActive () const noexcept { return active.load (std::memory_order_relaxed); }
    double getPositionSeconds () const noexcept;

  private:
    static constexpr double kSyncThresholdSec = 0.5;

    juce::CriticalSection lock;
    std::array<std::unique_ptr<juce::AudioFormatReader>,       4> readers;
    std::array<std::unique_ptr<juce::AudioFormatReaderSource>, 4> sources;

    juce::AudioBuffer<float> stemBuf; // pre-allocated scratch (2 × maxBlockSize)

    std::atomic<bool>    active{false};
    std::atomic<bool>    isPlaying{false};
    std::atomic<int64_t> displayPos{0}; // updated by audio thread for position query

    double preparedSr{0.0};
    int    preparedBlock{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StemPlayer)
};
