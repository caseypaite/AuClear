#pragma once
#include <JuceHeader.h>
#include "../engine/ProcessorRack.h"
#include <atomic>
#include <functional>
#include <memory>

enum class JobType
{
    DspProcess,   // run through a pre-prepared ProcessorRack
    DemucsStems,  // 4-stem separation (Phase 4b)
    DeClip,       // offline declip  (Phase 4c)
    DeCrackle,    // offline decrackle (Phase 4c)
};

enum class JobState
{
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled,
};

/**
 * Descriptor for a single offline processing job.
 *
 * Ownership and threading:
 *   - Submit on the message thread via OfflineJobManager::submit().
 *   - For DspProcess: construct + prepare `rack` on the message thread before submit().
 *   - state / progress / cancelRequested are atomic; safe to poll from any thread.
 *   - statusMessage is guarded by statusLock; use getStatusMessage() / setStatusMessage().
 *   - onProgress / onDone are dispatched on the message thread by the manager.
 *
 * Non-copyable and non-movable (atomics + CriticalSection).
 */
struct OfflineJob
{
    using ProgressFn = std::function<void (float progress, juce::String status)>;
    using DoneFn = std::function<void (bool success, juce::String message)>;

    // ── Identity ──────────────────────────────────────────────────────────────
    juce::Uuid id;
    JobType type{JobType::DspProcess};

    // ── Files ─────────────────────────────────────────────────────────────────
    juce::File inputFile;
    juce::File outputFile;  // primary output (audio / video)
    juce::File outputDir;   // multi-file output (stems)

    // ── Params ────────────────────────────────────────────────────────────────
    // Flat ValueTree for job-specific parameters; used as part of the cache key.
    juce::ValueTree params{"Params"};

    // ── DspProcess-specific ───────────────────────────────────────────────────
    // Caller constructs and prepares this on the message thread, then moves it
    // into the job before submit(). The pool thread only calls processBlock().
    std::unique_ptr<ProcessorRack> rack;
    int rackLatency{0};
    double sampleRate{48000.0};
    int numChannels{2};

    // ── Live state ────────────────────────────────────────────────────────────
    std::atomic<JobState> state{JobState::Pending};
    std::atomic<float> progress{0.f};

    mutable juce::CriticalSection statusLock;
    juce::String statusMessage; // guarded by statusLock

    juce::String getStatusMessage () const
    {
        juce::ScopedLock sl (statusLock);
        return statusMessage;
    }
    void setStatusMessage (const juce::String& msg)
    {
        juce::ScopedLock sl (statusLock);
        statusMessage = msg;
    }

    // ── Callbacks (dispatched on the message thread) ──────────────────────────
    ProgressFn onProgress;
    DoneFn onDone;

    // ── Cancellation ──────────────────────────────────────────────────────────
    std::atomic<bool> cancelRequested{false};

    OfflineJob () = default;
    OfflineJob (const OfflineJob&) = delete;
    OfflineJob& operator= (const OfflineJob&) = delete;
    OfflineJob (OfflineJob&&) = delete;
    OfflineJob& operator= (OfflineJob&&) = delete;
};
