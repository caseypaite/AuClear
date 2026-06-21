#pragma once
#include <memory>
#include <string>
#include <vector>

/**
 * ORT inference session for Demucs-family source-separation models.
 *
 * Model contract (enforced at loadModel() time):
 *   Input  "input"  — shape [1, 2, segmentSamples], dtype float32
 *   Output "output" — shape [1, numSources, 2, segmentSamples], dtype float32
 *
 * Optional ONNX model-metadata keys (written by tools/export_demucs.py):
 *   "demucs_samplerate" — e.g. "44100"
 *   "demucs_sources"    — comma-separated, e.g. "drums,bass,other,vocals"
 *   "demucs_segment"    — segment length in samples, e.g. "343980"
 *
 * If metadata is absent, defaults are: 44100 Hz, 4 sources ["drums","bass","other","vocals"],
 * segment discovered from the model's static input shape (must not be -1 if no metadata).
 *
 * GPU execution providers are attempted in order (CUDA, CoreML) and silently
 * skipped if not installed; falls back to CPU.
 *
 * Not thread-safe. Call runSegment() from a single background thread only.
 */
class DemucsSession
{
  public:
    DemucsSession ();
    ~DemucsSession ();

    bool loadModel (const std::string& modelPath);
    void unloadModel ();

    bool isLoaded () const;
    const std::string& getLastError () const;

    int segmentSamples () const;
    int numSources () const;
    double sampleRate () const;
    const std::vector<std::string>& sourceNames () const;

    /**
     * Run one segment.
     *   in  — planar float [2 * segmentSamples]: ch0[0..N-1], ch1[0..N-1]
     *   out — planar float [numSources * 2 * segmentSamples]:
     *           src0_ch0[0..N-1], src0_ch1[0..N-1], src1_ch0[0..N-1], ...
     * Returns false on inference error (check getLastError()).
     */
    bool runSegment (const float* in, float* out);

  private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};
