#include "DemucsRunner.h"
#include <cmath>
#include <cstring>
#include <vector>

DemucsRunner::DemucsRunner (DemucsSession& s) : session (s) {}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

bool DemucsRunner::resampleChannel (const float* src, int nSrc, double srcRate,
                                    float* dst, int nDst, double dstRate)
{
    if (std::abs (srcRate - dstRate) < 0.5)
    {
        const int n = std::min (nSrc, nDst);
        std::memcpy (dst, src, (size_t)n * sizeof (float));
        if (nDst > n)
            std::fill (dst + n, dst + nDst, 0.f);
        return true;
    }

    // LagrangeInterpolator may read slightly past the end of src — add zero pad.
    const int padded = nSrc + 16;
    std::vector<float> padBuf ((size_t)padded, 0.f);
    std::memcpy (padBuf.data (), src, (size_t)nSrc * sizeof (float));

    juce::LagrangeInterpolator interp;
    interp.reset ();
    interp.process (srcRate / dstRate, padBuf.data (), dst, nDst);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// run()
// ─────────────────────────────────────────────────────────────────────────────

juce::Array<juce::File> DemucsRunner::run (const juce::File& inputFile,
                                           const juce::File& outputDir,
                                           const Callbacks& cb)
{
    lastError.clear ();

    auto fail = [&] (const juce::String& msg) -> juce::Array<juce::File>
    {
        lastError = msg;
        return {};
    };

    auto cancelled = [&]
    { return cb.shouldCancel && cb.shouldCancel (); };

    auto progress = [&] (float p, const juce::String& msg)
    {
        if (cb.onProgress)
            cb.onProgress (p, msg);
    };

    if (! session.isLoaded ())
        return fail ("No Demucs model loaded.");

    const int segLen   = session.segmentSamples ();
    const int nSources = session.numSources ();
    const double modelSR = session.sampleRate ();
    const auto& sourceNames = session.sourceNames ();

    // ── 1. Open input file ────────────────────────────────────────────────────
    juce::AudioFormatManager fmt;
    fmt.registerBasicFormats ();
    fmt.registerFormat (new juce::MP3AudioFormat (), true);

    auto reader = std::unique_ptr<juce::AudioFormatReader> (fmt.createReaderFor (inputFile));
    if (! reader)
        return fail ("Could not open input file: " + inputFile.getFileName ());

    const double fileSR    = reader->sampleRate;
    const int fileNch      = juce::jlimit (1, 2, (int)reader->numChannels);
    const juce::int64 fileLen = reader->lengthInSamples;
    if (fileLen <= 0)
        return fail ("Input file appears empty.");

    progress (0.0f, "Reading input...");

    // Read entire file into a stereo buffer at its native SR
    juce::AudioBuffer<float> fileBuf (2, (int)fileLen);
    reader->read (&fileBuf, 0, (int)fileLen, 0, true, fileNch > 1);
    if (fileNch == 1)
        fileBuf.copyFrom (1, 0, fileBuf, 0, 0, (int)fileLen); // mono → stereo

    if (cancelled ())
        return {};

    // ── 2. Resample to model SR ───────────────────────────────────────────────
    progress (0.02f, "Resampling to model rate...");

    const int modelLen = (std::abs (fileSR - modelSR) < 0.5)
                             ? (int)fileLen
                             : (int)std::round ((double)fileLen * modelSR / fileSR);

    // Planar stereo at model SR: ch0 then ch1
    std::vector<float> modelInput ((size_t)modelLen * 2, 0.f);

    resampleChannel (fileBuf.getReadPointer (0), (int)fileLen, fileSR,
                     modelInput.data (), modelLen, modelSR);
    resampleChannel (fileBuf.getReadPointer (1), (int)fileLen, fileSR,
                     modelInput.data () + modelLen, modelLen, modelSR);

    fileBuf.setSize (0, 0); // release file memory

    if (cancelled ())
        return {};

    // ── 3. Compute overlap-add parameters ────────────────────────────────────
    const int overlapSamples = juce::jmax (1, (int)(segLen * overlapFraction));
    const int hop            = segLen - overlapSamples;

    // Cosine fade-in/out window applied at segment boundaries
    std::vector<float> fadeWin ((size_t)segLen, 1.0f);
    for (int n = 0; n < overlapSamples; ++n)
    {
        const float t = juce::MathConstants<float>::pi * (float)n / (float)overlapSamples;
        const float w = 0.5f * (1.0f - std::cos (t));
        fadeWin[(size_t)n] = w;
        fadeWin[(size_t)(segLen - 1 - n)] = w;
    }

    // ── 4. Allocate stem accumulation buffers ─────────────────────────────────
    // stemSum[src*2 + ch][sample] += weighted output
    const size_t stemBufSz = (size_t)nSources * 2 * (size_t)modelLen;
    std::vector<float> stemSum (stemBufSz, 0.f);
    std::vector<float> weightSum ((size_t)modelLen, 0.f);

    // ── 5. Chunk / overlap-add loop ───────────────────────────────────────────
    std::vector<float> segBuf ((size_t)2 * segLen, 0.f);
    std::vector<float> outBuf ((size_t)nSources * 2 * segLen, 0.f);

    const int numChunks = 1 + (modelLen + hop - 1) / hop;
    int chunksDone = 0;

    for (int start = 0; start < modelLen; start += hop)
    {
        if (cancelled ())
            return {};

        // Fill segment (zero-padded where start+n >= modelLen)
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* srcPtr = modelInput.data () + (size_t)ch * modelLen;
            float* segCh        = segBuf.data () + (size_t)ch * segLen;
            for (int n = 0; n < segLen; ++n)
            {
                const int idx = start + n;
                segCh[(size_t)n] = (idx < modelLen) ? srcPtr[(size_t)idx] : 0.f;
            }
        }

        if (! session.runSegment (segBuf.data (), outBuf.data ()))
            return fail ("Inference failed: " + juce::String (session.getLastError ()));

        // Accumulate weighted output into stemSum
        for (int src = 0; src < nSources; ++src)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                const size_t outOff = (size_t)(src * 2 + ch) * segLen;
                const size_t sumOff = (size_t)(src * 2 + ch) * modelLen;
                for (int n = 0; n < segLen; ++n)
                {
                    const int p = start + n;
                    if (p >= modelLen)
                        break;
                    stemSum[sumOff + (size_t)p] += outBuf[outOff + (size_t)n] * fadeWin[(size_t)n];
                }
            }
        }
        for (int n = 0; n < segLen; ++n)
        {
            const int p = start + n;
            if (p >= modelLen)
                break;
            weightSum[(size_t)p] += fadeWin[(size_t)n];
        }

        ++chunksDone;
        const float p = 0.05f + 0.90f * ((float)chunksDone / (float)numChunks);
        progress (p, "Separating stems " + juce::String (juce::roundToInt (p * 100)) + "%");
    }

    // ── 6. Normalise ─────────────────────────────────────────────────────────
    progress (0.96f, "Normalising...");
    for (int n = 0; n < modelLen; ++n)
    {
        const float invW = 1.0f / juce::jmax (1e-8f, weightSum[(size_t)n]);
        for (int src = 0; src < nSources; ++src)
            for (int ch = 0; ch < 2; ++ch)
                stemSum[(size_t)(src * 2 + ch) * modelLen + (size_t)n] *= invW;
    }

    if (cancelled ())
        return {};

    // ── 7. Write stem WAV files ───────────────────────────────────────────────
    progress (0.97f, "Writing stems...");
    outputDir.createDirectory ();

    const juce::String baseName = inputFile.getFileNameWithoutExtension ();
    juce::WavAudioFormat wavFmt;
    juce::Array<juce::File> outputs;

    // Resample buffer (reused per stem/channel)
    const int outLen = (int)fileLen;
    std::vector<float> stemCh0 ((size_t)outLen);
    std::vector<float> stemCh1 ((size_t)outLen);

    for (int src = 0; src < nSources; ++src)
    {
        if (cancelled ())
            return {};

        const juce::String name = src < (int)sourceNames.size () ? sourceNames[(size_t)src]
                                                                  : ("stem" + juce::String (src));
        const juce::File outFile = outputDir.getChildFile (baseName + "_" + name + ".wav");
        outFile.deleteFile ();

        auto outStream = std::make_unique<juce::FileOutputStream> (outFile);
        if (! outStream->openedOk ())
            return fail ("Could not create stem file: " + outFile.getFileName ());

        // Resample each channel from modelSR back to fileSR
        resampleChannel (stemSum.data () + (size_t)(src * 2 + 0) * modelLen, modelLen, modelSR,
                         stemCh0.data (), outLen, fileSR);
        resampleChannel (stemSum.data () + (size_t)(src * 2 + 1) * modelLen, modelLen, modelSR,
                         stemCh1.data (), outLen, fileSR);

        juce::StringPairArray noMeta;
        JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wdeprecated-declarations")
        JUCE_BEGIN_IGNORE_WARNINGS_MSVC (4996)
        auto writer = std::unique_ptr<juce::AudioFormatWriter> (
            wavFmt.createWriterFor (outStream.release (), fileSR, 2, 24, noMeta, 0));
        JUCE_END_IGNORE_WARNINGS_MSVC
        JUCE_END_IGNORE_WARNINGS_GCC_LIKE
        if (! writer)
            return fail ("Could not create WAV writer for " + name);

        // Write in chunks to avoid huge stack allocation
        constexpr int kWriteBlock = 4096;
        const float* ptrs[2] = {stemCh0.data (), stemCh1.data ()};
        for (int s = 0; s < outLen; s += kWriteBlock)
        {
            const int n = std::min (kWriteBlock, outLen - s);
            const float* chPtrs[2] = {ptrs[0] + s, ptrs[1] + s};
            writer->writeFromFloatArrays (chPtrs, 2, n);
        }

        writer.reset ();
        outputs.add (outFile);
    }

    progress (1.0f, "Done.");
    return outputs;
}
