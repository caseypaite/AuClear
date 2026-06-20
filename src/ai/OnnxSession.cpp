#include "OnnxSession.h"

#if AUCLEAR_HAS_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

#include <array>
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
    std::array<int64_t, 2> shape{}; // {1, fSize}
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

        impl.session = std::make_unique<Ort::Session> (impl.env, modelPath.c_str (), opts);
        impl.inputName = inputName;
        impl.outputName = outputName;

        // Discover frame size from model input shape [batch, frameSize]
        Ort::AllocatorWithDefaultOptions alloc;
        auto typeInfo = impl.session->GetInputTypeInfo (0);
        auto shape = typeInfo.GetTensorTypeAndShapeInfo ().GetShape ();

        if (shape.size () < 2 || shape[1] <= 0)
        {
            impl.lastError = "Unexpected input shape (expected [batch, frameSize]).";
            impl.status = Status::Error;
            impl.session.reset ();
            return false;
        }

        impl.fSize = static_cast<int> (shape[1]);
        impl.shape = {1, static_cast<int64_t> (impl.fSize)};
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
#if AUCLEAR_HAS_ONNXRUNTIME
    pImpl->session.reset ();
#endif
    pImpl->fSize = 0;
    pImpl->status = Status::Idle;
    pImpl->lastError = {};
}

bool OnnxSession::isLoaded () const
{
    return pImpl->status == Status::Ready;
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
    auto& impl = *pImpl;
    if (!impl.session)
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
    if (!isLoaded ())
        return false;
    std::vector<float> dummy ((size_t)pImpl->fSize, 0.f);
    std::vector<float> dummyOut ((size_t)pImpl->fSize, 0.f);
    return runFrame (dummy.data (), dummyOut.data ());
}
