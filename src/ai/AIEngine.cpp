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

    void reset (double hostSR, double modelSR, int maxHostBlock, int modelFrameSize, int totalLatencyHostSamples)
    {
        reBlocker.reset (modelFrameSize);

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

    constexpr int numCh = 2; // always prepare stereo
    channels.resize ((size_t)numCh);
    for (int ch = 0; ch < numCh; ++ch)
    {
        channels[(size_t)ch] = std::make_unique<ChannelState> ();
        channels[(size_t)ch]->reset (sampleRate, modelSR, maxBlockSize, modelFrame, cachedLatency);
    }
    prepared = true;
}

void AIEngine::reset ()
{
    for (auto& ch : channels)
        if (ch)
            ch->reset (hostSR, modelSR, maxBlock, modelFrame, cachedLatency);
}

// ---------------------------------------------------------------------------
void AIEngine::process (juce::AudioBuffer<float>& buffer, float strength, bool listen)
{
    if (!prepared)
        return;

    const int numCh = std::min (buffer.getNumChannels (), (int)channels.size ());

    const juce::int64 t0 = juce::Time::getHighResolutionTicks ();

    for (int ch = 0; ch < numCh; ++ch)
        processChannel (ch, buffer, strength, listen);

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
    cs.reBlocker.push (cs.modelIn.data (), nModel,
                       [this, ch] (const float* in, float* out)
                       {
                           if (!sessions[(size_t)ch]->runFrame (in, out))
                               std::memcpy (out, in, (size_t)modelFrame * sizeof (float));
                       });

    cs.reBlocker.pop (cs.modelOut.data (), nModel);

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
