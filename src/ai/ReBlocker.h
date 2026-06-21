#pragma once
#include <vector>
#include <functional>
#include <algorithm>
#include <cstring>

/**
 * Adapts variable host buffer sizes to a fixed model frame size,
 * supporting an arbitrary number of synchronized audio channels (e.g. Mono or Stereo).
 *
 * Strategy:
 *   - Input rings accumulate host samples until a full frame is ready across all channels.
 *   - On each full frame: invoke the caller's FrameCallback, push result
 *     to the output rings.
 *   - Output rings are pre-seeded with one frame of silence, so
 *     latency == frameSize samples (in whatever SR the model runs at).
 *
 * Audio thread only; no locks or dynamic allocations after reset().
 */
class ReBlocker
{
  public:
    // fn(in, out): process one frame of fSize floats across numChannels.
    // in[ch] and out[ch] each point to fSize floats.
    using FrameCallback = std::function<void (const float* const* in, float* const* out)>;

    void reset (int frameSize, int numChannels = 1)
    {
        fSize = frameSize;
        numCh = numChannels;

        inBufs.resize ((size_t)numCh);
        tmpOuts.resize ((size_t)numCh);
        inPtrs.resize ((size_t)numCh);
        outPtrs.resize ((size_t)numCh);
        
        for (int ch = 0; ch < numCh; ++ch)
        {
            inBufs[(size_t)ch].assign ((size_t)frameSize, 0.f);
            tmpOuts[(size_t)ch].assign ((size_t)frameSize, 0.f);
            inPtrs[(size_t)ch] = inBufs[(size_t)ch].data ();
            outPtrs[(size_t)ch] = tmpOuts[(size_t)ch].data ();
        }
        inHead = 0;

        const int ringCap = frameSize * 8; // ample for any practical host block
        outRings.resize ((size_t)numCh);
        for (int ch = 0; ch < numCh; ++ch)
        {
            outRings[(size_t)ch].assign ((size_t)ringCap, 0.f);
        }

        // Pre-seed one silent frame → latency = frameSize samples
        outWrite = frameSize % ringCap;
        outRead = 0;
        outCount = frameSize;
    }

    // Push n samples across all channels; calls fn whenever a full frame is ready.
    void push (const float* const* src, int n, FrameCallback fn)
    {
        int remaining = n;
        int offset = 0;

        while (remaining > 0)
        {
            const int space = fSize - inHead;
            const int take = std::min (remaining, space);

            for (int ch = 0; ch < numCh; ++ch)
            {
                std::memcpy (inBufs[(size_t)ch].data () + inHead, src[ch] + offset, (size_t)take * sizeof (float));
            }
            inHead += take;
            offset += take;
            remaining -= take;

            if (inHead == fSize)
            {
                fn (inPtrs.data (), outPtrs.data ());

                const int cap = (int)outRings[0].size ();
                for (int ch = 0; ch < numCh; ++ch)
                {
                    for (int i = 0; i < fSize; ++i)
                        outRings[(size_t)ch][(size_t)((outWrite + i) % cap)] = tmpOuts[(size_t)ch][(size_t)i];
                }
                outWrite = (outWrite + fSize) % cap;
                outCount += fSize;
                inHead = 0;
            }
        }
    }

    // Pop n samples into dst channels; zero-fills if the output ring runs short.
    void pop (float* const* dst, int n)
    {
        const int avail = std::min (n, outCount);
        const int cap = (int)outRings[0].size ();

        for (int ch = 0; ch < numCh; ++ch)
        {
            for (int i = 0; i < avail; ++i)
                dst[ch][i] = outRings[(size_t)ch][(size_t)((outRead + i) % cap)];

            if (avail < n)
                std::fill (dst[ch] + avail, dst[ch] + n, 0.f);
        }

        outRead = (outRead + avail) % cap;
        outCount -= avail;
    }

    int frameSize () const { return fSize; }
    int numChannels () const { return numCh; }

  private:
    int fSize = 480;
    int numCh = 1;
    
    std::vector<std::vector<float>> inBufs;
    int inHead = 0;
    
    std::vector<std::vector<float>> outRings;
    int outWrite = 0;
    int outRead = 0;
    int outCount = 0;
    
    std::vector<std::vector<float>> tmpOuts; // scratch
    std::vector<const float*> inPtrs;
    std::vector<float*> outPtrs;
};
