#pragma once
#include <JuceHeader.h>
#include "../engine/ProcessorRack.h"
#include <functional>
#include <atomic>

/**
 * Offline file processor. Runs on a dedicated background thread.
 *
 * Set up the local rack on the message thread via prepareLocalRack(), then
 * start processing with startJob(). The background thread calls processBlock()
 * on the isolated rack — it never touches the live plugin rack.
 *
 * For video files, FFmpeg is required (searched on PATH). Audio is extracted,
 * processed, then remuxed with the video stream copied losslessly.
 */
class FileProcessor : public juce::Thread
{
  public:
    using ProgressFn = std::function<void (float progress, juce::String status)>;
    using DoneFn = std::function<void (bool success, juce::String message)>;

    FileProcessor ();
    ~FileProcessor () override;

    /** Must be called on the message thread before startJob().
     *  Opens the file briefly to obtain sample rate, then prepares the local
     *  rack (both prepare() and setState() must be on the message thread).
     *  For video files pass sr=48000, nch=2 — FFmpeg will convert on extract. */
    void prepareLocalRack (const juce::ValueTree& rackState, double sr, int nch);

    /** Starts background processing. prepareLocalRack() must have been called first.
     *  Callbacks are dispatched on the message thread. */
    void startJob (juce::File input, juce::File output, ProgressFn onProgress, DoneFn onDone);

    void cancel ();
    bool isBusy () const noexcept { return isThreadRunning (); }

    // ─── Helpers ─────────────────────────────────────────────────────────────
    static bool isVideoFile (const juce::File& f) noexcept;
    static juce::File suggestOutput (const juce::File& input);

    /** Returns the path to ffmpeg if found on PATH or common locations, else "". */
    static juce::String findFfmpeg ();

  private:
    void run () override;
    bool processAudioFile (juce::File input, juce::File output);
    bool processVideoFile (juce::File input, juce::File output);

    bool runFfmpeg (const juce::StringArray& args);

    void postProgress (float p, const juce::String& msg);
    void postDone (bool ok, const juce::String& msg);

    // Job fields (written on message thread before thread starts)
    juce::File jobInput, jobOutput;
    ProgressFn progressFn;
    DoneFn doneFn;

    // Isolated rack for offline processing
    ProcessorRack localRack;
    int preparedLatency{0};

    std::atomic<bool> cancelFlag{false};
    juce::uint32 lastProgressMs{0};

    static constexpr int kChunkSize = 512;
    static constexpr juce::uint32 kProgressIntervalMs = 80;
};
