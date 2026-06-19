#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>

/**
 * Accumulates audio samples on the audio thread, computes a windowed FFT when
 * kFFTSize samples are ready, and pushes the log-magnitude bins via an SPSC
 * FIFO so the GUI can drain and render a spectrum display.
 *
 * Call prepare() in prepareToPlay, pushSamples() in processBlock,
 * and drain() from a GUI timer.
 */
class SpectrumFifo
{
  public:
    static constexpr int kFFTOrder = 10; // 1024 samples
    static constexpr int kFFTSize = 1 << kFFTOrder;
    static constexpr int kNumBins = kFFTSize / 2;
    static constexpr int kFifoDepth = 4;

    using Bins = std::array<float, kNumBins>;

    void prepare (double /*sampleRate*/)
    {
        fft = std::make_unique<juce::dsp::FFT> (kFFTOrder);
        fftData.assign (kFFTSize * 2, 0.f);

        hann.resize (kFFTSize);
        for (int i = 0; i < kFFTSize; ++i)
            hann[static_cast<size_t> (i)] =
                0.5f * (1.f - std::cos (2.f * juce::MathConstants<float>::pi * i / (kFFTSize - 1)));

        accumulator.assign (kFFTSize, 0.f);
        accCount = 0;

        for (auto& b : binsFifo)
            b.fill (0.f);
    }

    // Called on audio thread: mono mix of a buffer
    void pushSamples (const juce::AudioBuffer<float>& buf) noexcept
    {
        if (!fft)
            return;

        const int nCh = buf.getNumChannels ();
        const int nSamples = buf.getNumSamples ();

        for (int i = 0; i < nSamples; ++i)
        {
            float mono = 0.f;
            for (int ch = 0; ch < nCh; ++ch)
                mono += buf.getSample (ch, i);
            if (nCh > 0)
                mono /= static_cast<float> (nCh);

            accumulator[static_cast<size_t> (accCount)] = mono;
            if (++accCount >= kFFTSize)
            {
                computeAndPush ();
                accCount = 0;
            }
        }
    }

    // Called on GUI thread: returns true if new data
    bool drain (Bins& out) noexcept
    {
        int s1, b1, s2, b2;
        fifo.prepareToRead (1, s1, b1, s2, b2);
        if (b1 == 0)
            return false;
        out = binsFifo[static_cast<size_t> (s1)];
        fifo.finishedRead (1);
        return true;
    }

  private:
    void computeAndPush () noexcept
    {
        // Apply Hann window and copy into FFT buffer
        for (int i = 0; i < kFFTSize; ++i)
        {
            const auto idx = static_cast<size_t> (i);
            fftData[idx] = accumulator[idx] * hann[idx];
        }
        std::fill (fftData.begin () + kFFTSize, fftData.end (), 0.f);

        fft->performFrequencyOnlyForwardTransform (fftData.data ());

        Bins bins;
        const float norm = 1.f / static_cast<float> (kFFTSize);
        for (int i = 0; i < kNumBins; ++i)
        {
            const float mag = fftData[static_cast<size_t> (i)] * norm;
            bins[static_cast<size_t> (i)] =
                mag > 0.f ? juce::Decibels::gainToDecibels (mag) : -100.f;
        }

        int s1, b1, s2, b2;
        fifo.prepareToWrite (1, s1, b1, s2, b2);
        if (b1 > 0)
        {
            binsFifo[static_cast<size_t> (s1)] = bins;
            fifo.finishedWrite (1);
        }
    }

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> fftData;
    std::vector<float> hann;
    std::vector<float> accumulator;
    int accCount{0};

    juce::AbstractFifo fifo{kFifoDepth};
    std::array<Bins, kFifoDepth> binsFifo{};
};
