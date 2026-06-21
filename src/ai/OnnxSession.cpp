#include "OnnxSession.h"

#if AUCLEAR_HAS_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

#include <array>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
struct OnnxSession::Impl
{
#if AUCLEAR_HAS_ONNXRUNTIME
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "AuClear"};
    std::unique_ptr<Ort::Session> session;
    Ort::MemoryInfo memInfo{Ort::MemoryInfo::CreateCpu (OrtArenaAllocator, OrtMemTypeDefault)};
#endif

    std::string inputName;
    std::string outputName;
    int fSize = 0;
    Status status = Status::Idle;
    std::string lastError;

    // Pre-allocated buffers — no alloc in runFrame after init.
    std::vector<float> inBuf;
    std::vector<float> outBuf;
    std::vector<int64_t> shape;

    // Atomic "safe to call runFrame" flag. Set by makeReady() AFTER loadModel()
    // + preWarm() complete so the audio thread never races on session/inBuf/outBuf.
    // Cleared at the START of unloadModel() so the audio thread stops using the
    // session before we free it.
    std::atomic<bool> ready{false};
};

// ---------------------------------------------------------------------------
OnnxSession::OnnxSession () : pImpl (std::make_unique<Impl> ()) {}
OnnxSession::~OnnxSession () = default;

bool OnnxSession::loadModel (const std::string& modelPath, const std::string& inputName,
                             const std::string& outputName)
{
    unloadModel ();
    auto& impl = *pImpl;

#if !AUCLEAR_HAS_ONNXRUNTIME
    (void)modelPath;
    (void)inputName;
    (void)outputName;
    impl.lastError = "Built without ONNX Runtime support.";
    impl.status = Status::Error;
    return false;
#else
    try
    {
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads (1);
        opts.SetInterOpNumThreads (1);
        opts.SetGraphOptimizationLevel (GraphOptimizationLevel::ORT_ENABLE_ALL);

#ifdef _WIN32
        std::wstring wpath (modelPath.begin (), modelPath.end ());
        impl.session = std::make_unique<Ort::Session> (impl.env, wpath.c_str (), opts);
#else
        impl.session = std::make_unique<Ort::Session> (impl.env, modelPath.c_str (), opts);
#endif
        impl.inputName = inputName;
        impl.outputName = outputName;

        // Discover frame size from model input shape [batch, frameSize]
        Ort::AllocatorWithDefaultOptions alloc;
        size_t numInputs = impl.session->GetInputCount ();
        size_t numOutputs = impl.session->GetOutputCount ();

        if (numInputs != 1 || numOutputs != 1)
        {
            std::string msg = "Model must have exactly 1 input and 1 output. Found " 
                              + std::to_string (numInputs) + " inputs, " 
                              + std::to_string (numOutputs) + " outputs. ";
            
            msg += "Inputs: [";
            for (size_t i = 0; i < numInputs; ++i)
            {
                auto nameAlloc = impl.session->GetInputNameAllocated (i, alloc);
                auto sh = impl.session->GetInputTypeInfo (i).GetTensorTypeAndShapeInfo ().GetShape ();
                msg += (i > 0 ? ", " : "") + std::string (nameAlloc.get ()) + "(";
                for (size_t d = 0; d < sh.size (); ++d)
                    msg += (d > 0 ? "x" : "") + std::to_string (sh[d]);
                msg += ")";
            }
            msg += "]. Outputs: [";
            for (size_t i = 0; i < numOutputs; ++i)
            {
                auto nameAlloc = impl.session->GetOutputNameAllocated (i, alloc);
                auto sh = impl.session->GetOutputTypeInfo (i).GetTensorTypeAndShapeInfo ().GetShape ();
                msg += (i > 0 ? ", " : "") + std::string (nameAlloc.get ()) + "(";
                for (size_t d = 0; d < sh.size (); ++d)
                    msg += (d > 0 ? "x" : "") + std::to_string (sh[d]);
                msg += ")";
            }
            msg += "].";
            
            impl.lastError = msg;
            impl.status = Status::Error;
            impl.session.reset ();
            return false;
        }

        // Get actual input & output names dynamically
        auto inputNameAllocated = impl.session->GetInputNameAllocated (0, alloc);
        impl.inputName = inputNameAllocated.get ();

        auto outputNameAllocated = impl.session->GetOutputNameAllocated (0, alloc);
        impl.outputName = outputNameAllocated.get ();

        auto typeInfo = impl.session->GetInputTypeInfo (0);
        auto shape = typeInfo.GetTensorTypeAndShapeInfo ().GetShape ();

        if (shape.size () < 2)
        {
            impl.lastError = "Model input shape must have at least 2 dimensions.";
            impl.status = Status::Error;
            impl.session.reset ();
            return false;
        }

        // The frame size is the last dimension of the input shape
        int fSizeIdx = (int)shape.size () - 1;
        int64_t discoveredFrame = shape[(size_t)fSizeIdx];
        if (discoveredFrame <= 0)
        {
            impl.lastError = "Invalid frame size discovered: " + std::to_string (discoveredFrame);
            impl.status = Status::Error;
            impl.session.reset ();
            return false;
        }

        impl.fSize = static_cast<int> (discoveredFrame);
        
        // Preserve the full shape but force batch=1
        impl.shape = shape;
        impl.shape[0] = 1;

        impl.inBuf.assign ((size_t)impl.fSize, 0.f);
        impl.outBuf.assign ((size_t)impl.fSize, 0.f);
        impl.status = Status::Ready;
        return true;
    }
    catch (const Ort::Exception& e)
    {
        impl.lastError = std::string ("ORT: ") + e.what ();
        impl.status = Status::Error;
        return false;
    }
    catch (const std::exception& e)
    {
        impl.lastError = e.what ();
        impl.status = Status::Error;
        return false;
    }
#endif
}

void OnnxSession::unloadModel ()
{
    // Clear the audio-thread flag first so runFrame() returns false immediately.
    // The store with seq_cst + the sleep below gives any in-progress runFrame()
    // time to exit before we free the session.
    pImpl->ready.store (false, std::memory_order_seq_cst);
    std::this_thread::sleep_for (std::chrono::milliseconds (50)); // > one audio-callback at any practical block size

#if AUCLEAR_HAS_ONNXRUNTIME
    pImpl->session.reset ();
#endif
    pImpl->fSize = 0;
    pImpl->status = Status::Idle;
    pImpl->lastError = {};
}

bool OnnxSession::isLoaded () const
{
    return pImpl->ready.load (std::memory_order_acquire);
}

void OnnxSession::makeReady ()
{
    pImpl->ready.store (true, std::memory_order_release);
}

OnnxSession::Status OnnxSession::getStatus () const
{
    return pImpl->status;
}
const std::string& OnnxSession::getLastError () const
{
    return pImpl->lastError;
}
int OnnxSession::frameSize () const
{
    return pImpl->fSize;
}

bool OnnxSession::runFrame (const float* in, float* out)
{
#if !AUCLEAR_HAS_ONNXRUNTIME
    (void)in;
    (void)out;
    return false;
#else
    // Acquire-load: if ready is false the session pointer may be null or stale.
    if (! pImpl->ready.load (std::memory_order_acquire))
        return false;

    auto& impl = *pImpl;
    if (! impl.session)
        return false;

    std::memcpy (impl.inBuf.data (), in, (size_t)impl.fSize * sizeof (float));

    const char* inName = impl.inputName.c_str ();
    const char* outName = impl.outputName.c_str ();

    Ort::Value inTensor =
        Ort::Value::CreateTensor<float> (impl.memInfo, impl.inBuf.data (), (size_t)impl.fSize,
                                         impl.shape.data (), impl.shape.size ());

    try
    {
        auto outputs =
            impl.session->Run (Ort::RunOptions{nullptr}, &inName, &inTensor, 1, &outName, 1);

        const float* ptr = outputs[0].GetTensorData<float> ();
        std::memcpy (out, ptr, (size_t)impl.fSize * sizeof (float));
        return true;
    }
    catch (...)
    {
        return false;
    }
#endif
}

bool OnnxSession::preWarm ()
{
    // preWarm() is called on the message thread before makeReady(), so the
    // audio-thread-facing ready flag is not yet set.  Check status directly
    // and bypass the ready guard in runFrame() by calling the ORT session inline.
#if !AUCLEAR_HAS_ONNXRUNTIME
    return false;
#else
    auto& impl = *pImpl;
    if (impl.status != Status::Ready || ! impl.session)
        return false;

    std::vector<float> dummy ((size_t)impl.fSize, 0.f);
    std::vector<float> dummyOut ((size_t)impl.fSize, 0.f);

    std::memcpy (impl.inBuf.data (), dummy.data (), (size_t)impl.fSize * sizeof (float));
    const char* inName  = impl.inputName.c_str ();
    const char* outName = impl.outputName.c_str ();
    try
    {
        Ort::Value inTensor = Ort::Value::CreateTensor<float> (
            impl.memInfo, impl.inBuf.data (), (size_t)impl.fSize,
            impl.shape.data (), impl.shape.size ());
        auto outputs = impl.session->Run (Ort::RunOptions{nullptr}, &inName, &inTensor, 1,
                                          &outName, 1);
        const float* ptr = outputs[0].GetTensorData<float> ();
        std::memcpy (dummyOut.data (), ptr, (size_t)impl.fSize * sizeof (float));
        return true;
    }
    catch (...)
    {
        return false;
    }
#endif
}
