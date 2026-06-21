#include "DemucsSession.h"

#if AUCLEAR_HAS_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────

static const std::vector<std::string> kDefaultSources{"drums", "bass", "other", "vocals"};

struct DemucsSession::Impl
{
#if AUCLEAR_HAS_ONNXRUNTIME
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "AuClearDemucs"};
    std::unique_ptr<Ort::Session> session;
    Ort::MemoryInfo memInfo{Ort::MemoryInfo::CreateCpu (OrtArenaAllocator, OrtMemTypeDefault)};
#endif

    int segSamples = 0;
    int nSources = 0;
    double sr = 44100.0;
    std::vector<std::string> sources;
    std::string lastError;
    bool loaded = false;

    // Pre-allocated flat buffers (no heap allocs in runSegment after loadModel)
    std::vector<float> inBuf;  // [2 * segSamples]
    std::vector<float> outBuf; // [nSources * 2 * segSamples]
};

// ─────────────────────────────────────────────────────────────────────────────

DemucsSession::DemucsSession () : pImpl (std::make_unique<Impl> ()) {}
DemucsSession::~DemucsSession () = default;

bool DemucsSession::loadModel (const std::string& modelPath)
{
    unloadModel ();
    auto& impl = *pImpl;

#if !AUCLEAR_HAS_ONNXRUNTIME
    (void)modelPath;
    impl.lastError = "Built without ONNX Runtime support.";
    return false;
#else
    try
    {
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads (std::max (1, 2)); // 2 threads for offline throughput
        opts.SetInterOpNumThreads (1);
        opts.SetGraphOptimizationLevel (GraphOptimizationLevel::ORT_ENABLE_ALL);

        // Attempt GPU EPs; silently skip if not installed.
        try
        {
            OrtCUDAProviderOptions cuda{};
            opts.AppendExecutionProvider_CUDA (cuda);
        }
        catch (...) {}

#if defined(__APPLE__)
        try
        {
            opts.AppendExecutionProvider_CoreML (0);
        }
        catch (...) {}
#endif

#ifdef _WIN32
        std::wstring wpath (modelPath.begin (), modelPath.end ());
        impl.session = std::make_unique<Ort::Session> (impl.env, wpath.c_str (), opts);
#else
        impl.session = std::make_unique<Ort::Session> (impl.env, modelPath.c_str (), opts);
#endif

        // ── Read model metadata ───────────────────────────────────────────────
        Ort::AllocatorWithDefaultOptions alloc;
        const auto meta = impl.session->GetModelMetadata ();

        auto readMeta = [&] (const char* key) -> std::string
        {
            try
            {
                return meta.LookupCustomMetadataMapAllocated (key, alloc).get ();
            }
            catch (...)
            {
                return {};
            }
        };

        const auto srStr  = readMeta ("demucs_samplerate");
        const auto srcStr = readMeta ("demucs_sources");
        const auto segStr = readMeta ("demucs_segment");

        if (! srStr.empty ())
            impl.sr = std::stod (srStr);

        if (! srcStr.empty ())
        {
            std::istringstream ss (srcStr);
            std::string token;
            impl.sources.clear ();
            while (std::getline (ss, token, ','))
            {
                while (! token.empty () && token.front () == ' ')
                    token.erase (token.begin ());
                while (! token.empty () && token.back () == ' ')
                    token.pop_back ();
                if (! token.empty ())
                    impl.sources.push_back (token);
            }
        }

        if (! segStr.empty ())
            impl.segSamples = std::stoi (segStr);

        // ── Validate and discover shapes ──────────────────────────────────────
        // Input: [batch, channels, time] → 3 dims
        const auto inInfo   = impl.session->GetInputTypeInfo (0);
        const auto inShape  = inInfo.GetTensorTypeAndShapeInfo ().GetShape ();
        // Output: [batch, sources, channels, time] → 4 dims
        const auto outInfo  = impl.session->GetOutputTypeInfo (0);
        const auto outShape = outInfo.GetTensorTypeAndShapeInfo ().GetShape ();

        if (inShape.size () < 3)
        {
            impl.lastError = "Expected input rank 3 [batch, channels, time]; got rank "
                             + std::to_string (inShape.size ());
            impl.session.reset ();
            return false;
        }
        if (outShape.size () < 4)
        {
            impl.lastError = "Expected output rank 4 [batch, sources, channels, time]; got rank "
                             + std::to_string (outShape.size ());
            impl.session.reset ();
            return false;
        }

        // Discover segment length from shape if not in metadata
        if (impl.segSamples <= 0)
        {
            if (inShape[2] > 0)
                impl.segSamples = static_cast<int> (inShape[2]);
            else
            {
                impl.lastError =
                    "Segment length is dynamic and not specified in model metadata "
                    "(set 'demucs_segment' with tools/export_demucs.py).";
                impl.session.reset ();
                return false;
            }
        }

        // Discover num sources from output shape if not in metadata
        impl.nSources = impl.sources.empty () ? 0 : (int)impl.sources.size ();
        if (outShape[1] > 0)
        {
            const int shapeNSources = static_cast<int> (outShape[1]);
            if (impl.nSources > 0 && impl.nSources != shapeNSources)
            {
                impl.lastError = "metadata says " + std::to_string (impl.nSources)
                                 + " sources but model output shape says "
                                 + std::to_string (shapeNSources);
                impl.session.reset ();
                return false;
            }
            impl.nSources = shapeNSources;
        }

        if (impl.nSources <= 0)
        {
            impl.lastError = "Could not determine number of sources from model shape or metadata.";
            impl.session.reset ();
            return false;
        }

        // Fill default source names if still missing
        if (impl.sources.empty ())
        {
            if (impl.nSources == 4)
                impl.sources = kDefaultSources;
            else
                for (int i = 0; i < impl.nSources; ++i)
                    impl.sources.push_back ("stem" + std::to_string (i));
        }

        // Allocate inference buffers
        impl.inBuf.assign ((size_t)2 * impl.segSamples, 0.f);
        impl.outBuf.assign ((size_t)impl.nSources * 2 * impl.segSamples, 0.f);

        impl.loaded = true;
        return true;
    }
    catch (const std::exception& e)
    {
        impl.lastError = e.what ();
        impl.session.reset ();
        return false;
    }
#endif
}

void DemucsSession::unloadModel ()
{
    auto& impl = *pImpl;
    impl.loaded = false;
#if AUCLEAR_HAS_ONNXRUNTIME
    impl.session.reset ();
#endif
    impl.segSamples = 0;
    impl.nSources = 0;
    impl.sources.clear ();
    impl.inBuf.clear ();
    impl.outBuf.clear ();
    impl.lastError = {};
}

bool DemucsSession::isLoaded () const { return pImpl->loaded; }
const std::string& DemucsSession::getLastError () const { return pImpl->lastError; }
int DemucsSession::segmentSamples () const { return pImpl->segSamples; }
int DemucsSession::numSources () const { return pImpl->nSources; }
double DemucsSession::sampleRate () const { return pImpl->sr; }
const std::vector<std::string>& DemucsSession::sourceNames () const { return pImpl->sources; }

bool DemucsSession::runSegment (const float* in, float* out)
{
#if !AUCLEAR_HAS_ONNXRUNTIME
    (void)in;
    (void)out;
    return false;
#else
    auto& impl = *pImpl;
    if (! impl.loaded || ! impl.session)
        return false;

    const size_t inSize  = (size_t)2 * impl.segSamples;
    const size_t outSize = (size_t)impl.nSources * 2 * impl.segSamples;

    std::memcpy (impl.inBuf.data (), in, inSize * sizeof (float));

    const int64_t inShape[3]  = {1, 2, impl.segSamples};

    const char* inName  = "input";
    const char* outName = "output";

    Ort::Value inTensor = Ort::Value::CreateTensor<float> (
        impl.memInfo, impl.inBuf.data (), inSize, inShape, 3);

    try
    {
        auto outputs = impl.session->Run (Ort::RunOptions{nullptr}, &inName, &inTensor, 1,
                                          &outName, 1);
        const float* ptr = outputs[0].GetTensorData<float> ();
        std::memcpy (out, ptr, outSize * sizeof (float));
        return true;
    }
    catch (const std::exception& e)
    {
        impl.lastError = e.what ();
        return false;
    }
#endif
}
