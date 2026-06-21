#pragma once
#include <JuceHeader.h>
#include <array>

/**
 * Lock-free SPSC ring buffer of stereo sample pairs for goniometer display.
 * Audio thread pushes; GUI thread drains.  Samples older than the ring capacity
 * are silently dropped so the display stays current without blocking.
 */
struct GoniometerFifo
{
    static constexpr int kCapacity = 8192;

    void push (const juce::AudioBuffer<float>& buf) noexcept
    {
        const int n   = buf.getNumSamples ();
        const int nCh = buf.getNumChannels ();

        int s1, b1, s2, b2;
        fifo.prepareToWrite (n, s1, b1, s2, b2);

        const float* L = buf.getReadPointer (0);
        const float* R = (nCh > 1) ? buf.getReadPointer (1) : L;

        for (int i = 0; i < b1; ++i)
        {
            lRing[static_cast<size_t> (s1 + i)] = L[i];
            rRing[static_cast<size_t> (s1 + i)] = R[i];
        }
        for (int i = 0; i < b2; ++i)
        {
            lRing[static_cast<size_t> (s2 + i)] = L[b1 + i];
            rRing[static_cast<size_t> (s2 + i)] = R[b1 + i];
        }
        fifo.finishedWrite (b1 + b2);
    }

    // Returns number of samples actually read (≤ maxSamples).
    int drain (float* outL, float* outR, int maxSamples) noexcept
    {
        const int available = fifo.getNumReady ();
        const int toRead    = juce::jmin (available, maxSamples);
        if (toRead == 0)
            return 0;

        int s1, b1, s2, b2;
        fifo.prepareToRead (toRead, s1, b1, s2, b2);

        for (int i = 0; i < b1; ++i)
        {
            outL[i] = lRing[static_cast<size_t> (s1 + i)];
            outR[i] = rRing[static_cast<size_t> (s1 + i)];
        }
        for (int i = 0; i < b2; ++i)
        {
            outL[b1 + i] = lRing[static_cast<size_t> (s2 + i)];
            outR[b1 + i] = rRing[static_cast<size_t> (s2 + i)];
        }
        fifo.finishedRead (b1 + b2);
        return b1 + b2;
    }

  private:
    juce::AbstractFifo fifo{kCapacity};
    std::array<float, kCapacity> lRing{}, rRing{};
};
