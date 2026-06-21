#include "RealtimeStemProcessor.h"

#if AUCLEAR_HAS_ONNXRUNTIME

//==============================================================================
RealtimeStemProcessor::RealtimeStemProcessor ()
    : juce::Thread ("RealtimeStem", 0)
{
}

RealtimeStemProcessor::~RealtimeStemProcessor ()
{
    stopThread (4000);
}

//==============================================================================
bool RealtimeStemProcessor::loadModel (const juce::File& onnxPath)
{
    stopThread (4000); // ensure inference thread is done before swapping the session

    {
        juce::ScopedLock sl (modelLock);

        auto newSession = std::make_unique<DemucsSession> ();
        if (! newSession->loadModel (onnxPath.getFullPathName ().toStdString ()))
            return false;

        session    = std::move (newSession);
        segLen     = session->segmentSamples ();
        nSrc       = juce::jmin (4, session->numSources ());
        modelLoaded.store (true, std::memory_order_release);
    }

    // Allocate inference-thread private buffers (message thread — no alloc on audio thread).
    inBuf.resize  ((size_t)(2 * segLen), 0.f);
    outBuf.resize ((size_t)(nSrc * 2 * segLen), 0.f);

    resetFifos ();

    if (enabled.load (std::memory_order_relaxed))
    {
        status.store (Status::Buffering, std::memory_order_relaxed);
        startThread (juce::Thread::Priority::normal);
    }
    return true;
}

void RealtimeStemProcessor::unloadModel ()
{
    stopThread (4000);
    {
        juce::ScopedLock sl (modelLock);
        session.reset ();
        modelLoaded.store (false, std::memory_order_release);
        segLen = 0;
        nSrc   = 4;
    }
    resetFifos ();
    status.store (Status::Idle, std::memory_order_relaxed);
}

void RealtimeStemProcessor::setEnabled (bool e)
{
    enabled.store (e, std::memory_order_relaxed);

    if (e && isModelLoaded () && ! isThreadRunning ())
    {
        resetFifos ();
        status.store (Status::Buffering, std::memory_order_relaxed);
        startThread (juce::Thread::Priority::normal);
    }
    else if (! e && isThreadRunning ())
    {
        stopThread (4000);
        status.store (Status::Idle, std::memory_order_relaxed);
    }
}

void RealtimeStemProcessor::prepare (int maxBlockSize, double sampleRate)
{
    sr       = sampleRate;
    maxBlock = maxBlockSize;
    // Pre-allocate dry scratch — avoidReallocating means the audio thread never allocs.
    dryBuf.setSize (2, maxBlockSize, false, true, false);
}

void RealtimeStemProcessor::releaseResources ()
{
    stopThread (4000);
    resetFifos ();
}

void RealtimeStemProcessor::resetFifos ()
{
    if (segLen <= 0) return;

    // 3 segments of headroom: input side buffers while inference runs;
    // output side pre-fills while audio thread drains.
    const int ringSize = segLen * 3 + 1;

    inputFifo.setTotalSize (ringSize);
    inputRing.setSize (2, ringSize, false, true, false);
    inputFifo.reset ();

    outputFifo.setTotalSize (ringSize);
    outputRing.setSize (nSrc * 2, ringSize, false, true, false);
    outputFifo.reset ();
}

//==============================================================================
void RealtimeStemProcessor::run ()
{
    while (! threadShouldExit ())
    {
        wakeSignal.wait (80); // fallback poll: max 80 ms before re-checking

        if (threadShouldExit ()) break;

        // Try to acquire the model lock without blocking the message thread.
        juce::ScopedTryLock tryLock (modelLock);
        if (! tryLock.isLocked () || ! session) continue;

        // Process every complete segment that is available in the input ring.
        while (inputFifo.getNumReady () >= segLen && ! threadShouldExit ())
        {
            // ── Read one segment from the input ring ──────────────────────────
            int s1, sz1, s2, sz2;
            inputFifo.prepareToRead (segLen, s1, sz1, s2, sz2);

            const float* rL = inputRing.getReadPointer (0);
            const float* rR = inputRing.getReadPointer (1);

            // ch0 (left) → inBuf[0 .. segLen-1]
            juce::FloatVectorOperations::copy (inBuf.data (),          rL + s1, sz1);
            if (sz2 > 0) juce::FloatVectorOperations::copy (inBuf.data () + sz1, rL + s2, sz2);

            // ch1 (right) → inBuf[segLen .. 2*segLen-1]
            juce::FloatVectorOperations::copy (inBuf.data () + segLen,          rR + s1, sz1);
            if (sz2 > 0) juce::FloatVectorOperations::copy (inBuf.data () + segLen + sz1, rR + s2, sz2);

            inputFifo.finishedRead (sz1 + sz2);

            // ── Run inference ─────────────────────────────────────────────────
            status.store (Status::Processing, std::memory_order_relaxed);
            if (! session->runSegment (inBuf.data (), outBuf.data ()))
                continue; // inference error — skip segment, stay in Processing

            // ── Write result to the output ring ───────────────────────────────
            if (outputFifo.getFreeSpace () < segLen)
            {
                // Output ring full — inference is running faster than playback or
                // the audio thread is paused.  Drop this segment to make room.
                status.store (Status::Overrun, std::memory_order_relaxed);
                continue;
            }

            int os1, osz1, os2, osz2;
            outputFifo.prepareToWrite (segLen, os1, osz1, os2, osz2);

            for (int src = 0; src < nSrc; ++src)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float* srcPtr = outBuf.data () + (size_t)(src * 2 + ch) * (size_t)segLen;
                    float*       dstPtr = outputRing.getWritePointer (src * 2 + ch);

                    juce::FloatVectorOperations::copy (dstPtr + os1, srcPtr,          osz1);
                    if (osz2 > 0)
                        juce::FloatVectorOperations::copy (dstPtr + os2, srcPtr + osz1, osz2);
                }
            }

            outputFifo.finishedWrite (osz1 + osz2);
            status.store (Status::Active, std::memory_order_relaxed);
        }

        // If the input ring is partially filled but not a full segment, we're buffering.
        if (inputFifo.getNumReady () < segLen
            && status.load (std::memory_order_relaxed) == Status::Active)
        {
            status.store (Status::Buffering, std::memory_order_relaxed);
        }
    }
}

//==============================================================================
bool RealtimeStemProcessor::process (juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (! isActive ()) return false;

    const int nCh = juce::jmin (2, buffer.getNumChannels ());

    // ── Save dry signal for potential blend ───────────────────────────────────
    // avoidReallocating=true: never allocs if prepare() was called correctly.
    dryBuf.setSize (nCh, numSamples, false, false, true);
    for (int ch = 0; ch < nCh; ++ch)
        dryBuf.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    // ── Push input to the ring ────────────────────────────────────────────────
    if (inputFifo.getFreeSpace () >= numSamples)
    {
        int s1, sz1, s2, sz2;
        inputFifo.prepareToWrite (numSamples, s1, sz1, s2, sz2);

        for (int ch = 0; ch < nCh; ++ch)
        {
            const float* src = buffer.getReadPointer (ch);
            float*       dst = inputRing.getWritePointer (ch);
            juce::FloatVectorOperations::copy (dst + s1, src,          sz1);
            if (sz2 > 0) juce::FloatVectorOperations::copy (dst + s2, src + sz1, sz2);
        }

        inputFifo.finishedWrite (sz1 + sz2);

        // Wake the inference thread once a full segment is available.
        if (inputFifo.getNumReady () >= segLen)
            wakeSignal.signal ();
    }

    // ── Pull and mix stem output ──────────────────────────────────────────────
    if (outputFifo.getNumReady () < numSamples)
    {
        // Not enough output yet: dry signal plays through unchanged.
        if (status.load (std::memory_order_relaxed) != Status::Processing)
            status.store (Status::Buffering, std::memory_order_relaxed);
        return false;
    }

    int rs1, rsz1, rs2, rsz2;
    outputFifo.prepareToRead (numSamples, rs1, rsz1, rs2, rsz2);

    const bool anySoloed = [this]
    {
        for (const auto& s : stems)
            if (s.soloed.load (std::memory_order_relaxed)) return true;
        return false;
    }();

    buffer.clear (0, numSamples);

    for (int i = 0; i < nSrc; ++i)
    {
        const bool muted  = stems[i].muted.load  (std::memory_order_relaxed);
        const bool soloed = stems[i].soloed.load (std::memory_order_relaxed);
        if (muted || (anySoloed && ! soloed)) continue;

        const float g  = stems[i].gain.load (std::memory_order_relaxed);
        const float p  = stems[i].pan.load  (std::memory_order_relaxed);
        const float gL = g * (p <= 0.f ? 1.f : 1.f - p);
        const float gR = g * (p >= 0.f ? 1.f : 1.f + p);

        const float gains[2] = { gL, gR };

        for (int ch = 0; ch < nCh; ++ch)
        {
            const float* src = outputRing.getReadPointer (i * 2 + ch);
            buffer.addFrom (ch, 0,    src + rs1, rsz1, gains[ch]);
            if (rsz2 > 0)
                buffer.addFrom (ch, rsz1, src + rs2, rsz2, gains[ch]);
        }
    }

    outputFifo.finishedRead (rsz1 + rsz2);

    // ── Dry blend ─────────────────────────────────────────────────────────────
    const float dm = dryMix.load (std::memory_order_relaxed);
    if (dm > 0.f)
    {
        const float wet = 1.f - dm;
        for (int ch = 0; ch < nCh; ++ch)
        {
            buffer.applyGain (ch, 0, numSamples, wet);
            buffer.addFrom   (ch, 0, dryBuf, ch, 0, numSamples, dm);
        }
    }

    return true;
}

//==============================================================================
juce::String RealtimeStemProcessor::getStatusString () const
{
    switch (status.load (std::memory_order_relaxed))
    {
        case Status::Idle:
            return isModelLoaded () ? "Disabled" : "No model";
        case Status::Buffering:
        {
            const int ready = inputFifo.getNumReady ();
            const int pct   = segLen > 0 ? ready * 100 / segLen : 0;
            return juce::String ("Buffering ") + juce::String (pct) + "%";
        }
        case Status::Processing:
            return "Processing...";
        case Status::Active:
            return "Active";
        case Status::Overrun:
            return "Overrun";
    }
    return {};
}

#endif  // AUCLEAR_HAS_ONNXRUNTIME
