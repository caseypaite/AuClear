#include "AIEngine.h"
#include "ReBlocker.h"
#include <cstring>
#include <cmath>

// ---------------------------------------------------------------------------
// ChannelState: per-channel re-blocking + resampling + dry delay
// ---------------------------------------------------------------------------
struct AIEngine::ChannelState
{
    ReBlocker reBlocker;
    juce::LagrangeInterpolator downSampler; // host SR → model SR
    juce::LagrangeInterpolator upSampler;   // model SR → host SR

    // Scratch buffers — all allocated once in reset(), never on the audio thread.
    std::vector<float> modelIn;
    std::vector<float> modelOut;
    std::vector<float> wetHost; // host-SR output of the upsample stage
    std::vector<float> resampleInPad; // padded input for downSampler

    // Dry delay ring: aligns original with the re-blocked wet output.
    std::vector<float> dryRing;
    int dryWrite = 0;
    int dryRead = 0;

    void reset (double hostSR, double modelSR, int maxHostBlock, int modelFrameSize, int totalLatencyHostSamples, int numChannels = 1)
    {
        reBlocker.reset (modelFrameSize, numChannels);

        downSampler.reset ();
        upSampler.reset ();

        // Worst-case model-SR samples per host block (with headroom).
        const int maxModelSamples =
            (int)std::ceil ((double)maxHostBlock * modelSR / hostSR) + 16;
        modelIn.assign ((size_t)maxModelSamples, 0.f);
        modelOut.assign ((size_t)maxModelSamples, 0.f);
        wetHost.assign ((size_t)maxHostBlock, 0.f); // exact host-block size
        resampleInPad.assign ((size_t)maxHostBlock + 16, 0.f);


        // Dry delay ring must hold at least totalLatencyHostSamples.
        const int ringLen = juce::nextPowerOfTwo (totalLatencyHostSamples * 2 + maxHostBlock);
        dryRing.assign ((size_t)ringLen, 0.f);
        dryWrite = totalLatencyHostSamples % ringLen; // pre-advance write head
        dryRead = 0;
    }
};

// ---------------------------------------------------------------------------
AIEngine::AIEngine ()
{
    constexpr size_t numCh = 2;
    sessions.resize (numCh);
    for (size_t i = 0; i < numCh; ++i)
        sessions[i] = std::make_unique<OnnxSession> ();
}
AIEngine::~AIEngine () = default;

static void parseConfigIni (const juce::File& configFile, double& outSR, int& outFrameSize)
{
    juce::StringArray lines;
    configFile.readLines (lines);
    for (const auto& line : lines)
    {
        auto trimmed = line.trim ();
        if (trimmed.startsWithChar (';') || trimmed.startsWithChar ('#'))
            continue; // Skip comment
        
        if (trimmed.contains ("="))
        {
            auto key = trimmed.upToFirstOccurrenceOf ("=", false, false).trim ().toLowerCase ();
            auto val = trimmed.fromFirstOccurrenceOf ("=", false, false).trim ();
            
            if (key == "sr")
            {
                double parsedSR = val.getDoubleValue ();
                if (parsedSR > 0.0)
                    outSR = parsedSR;
            }
            else if (key == "hop_size" || key == "frame_size" || key == "fft_size")
            {
                int parsedFrame = val.getIntValue ();
                if (parsedFrame > 0)
                    outFrameSize = parsedFrame;
            }
        }
    }
}

bool AIEngine::loadModel (const juce::File& file)
{
    if (! file.existsAsFile ())
        return false;

    // Reset default parameters
    modelSR = 48000.0;
    modelFrame = 480;

    // Try parsing config.ini
    bool hasCustomHopSize = false;
    double parsedSR = 48000.0;
    int parsedFrame = 480;

    auto config = file.getParentDirectory ().getChildFile (file.getFileNameWithoutExtension () + ".config.ini");
    if (! config.existsAsFile ())
        config = file.getParentDirectory ().getChildFile ("config.ini");

    if (config.existsAsFile ())
    {
        double oldSR = parsedSR;
        int oldFrame = parsedFrame;
        parseConfigIni (config, parsedSR, parsedFrame);
        if (parsedSR != oldSR)
            modelSR = parsedSR;
        if (parsedFrame != oldFrame)
        {
            modelFrame = parsedFrame;
            hasCustomHopSize = true;
        }
    }

    bool allOk = true;
    for (auto& s : sessions)
    {
        if (! s->loadModel (file.getFullPathName ().toStdString ()))
            allOk = false;
    }

    if (allOk)
    {
        // ONNX shape frame size is the ultimate source of truth if config.ini didn't override it
        int discoveredFrame = sessions[0]->frameSize ();
        if (discoveredFrame > 0 && ! hasCustomHopSize)
            modelFrame = discoveredFrame;

        // Re-prepare dynamically to update channel states/resamplers with new model parameters
        if (prepared)
            prepare (hostSR, maxBlock);

        for (auto& s : sessions)
        {
            s->preWarm ();   // JIT compile on message thread before audio thread sees the model
            s->makeReady (); // release-store: audio thread may now call runFrame()
        }
    }
    return allOk;
}

void AIEngine::unloadModel ()
{
    for (auto& s : sessions)
        s->unloadModel ();
}

bool AIEngine::isLoaded () const
{
    for (const auto& s : sessions)
        if (! s->isLoaded ())
            return false;
    return ! sessions.empty ();
}

OnnxSession::Status AIEngine::getStatus () const
{
    if (sessions.empty ())
        return OnnxSession::Status::Idle;
    return sessions[0]->getStatus ();
}

juce::String AIEngine::getStatusString () const
{
    if (sessions.empty ())
        return "No model loaded";

    for (const auto& s : sessions)
    {
        if (s->getStatus () == OnnxSession::Status::Error)
            return "Error: " + juce::String (s->getLastError ());
    }

    switch (sessions[0]->getStatus ())
    {
    case OnnxSession::Status::Ready:
        return "Model ready";
    case OnnxSession::Status::Error:
        return "Error: " + juce::String (sessions[0]->getLastError ());
    case OnnxSession::Status::Idle:
    default:
        return "No model loaded";
    }
}

// ---------------------------------------------------------------------------
void AIEngine::prepare (double sampleRate, int maxBlockSize)
{
    hostSR = sampleRate;
    maxBlock = maxBlockSize;

    // Latency = one model frame converted to host samples + 1 resampler pad
    const int frameHost = (int)std::ceil ((double)modelFrame * sampleRate / modelSR);
    cachedLatency = frameHost;

    const bool isStereo = isLoaded () && sessions[0]->isStereo ();

    constexpr int numCh = 2; // always prepare stereo
    channels.resize ((size_t)numCh);
    for (int ch = 0; ch < numCh; ++ch)
    {
        channels[(size_t)ch] = std::make_unique<ChannelState> ();
    }
    if (numCh >= 2)
    {
        channels[0]->reset (sampleRate, modelSR, maxBlockSize, modelFrame, cachedLatency, isStereo ? 2 : 1);
        channels[1]->reset (sampleRate, modelSR, maxBlockSize, modelFrame, cachedLatency, 1);
    }
    prepared = true;
}

void AIEngine::reset ()
{
    const bool isStereo = isLoaded () && sessions[0]->isStereo ();
    if (channels.size () >= 2 && channels[0] && channels[1])
    {
        channels[0]->reset (hostSR, modelSR, maxBlock, modelFrame, cachedLatency, isStereo ? 2 : 1);
        channels[1]->reset (hostSR, modelSR, maxBlock, modelFrame, cachedLatency, 1);
    }
}

// ---------------------------------------------------------------------------
void AIEngine::process (juce::AudioBuffer<float>& buffer, float strength, bool listen)
{
    if (!prepared)
        return;

    const int numCh = std::min (buffer.getNumChannels (), (int)channels.size ());
    if (numCh == 0)
        return;

    const bool isStereo = isLoaded () && sessions[0]->isStereo ();
    const juce::int64 t0 = juce::Time::getHighResolutionTicks ();

    if (isStereo && numCh >= 2)
    {
        // ── Stereo model path (joint processing) ───────────────────────────
        auto& cs0 = *channels[0];
        auto& cs1 = *channels[1];
        
        float* wr0 = buffer.getWritePointer (0);
        float* wr1 = buffer.getWritePointer (1);
        const int n = buffer.getNumSamples ();

        // 1. Capture dry into rings
        const int cap0 = (int)cs0.dryRing.size ();
        for (int i = 0; i < n; ++i)
        {
            cs0.dryRing[(size_t)(cs0.dryWrite % cap0)] = wr0[i];
            cs0.dryWrite = (cs0.dryWrite + 1) % cap0;
        }
        const int cap1 = (int)cs1.dryRing.size ();
        for (int i = 0; i < n; ++i)
        {
            cs1.dryRing[(size_t)(cs1.dryWrite % cap1)] = wr1[i];
            cs1.dryWrite = (cs1.dryWrite + 1) % cap1;
        }

        // 2. Resample host -> model SR
        const double downRatio = hostSR / modelSR;
        int nModel;

        if (std::abs (hostSR - modelSR) < 0.5)
        {
            nModel = n;
            std::memcpy (cs0.modelIn.data (), wr0, (size_t)n * sizeof (float));
            std::memcpy (cs1.modelIn.data (), wr1, (size_t)n * sizeof (float));
        }
        else
        {
            nModel = (int)std::ceil ((double)n / downRatio);
            nModel = std::min (nModel, (int)cs0.modelIn.size ());

            // Downsample Left
            const int capL = std::min (n, (int)cs0.resampleInPad.size ());
            std::memcpy (cs0.resampleInPad.data (), wr0, (size_t)capL * sizeof (float));
            const int padL = std::min (16, (int)cs0.resampleInPad.size () - capL);
            if (padL > 0) std::fill (cs0.resampleInPad.begin () + capL, cs0.resampleInPad.begin () + capL + padL, 0.f);
            cs0.downSampler.process (downRatio, cs0.resampleInPad.data (), cs0.modelIn.data (), nModel);

            // Downsample Right
            const int capR = std::min (n, (int)cs1.resampleInPad.size ());
            std::memcpy (cs1.resampleInPad.data (), wr1, (size_t)capR * sizeof (float));
            const int padR = std::min (16, (int)cs1.resampleInPad.size () - capR);
            if (padR > 0) std::fill (cs1.resampleInPad.begin () + capR, cs1.resampleInPad.begin () + capR + padR, 0.f);
            cs1.downSampler.process (downRatio, cs1.resampleInPad.data (), cs1.modelIn.data (), nModel);
        }

        // 3. Stereo re-block + joint inference
        const float* srcPtrs[2] = { cs0.modelIn.data (), cs1.modelIn.data () };
        cs0.reBlocker.push (srcPtrs, nModel, [this] (const float* const* in, float* const* out)
        {
            if (!sessions[0]->runFrame (in[0], in[1], out[0], out[1]))
            {
                std::memcpy (out[0], in[0], (size_t)modelFrame * sizeof (float));
                std::memcpy (out[1], in[1], (size_t)modelFrame * sizeof (float));
            }
        });

        float* dstPtrs[2] = { cs0.modelOut.data (), cs1.modelOut.data () };
        cs0.reBlocker.pop (dstPtrs, nModel);

        // Pad modelOuts for LagrangeInterpolator
        const int padStart0 = std::min (nModel, (int)cs0.modelOut.size ());
        const int padSize0 = std::min (16, (int)cs0.modelOut.size () - padStart0);
        if (padSize0 > 0) std::fill (cs0.modelOut.begin () + padStart0, cs0.modelOut.begin () + padStart0 + padSize0, 0.f);

        const int padStart1 = std::min (nModel, (int)cs1.modelOut.size ());
        const int padSize1 = std::min (16, (int)cs1.modelOut.size () - padStart1);
        if (padSize1 > 0) std::fill (cs1.modelOut.begin () + padStart1, cs1.modelOut.begin () + padStart1 + padSize1, 0.f);

        // 4. Resample model -> host SR
        if (std::abs (hostSR - modelSR) < 0.5)
        {
            std::memcpy (cs0.wetHost.data (), cs0.modelOut.data (), (size_t)n * sizeof (float));
            std::memcpy (cs1.wetHost.data (), cs1.modelOut.data (), (size_t)n * sizeof (float));
        }
        else
        {
            const double upRatio = modelSR / hostSR;
            cs0.upSampler.process (upRatio, cs0.modelOut.data (), cs0.wetHost.data (), n);
            cs1.upSampler.process (upRatio, cs1.modelOut.data (), cs1.wetHost.data (), n);
        }

        // 5. Mix dry and wet back to buffer
        for (int i = 0; i < n; ++i)
        {
            const float dry0 = cs0.dryRing[(size_t)(cs0.dryRead % cap0)];
            cs0.dryRead = (cs0.dryRead + 1) % cap0;
            const float wet0 = cs0.wetHost[(size_t)i];
            
            if (listen)
                wr0[i] = dry0 - wet0;
            else
                wr0[i] = dry0 + strength * (wet0 - dry0);

            const float dry1 = cs1.dryRing[(size_t)(cs1.dryRead % cap1)];
            cs1.dryRead = (cs1.dryRead + 1) % cap1;
            const float wet1 = cs1.wetHost[(size_t)i];

            if (listen)
                wr1[i] = dry1 - wet1;
            else
                wr1[i] = dry1 + strength * (wet1 - dry1);
        }
    }
    else
    {
        // ── Mono model path (existing separate loop) ───────────────────────
        for (int ch = 0; ch < numCh; ++ch)
            processChannel (ch, buffer, strength, listen);
    }

    const double elapsedSec = (double)(juce::Time::getHighResolutionTicks () - t0) /
                              (double)juce::Time::getHighResolutionTicksPerSecond ();
    const double budgetSec = (double)buffer.getNumSamples () / hostSR;
    cpuLoadFraction.store (budgetSec > 0.0 ? (float)(elapsedSec / budgetSec) : 0.f,
                           std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
void AIEngine::processChannel (int ch, juce::AudioBuffer<float>& buf, float strength, bool listen)
{
    auto& cs = *channels[(size_t)ch];
    auto* wr = buf.getWritePointer (ch);
    const int n = buf.getNumSamples ();

    // 1. Capture dry with ring delay
    {
        const int cap = (int)cs.dryRing.size ();
        for (int i = 0; i < n; ++i)
        {
            cs.dryRing[(size_t)(cs.dryWrite % cap)] = wr[i];
            cs.dryWrite = (cs.dryWrite + 1) % cap;
        }
    }

    // Always run the full pipeline so the ReBlocker and resamplers stay warmed up.
    // When no model is loaded, runFrame() returns false and the reBlocker callback
    // copies input → output (identity), maintaining the same latency with no glitch.
    // This avoids the click that would occur if we short-circuited here and then
    // switched to the inference path mid-stream when the model became available.

    // 2. Resample host→model SR if needed
    const double downRatio = hostSR / modelSR;
    int nModel;

    if (std::abs (hostSR - modelSR) < 0.5)
    {
        // No resampling needed — copy directly
        nModel = n;
        std::memcpy (cs.modelIn.data (), wr, (size_t)n * sizeof (float));
    }
    else
    {
        // Downsample / upsample to model SR
        // LagrangeInterpolator::process(speedRatio, src, dst, numDstSamples)
        // speedRatio = srcSamplesPerOutput = hostSR/modelSR
        nModel = (int)std::ceil ((double)n / downRatio);
        nModel = std::min (nModel, (int)cs.modelIn.size ());

        // LagrangeInterpolator may read past the end of the input — copy to padded buffer first
        const int cap = std::min (n, (int)cs.resampleInPad.size ());
        std::memcpy (cs.resampleInPad.data (), wr, (size_t)cap * sizeof (float));
        const int padSize = std::min (16, (int)cs.resampleInPad.size () - cap);
        if (padSize > 0)
            std::fill (cs.resampleInPad.begin () + cap, cs.resampleInPad.begin () + cap + padSize, 0.f);

        cs.downSampler.process (downRatio, cs.resampleInPad.data (), cs.modelIn.data (), nModel);
    }

    // 3. Re-block + inference
    const float* srcPtr = cs.modelIn.data ();
    cs.reBlocker.push (&srcPtr, nModel,
                       [this, ch] (const float* const* in, float* const* out)
                       {
                           if (!sessions[(size_t)ch]->runFrame (in[0], out[0]))
                               std::memcpy (out[0], in[0], (size_t)modelFrame * sizeof (float));
                       });

    float* dstPtr = cs.modelOut.data ();
    cs.reBlocker.pop (&dstPtr, nModel);

    // LagrangeInterpolator may read past the end of modelOut — zero-pad it
    const int padStart = std::min (nModel, (int)cs.modelOut.size ());
    const int padSize = std::min (16, (int)cs.modelOut.size () - padStart);
    if (padSize > 0)
        std::fill (cs.modelOut.begin () + padStart, cs.modelOut.begin () + padStart + padSize, 0.f);

    // 4. Resample model→host SR (into pre-allocated cs.wetHost — no heap alloc)
    if (std::abs (hostSR - modelSR) < 0.5)
    {
        std::memcpy (cs.wetHost.data (), cs.modelOut.data (), (size_t)n * sizeof (float));
    }
    else
    {
        const double upRatio = modelSR / hostSR; // < 1 when upsampling to lower SR
        cs.upSampler.process (upRatio, cs.modelOut.data (), cs.wetHost.data (), n);
    }


    // 5. Mix or listen-to-removed
    const int cap = (int)cs.dryRing.size ();
    for (int i = 0; i < n; ++i)
    {
        const float dry = cs.dryRing[(size_t)(cs.dryRead % cap)];
        cs.dryRead = (cs.dryRead + 1) % cap;

        const float wet = cs.wetHost[(size_t)i];

        if (listen)
            wr[i] = dry - wet; // subtract to hear what was removed
        else
            wr[i] = dry + strength * (wet - dry); // wet/dry blend
    }
}
