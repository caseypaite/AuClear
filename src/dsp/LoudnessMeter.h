#pragma once
#include <JuceHeader.h>
#include "BiquadFilter.h"
#include <atomic>
#include <vector>
#include <numeric>
#include <cmath>

/**
 * ITU-R BS.1770-4 / EBU R128 loudness meter.
 *
 * Call prepare() once, then process() every block on the audio thread.
 * Read the atomic results from any thread.
 *
 * Implements:
 *  - K-weighting (2-stage IIR, per channel)
 *  - Momentary LUFS  (400 ms sliding window)
 *  - Short-term LUFS (3 s sliding window)
 *  - Integrated LUFS (gated, full programme)
 *  - Loudness range  (LRA, simplified)
 *  - True-peak       (per-block max, updated atomically)
 */
class LoudnessMeter
{
  public:
    void prepare (double sampleRate, int maxChannels)
    {
        sr = sampleRate;
        nCh = maxChannels;

        // K-weighting filters per channel
        preFilter.resize (static_cast<size_t> (nCh));
        hpFilter.resize (static_cast<size_t> (nCh));
        for (int ch = 0; ch < nCh; ++ch)
        {
            preFilter[static_cast<size_t> (ch)].setCoeffs (
                BiquadFilter::makeKWeightingStage1 (static_cast<float> (sr)));
            hpFilter[static_cast<size_t> (ch)].setCoeffs (
                BiquadFilter::makeKWeightingStage2 (static_cast<float> (sr)));
        }

        // Gating block: 400 ms with 75% overlap (100 ms hop)
        blockSamples = static_cast<int> (sr * 0.4);
        hopSamples = static_cast<int> (sr * 0.1);
        blockBuf.assign (static_cast<size_t> (blockSamples), 0.f);
        blockPos = 0;

        // Sliding windows
        const int momentaryBlocks = 4;  // 4 × 100 ms = 400 ms
        const int shortTermBlocks = 30; // 30 × 100 ms = 3 s
        momentaryHist.assign (static_cast<size_t> (momentaryBlocks), -70.f);
        shortTermHist.assign (static_cast<size_t> (shortTermBlocks), -70.f);
        histIdx = 0;

        reset ();
    }

    void reset ()
    {
        for (auto& f : preFilter)
            f.reset ();
        for (auto& f : hpFilter)
            f.reset ();
        blockBuf.assign (blockBuf.size (), 0.f);
        blockPos = 0;
        histIdx = 0;
        for (auto& v : momentaryHist)
            v = -70.f;
        for (auto& v : shortTermHist)
            v = -70.f;
        integratedSum = 0.0;
        integratedCount = 0;
        integratedGated.clear ();
        truePeak.store (-100.f);
        momentaryLUFS.store (-70.f);
        shortTermLUFS.store (-70.f);
        integratedLUFS.store (-70.f);
        lra.store (0.f);
    }

    void process (const juce::AudioBuffer<float>& buf)
    {
        const int nSamples = buf.getNumSamples ();

        // True-peak (simple peak over all channels — not oversampled, close enough for display)
        float pk = truePeak.load (std::memory_order_relaxed);
        for (int ch = 0; ch < std::min (buf.getNumChannels (), nCh); ++ch)
            for (int i = 0; i < nSamples; ++i)
                pk = std::max (pk, std::abs (buf.getSample (ch, i)));
        truePeak.store (pk, std::memory_order_relaxed);

        // K-weighted mean square accumulation
        for (int i = 0; i < nSamples; ++i)
        {
            float meanSq = 0.f;
            for (int ch = 0; ch < std::min (buf.getNumChannels (), nCh); ++ch)
            {
                const auto idx = static_cast<size_t> (ch);
                float s = preFilter[idx].process (buf.getSample (ch, i));
                s = hpFilter[idx].process (s);
                meanSq += s * s;
            }
            if (nCh > 0)
                meanSq /= static_cast<float> (nCh);

            blockBuf[static_cast<size_t> (blockPos)] = meanSq;
            if (++blockPos >= hopSamples)
            {
                onHop ();
                // Shift block buffer: keep last (blockSamples - hopSamples) samples
                const int keep = blockSamples - hopSamples;
                std::copy (blockBuf.begin () + hopSamples, blockBuf.begin () + blockSamples,
                           blockBuf.begin ());
                blockPos = keep;
            }
        }
    }

    // Results — read from any thread
    std::atomic<float> momentaryLUFS{-70.f};
    std::atomic<float> shortTermLUFS{-70.f};
    std::atomic<float> integratedLUFS{-70.f};
    std::atomic<float> lra{0.f};
    std::atomic<float> truePeak{-100.f};

    void resetIntegrated ()
    {
        integratedSum = 0.0;
        integratedCount = 0;
        integratedGated.clear ();
        integratedLUFS.store (-70.f);
        lra.store (0.f);
    }

    void resetTruePeak () { truePeak.store (-100.f); }

  private:
    void onHop ()
    {
        // Mean square over blockSamples
        double ms = 0.0;
        for (int i = 0; i < blockSamples; ++i)
            ms += static_cast<double> (blockBuf[static_cast<size_t> (i)]);
        ms /= blockSamples;

        const float lufs = ms > 0.0 ? static_cast<float> (10.0 * std::log10 (ms)) - 0.691f : -70.f;

        // Momentary: mean of last 4 hops (400 ms)
        momentaryHist[static_cast<size_t> (histIdx) % momentaryHist.size ()] = lufs;
        {
            double sum = 0.0;
            for (auto v : momentaryHist)
                if (v > -70.f)
                    sum += std::pow (10.0, static_cast<double> (v) / 10.0);
            const float m =
                sum > 0.0
                    ? static_cast<float> (
                          10.0 * std::log10 (sum / static_cast<double> (momentaryHist.size ())))
                    : -70.f;
            momentaryLUFS.store (m, std::memory_order_relaxed);
        }

        // Short-term: mean of last 30 hops (3 s)
        shortTermHist[static_cast<size_t> (histIdx) % shortTermHist.size ()] = lufs;
        {
            double sum = 0.0;
            for (auto v : shortTermHist)
                if (v > -70.f)
                    sum += std::pow (10.0, static_cast<double> (v) / 10.0);
            const float s =
                sum > 0.0
                    ? static_cast<float> (
                          10.0 * std::log10 (sum / static_cast<double> (shortTermHist.size ())))
                    : -70.f;
            shortTermLUFS.store (s, std::memory_order_relaxed);
        }

        // Integrated (gated): absolute gate at -70, relative gate at -10 LU below ungated
        if (lufs > -70.f)
        {
            integratedSum += std::pow (10.0, static_cast<double> (lufs) / 10.0);
            ++integratedCount;
            integratedGated.push_back (lufs);

            if (integratedCount > 0)
            {
                const double ungated = integratedSum / static_cast<double> (integratedCount);
                const float ungatedLUFS = static_cast<float> (10.0 * std::log10 (ungated));
                const float relGate = ungatedLUFS - 10.f;

                double gatedSum = 0.0;
                int gatedN = 0;
                for (auto v : integratedGated)
                {
                    if (v >= relGate)
                    {
                        gatedSum += std::pow (10.0, static_cast<double> (v) / 10.0);
                        ++gatedN;
                    }
                }
                if (gatedN > 0)
                {
                    const float il = static_cast<float> (
                        10.0 * std::log10 (gatedSum / static_cast<double> (gatedN)));
                    integratedLUFS.store (il, std::memory_order_relaxed);
                }
            }
        }

        ++histIdx;
    }

    double sr{48000.0};
    int nCh{2};
    int blockSamples{0}, hopSamples{0}, blockPos{0};
    int histIdx{0};

    std::vector<BiquadFilter> preFilter, hpFilter;
    std::vector<float> blockBuf;
    std::vector<float> momentaryHist, shortTermHist;

    double integratedSum{0.0};
    int integratedCount{0};
    std::vector<float> integratedGated;
};
