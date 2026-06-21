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
    std::string inputName2;
    std::string outputName;
    std::string outputName2;
    int fSize = 0;
    Status status = Status::Idle;
    std::string lastError;
    bool isStereoModel = false;

    // Pre-allocated buffers — no alloc in runFrame after init.
    std::vector<float> inBuf;
    std::vector<float> inBuf2;
    std::vector<float> outBuf;
    std::vector<float> outBuf2;
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

bool OnnxSession::loadModel (const std::string& modelPath, int targetFrameSize)
{
    unloadModel ();
    auto& impl = *pImpl;

#if !AUCLEAR_HAS_ONNXRUNTIME
    (void)modelPath;
    (void)targetFrameSize;
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

        // Discover frame size from model input shape [batch, frameSize]
        Ort::AllocatorWithDefaultOptions alloc;
        size_t numInputs = impl.session->GetInputCount ();
        size_t numOutputs = impl.session->GetOutputCount ();

        if (targetFrameSize <= 0)
            targetFrameSize = 480;

        // 2. Discover audio inputs and outputs (filtering out auxiliary states)
        std::vector<std::string> audioInputs;
        std::vector<std::string> audioOutputs;

        for (size_t i = 0; i < numInputs; ++i)
        {
            auto sh = impl.session->GetInputTypeInfo (i).GetTensorTypeAndShapeInfo ().GetShape ();
            if (sh.size () >= 1 && (sh.back () == targetFrameSize || sh.back () <= 0))
            {
                auto nameAlloc = impl.session->GetInputNameAllocated (i, alloc);
                audioInputs.push_back (nameAlloc.get ());
            }
        }

        for (size_t i = 0; i < numOutputs; ++i)
        {
            auto sh = impl.session->GetOutputTypeInfo (i).GetTensorTypeAndShapeInfo ().GetShape ();
            if (sh.size () >= 1 && (sh.back () == targetFrameSize || sh.back () <= 0))
            {
                auto nameAlloc = impl.session->GetOutputNameAllocated (i, alloc);
                audioOutputs.push_back (nameAlloc.get ());
            }
        }

        // 3. Match inputs/outputs to decide Mono or Stereo mode
        if (audioInputs.size () >= 2 && audioOutputs.size () >= 2)
        {
            impl.isStereoModel = true;
            impl.inputName = audioInputs[0];
            impl.inputName2 = audioInputs[1];
            impl.outputName = audioOutputs[0];
            impl.outputName2 = audioOutputs[1];
        }
        else if (!audioInputs.empty () || !audioOutputs.empty () || (numInputs >= 1 && numOutputs >= 1))
        {
            impl.isStereoModel = false;
            
            // Pick input: use first audio input if available, else input 0
            if (!audioInputs.empty ())
            {
                impl.inputName = audioInputs[0];
            }
            else if (numInputs >= 1)
            {
                auto nameAlloc = impl.session->GetInputNameAllocated (0, alloc);
                impl.inputName = nameAlloc.get ();
            }

            // Pick output: use first audio output if available, else output 0
            if (!audioOutputs.empty ())
            {
                impl.outputName = audioOutputs[0];
            }
            else if (numOutputs >= 1)
            {
                auto nameAlloc = impl.session->GetOutputNameAllocated (0, alloc);
                impl.outputName = nameAlloc.get ();
            }
        }
        else
        {
            std::string msg = "Could not map model to mono or stereo audio. "
                              "Found " + std::to_string (numInputs) + " inputs (" + std::to_string (audioInputs.size ()) + " audio), "
                              + std::to_string (numOutputs) + " outputs (" + std::to_string (audioOutputs.size ()) + " audio). "
                              "Target frame size: " + std::to_string (targetFrameSize) + ". ";
            
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

        impl.fSize = targetFrameSize;

        // Set up shape vector: force batch=1 if 2D or more, and preserve the original input's shape dimensions
        size_t selectedInputIdx = 0;
        for (size_t i = 0; i < numInputs; ++i)
        {
            auto nameAlloc = impl.session->GetInputNameAllocated (i, alloc);
            if (std::string(nameAlloc.get()) == impl.inputName)
            {
                selectedInputIdx = i;
                break;
            }
        }

        auto typeInfo = impl.session->GetInputTypeInfo (selectedInputIdx);
        auto shape = typeInfo.GetTensorTypeAndShapeInfo ().GetShape ();
        impl.shape = shape;

        if (impl.shape.empty ())
        {
            impl.shape = { 1, (int64_t)impl.fSize };
        }
        else if (impl.shape.size () >= 2)
        {
            impl.shape[0] = 1;
            if (impl.shape.back () <= 0)
                impl.shape[impl.shape.size () - 1] = impl.fSize;
        }
        else // 1D tensor
        {
            if (impl.shape[0] <= 0)
                impl.shape[0] = impl.fSize;
        }

        impl.inBuf.assign ((size_t)impl.fSize, 0.f);
        impl.outBuf.assign ((size_t)impl.fSize, 0.f);
        if (impl.isStereoModel)
        {
            impl.inBuf2.assign ((size_t)impl.fSize, 0.f);
            impl.outBuf2.assign ((size_t)impl.fSize, 0.f);
        }
        else
        {
            impl.inBuf2.clear ();
            impl.outBuf2.clear ();
            impl.inputName2.clear ();
            impl.outputName2.clear ();
        }
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

    if (impl.isStereoModel)
    {
        return runFrame (in, in, out, out);
    }

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

bool OnnxSession::runFrame (const float* inL, const float* inR, float* outL, float* outR)
{
#if !AUCLEAR_HAS_ONNXRUNTIME
    (void)inL; (void)inR; (void)outL; (void)outR;
    return false;
#else
    if (! pImpl->ready.load (std::memory_order_acquire))
        return false;

    auto& impl = *pImpl;
    if (! impl.session || ! impl.isStereoModel)
        return false;

    std::memcpy (impl.inBuf.data (), inL, (size_t)impl.fSize * sizeof (float));
    std::memcpy (impl.inBuf2.data (), inR, (size_t)impl.fSize * sizeof (float));

    const char* inNames[2] = { impl.inputName.c_str (), impl.inputName2.c_str () };
    const char* outNames[2] = { impl.outputName.c_str (), impl.outputName2.c_str () };

    Ort::Value inTensors[2] = {
        Ort::Value::CreateTensor<float> (impl.memInfo, impl.inBuf.data (), (size_t)impl.fSize,
                                         impl.shape.data (), impl.shape.size ()),
        Ort::Value::CreateTensor<float> (impl.memInfo, impl.inBuf2.data (), (size_t)impl.fSize,
                                         impl.shape.data (), impl.shape.size ())
    };

    try
    {
        auto outputs =
            impl.session->Run (Ort::RunOptions{nullptr}, inNames, inTensors, 2, outNames, 2);

        const float* ptrL = outputs[0].GetTensorData<float> ();
        const float* ptrR = outputs[1].GetTensorData<float> ();
        std::memcpy (outL, ptrL, (size_t)impl.fSize * sizeof (float));
        std::memcpy (outR, ptrR, (size_t)impl.fSize * sizeof (float));
        return true;
    }
    catch (...)
    {
        return false;
    }
#endif
}

bool OnnxSession::isStereo () const
{
    return pImpl->isStereoModel;
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
    if (impl.isStereoModel)
        std::memcpy (impl.inBuf2.data (), dummy.data (), (size_t)impl.fSize * sizeof (float));

    try
    {
        if (impl.isStereoModel)
        {
            const char* inNames[2] = { impl.inputName.c_str (), impl.inputName2.c_str () };
            const char* outNames[2] = { impl.outputName.c_str (), impl.outputName2.c_str () };
            Ort::Value inTensors[2] = {
                Ort::Value::CreateTensor<float> (impl.memInfo, impl.inBuf.data (), (size_t)impl.fSize,
                                                 impl.shape.data (), impl.shape.size ()),
                Ort::Value::CreateTensor<float> (impl.memInfo, impl.inBuf2.data (), (size_t)impl.fSize,
                                                 impl.shape.data (), impl.shape.size ())
            };
            auto outputs = impl.session->Run (Ort::RunOptions{nullptr}, inNames, inTensors, 2,
                                              outNames, 2);
        }
        else
        {
            const char* inName  = impl.inputName.c_str ();
            const char* outName = impl.outputName.c_str ();
            Ort::Value inTensor = Ort::Value::CreateTensor<float> (
                impl.memInfo, impl.inBuf.data (), (size_t)impl.fSize,
                impl.shape.data (), impl.shape.size ());
            auto outputs = impl.session->Run (Ort::RunOptions{nullptr}, &inName, &inTensor, 1,
                                              &outName, 1);
            const float* ptr = outputs[0].GetTensorData<float> ();
            std::memcpy (dummyOut.data (), ptr, (size_t)impl.fSize * sizeof (float));
        }
        return true;
    }
    catch (...)
    {
        return false;
    }
#endif
}
