#pragma once
#include <JuceHeader.h>
#include <array>
#include <algorithm>

struct MeterValues
{
    float peakL { -100.0f };
    float peakR { -100.0f };
    float rmsL  { -100.0f };
    float rmsR  { -100.0f };
};

/**
 * Lock-free SPSC ring buffer: audio thread pushes one MeterValues per block,
 * GUI timer (30 Hz) drains and takes the max across all pending frames.
 */
class MeterBus
{
public:
    static constexpr int kCapacity = 16;

    void push (MeterValues v) noexcept
    {
        int s1, b1, s2, b2;
        fifo.prepareToWrite (1, s1, b1, s2, b2);
        if (b1 == 0) return; // GUI is behind; just drop oldest
        buffer[static_cast<size_t> (s1)] = v;
        fifo.finishedWrite (1);
    }

    // Returns true if any new data was available; maxValues is updated in-place.
    bool drain (MeterValues& maxValues) noexcept
    {
        const int available = fifo.getNumReady();
        if (available == 0) return false;

        int s1, b1, s2, b2;
        fifo.prepareToRead (available, s1, b1, s2, b2);

        auto accumulate = [&] (int start, int count)
        {
            for (int i = 0; i < count; ++i)
            {
                const auto& v = buffer[static_cast<size_t> (start + i)];
                maxValues.peakL = std::max (maxValues.peakL, v.peakL);
                maxValues.peakR = std::max (maxValues.peakR, v.peakR);
                maxValues.rmsL  = std::max (maxValues.rmsL,  v.rmsL);
                maxValues.rmsR  = std::max (maxValues.rmsR,  v.rmsR);
            }
        };

        accumulate (s1, b1);
        accumulate (s2, b2);
        fifo.finishedRead (available);
        return true;
    }

private:
    juce::AbstractFifo fifo { kCapacity };
    std::array<MeterValues, kCapacity> buffer {};
};
