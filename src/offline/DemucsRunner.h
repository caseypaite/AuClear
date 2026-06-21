#pragma once
#include "DemucsSession.h"
#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include <string>

/**
 * Offline stem separator using a DemucsSession.
 *
 * Processes an audio file through an overlap-add chunking loop:
 *   1. Read input file, resample to model SR (if needed).
 *   2. Divide into overlapping segments of session.segmentSamples().
 *   3. For each segment: run session.runSegment(), apply cosine-fade weight.
 *   4. Accumulate weighted stems and normalize by accumulated weights.
 *   5. Resample each stem back to the original file SR.
 *   6. Write one WAV file per stem into outputDir.
 *
 * Output file names: <inputStem>_<sourceName>.wav
 * (e.g. "track_vocals.wav", "track_drums.wav")
 *
 * Thread model: all methods are called from a single background thread.
 * The session is used exclusively by this runner during a run() call.
 */
class DemucsRunner
{
  public:
    struct Callbacks
    {
        std::function<bool ()> shouldCancel;
        std::function<void (float progress, juce::String status)> onProgress;
    };

    explicit DemucsRunner (DemucsSession& session);

    /**
     * Separate inputFile into stems written to outputDir.
     * Returns the list of written files (empty on failure).
     * Call getLastError() for diagnostics on failure.
     */
    juce::Array<juce::File> run (const juce::File& inputFile,
                                 const juce::File& outputDir,
                                 const Callbacks& callbacks);

    const juce::String& getLastError () const { return lastError; }

    // Overlap fraction [0, 0.5). Default 0.25 (25%).
    float overlapFraction = 0.25f;

  private:
    bool resampleChannel (const float* src, int nSrc, double srcRate,
                          float* dst, int nDst, double dstRate);

    DemucsSession& session;
    juce::String lastError;
};
