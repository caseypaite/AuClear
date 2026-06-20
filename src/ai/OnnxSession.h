#pragma once
#include <string>
#include <memory>

/**
 * Thin wrapper around an ONNX Runtime inference session.
 *
 * Supports models with a single audio input and output:
 *   input  "input"  — shape [1, frameSize], dtype float32
 *   output "output" — shape [1, frameSize], dtype float32
 *
 * frameSize is discovered from the model at load time.
 * Designed for RT-safe use after loadModel() and preWarm():
 *   - runFrame() performs no dynamic memory allocation.
 *   - All ORT types are hidden behind pImpl so this header is ONNX-free.
 */
class OnnxSession
{
  public:
    enum class Status
    {
        Idle,
        Ready,
        Error
    };

    OnnxSession ();
    ~OnnxSession ();

    // Load a .onnx model from an absolute path on the message thread.
    // Returns true on success; false with getLastError() set otherwise.
    bool loadModel (const std::string& modelPath,
                    const std::string& inputName  = "input",
                    const std::string& outputName = "output");

    void unloadModel ();

    bool isLoaded () const;
    Status getStatus () const;
    const std::string& getLastError () const;

    // Frame size discovered from the model's input shape (dim 1). 0 if not loaded.
    int frameSize () const;

    // Run one frame. in/out must point to frameSize() floats.
    // Audio thread only; no allocations.
    bool runFrame (const float* in, float* out);

    // Run a dummy frame to trigger JIT compilation before the first real call.
    bool preWarm ();

  private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};
