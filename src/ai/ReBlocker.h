#pragma once
#include <vector>
#include <functional>
#include <algorithm>
#include <cstring>

/**
 * Adapts variable host buffer sizes to a fixed model frame size.
 *
 * Strategy:
 *   - Input ring accumulates host samples until a full frame is ready.
 *   - On each full frame: invoke the caller's FrameCallback, push result
 *     to the output ring.
 *   - Output ring is pre-seeded with one frame of silence, so
 *     latency == frameSize samples (in whatever SR the model runs at).
 *
 * Audio thread only; no locks or dynamic allocations after reset().
 */
class ReBlocker
{
  public:
    // fn(in, out): process one frame of fSize floats.
    using FrameCallback = std::function<void (const float* in, float* out)>;

    void reset (int frameSize)
    {
        fSize = frameSize;

        inBuf.assign  ((size_t) frameSize, 0.f);
        tmpOut.assign ((size_t) frameSize, 0.f); // pre-allocate — no heap alloc in push()
        inHead = 0;

        const int ringCap = frameSize * 8; // ample for any practical host block
        outRing.assign ((size_t) ringCap, 0.f);

        // Pre-seed one silent frame → latency = frameSize samples
        outWrite = frameSize % ringCap;
        outRead  = 0;
        outCount = frameSize;
    }

    // Push n samples; calls fn whenever a full frame is ready.
    void push (const float* src, int n, FrameCallback fn)
    {
        int remaining = n;
        const float* rd = src;

        while (remaining > 0)
        {
            const int space = fSize - inHead;
            const int take  = std::min (remaining, space);

            std::memcpy (inBuf.data () + inHead, rd, (size_t) take * sizeof (float));
            inHead    += take;
            rd        += take;
            remaining -= take;

            if (inHead == fSize)
            {
                fn (inBuf.data (), tmpOut.data ()); // tmpOut pre-allocated in reset()

                const int cap = (int) outRing.size ();
                for (int i = 0; i < fSize; ++i)
                    outRing[(size_t) ((outWrite + i) % cap)] = tmpOut[(size_t) i];
                outWrite = (outWrite + fSize) % cap;
                outCount += fSize;
                inHead   = 0;
            }
        }
    }

    // Pop n samples into dst; zero-fills if the output ring runs short.
    void pop (float* dst, int n)
    {
        const int avail = std::min (n, outCount);
        const int cap   = (int) outRing.size ();

        for (int i = 0; i < avail; ++i)
            dst[i] = outRing[(size_t) ((outRead + i) % cap)];

        outRead   = (outRead + avail) % cap;
        outCount -= avail;

        if (avail < n)
            std::fill (dst + avail, dst + n, 0.f);
    }

    int frameSize () const { return fSize; }

  private:
    int               fSize    = 480;
    std::vector<float> inBuf;
    int               inHead   = 0;
    std::vector<float> outRing;
    int               outWrite = 0;
    int               outRead  = 0;
    int               outCount = 0;
    std::vector<float> tmpOut; // scratch — allocated by reset(), reused per frame
};
