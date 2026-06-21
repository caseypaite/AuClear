#include "SpectralRepairModule.h"
#include <cmath>
#include <algorithm>

// ── StftEngine ────────────────────────────────────────────────────────────────

void SpectralRepairModule::StftEngine::prepare (double /*sr*/)
{
    fft = std::make_unique<juce::dsp::FFT> (kOrder);

    window.resize (kFFTSize);
    for (int n = 0; n < kFFTSize; ++n)
        window[static_cast<size_t> (n)] =
            0.5f * (1.f - std::cos (2.f * juce::MathConstants<float>::pi * n / kFFTSize));

    fftBuf.assign       (2 * kFFTSize, 0.f);
    inputFifo.assign    (kHop,         0.f);
    outputFifo.assign   (kHop,         0.f);
    analysisFrame.assign(kFFTSize,     0.f);
    synthesisAcc.assign (2 * kFFTSize, 0.f);
    inputPos     = 0;
    outputPos    = 0;
    synthesisPos = 0;

    // Normalization: WOLA gain = N × Σ w²(n - r·H) at any n.
    // For Hann at 75% overlap (H = N/4), Σ w² = 1.5 across 4 overlapping frames.
    // JUCE performRealOnlyInverseTransform does not apply 1/N, so the round-trip
    // gain is N × 1.5 = 1536.  normScale = 1/1536 restores unity gain.
    float sumW2 = 0.f;
    const int center = kFFTSize / 2;
    for (int r = 0; r < kFFTSize / kHop; ++r)
    {
        int frameN = center - r * kHop;
        if (frameN < 0)  frameN += kFFTSize; // periodic wrap
        sumW2 += window[static_cast<size_t> (frameN)] * window[static_cast<size_t> (frameN)];
    }
    normScale = 1.f / (static_cast<float> (kFFTSize) * juce::jmax (0.001f, sumW2));
}

void SpectralRepairModule::StftEngine::reset () noexcept
{
    std::fill (inputFifo.begin (),    inputFifo.end (),    0.f);
    std::fill (outputFifo.begin (),   outputFifo.end (),   0.f);
    std::fill (analysisFrame.begin(), analysisFrame.end(), 0.f);
    std::fill (synthesisAcc.begin (), synthesisAcc.end (), 0.f);
    inputPos     = 0;
    outputPos    = 0;
    synthesisPos = 0;
}

float SpectralRepairModule::StftEngine::processSample (float x,
                                                        float attenGain,
                                                        int   loIdx,
                                                        int   hiIdx,
                                                        bool  pushSpectro,
                                                        MagFrame* spectroOut) noexcept
{
    inputFifo [static_cast<size_t> (inputPos++)]  = x;
    const float out = outputFifo[static_cast<size_t> (outputPos++)];

    if (inputPos < kHop)
        return out;

    // ── We have kHop new samples: fire an analysis frame ──────────────────

    inputPos  = 0;
    outputPos = 0;

    // Shift analysis frame left by kHop and append new inputFifo
    std::copy (analysisFrame.begin () + kHop, analysisFrame.end (),
               analysisFrame.begin ());
    std::copy (inputFifo.begin (), inputFifo.end (),
               analysisFrame.begin () + (kFFTSize - kHop));

    // Apply Hann analysis window and zero-pad imaginary half
    for (int n = 0; n < kFFTSize; ++n)
        fftBuf[static_cast<size_t> (n)] =
            analysisFrame[static_cast<size_t> (n)] * window[static_cast<size_t> (n)];
    std::fill (fftBuf.begin () + kFFTSize, fftBuf.end (), 0.f);

    fft->performRealOnlyForwardTransform (fftBuf.data ());

    // Optionally capture magnitude spectrum for display (before attenuation)
    if (pushSpectro && spectroOut != nullptr)
    {
        const float magNorm = 1.f / static_cast<float> (kFFTSize);
        for (int k = 0; k < kNumBins; ++k)
        {
            const float re = fftBuf[static_cast<size_t> (2 * k)];
            const float im = fftBuf[static_cast<size_t> (2 * k + 1)];
            (*spectroOut)[static_cast<size_t> (k)] =
                std::sqrt (re * re + im * im) * magNorm;
        }
    }

    // Attenuate bins in [loIdx, hiIdx] (both positive and conjugate negative)
    const int clampedLo  = juce::jlimit (0, kFFTSize / 2, loIdx);
    const int clampedHi  = juce::jlimit (0, kFFTSize / 2, hiIdx);
    for (int k = clampedLo; k <= clampedHi; ++k)
    {
        fftBuf[static_cast<size_t> (2 * k)]     *= attenGain;
        fftBuf[static_cast<size_t> (2 * k + 1)] *= attenGain;
        if (k > 0 && k < kFFTSize / 2)
        {
            const int kNeg = kFFTSize - k;
            fftBuf[static_cast<size_t> (2 * kNeg)]     *= attenGain;
            fftBuf[static_cast<size_t> (2 * kNeg + 1)] *= attenGain;
        }
    }

    fft->performRealOnlyInverseTransform (fftBuf.data ());

    // Apply Hann synthesis window
    for (int n = 0; n < kFFTSize; ++n)
        fftBuf[static_cast<size_t> (n)] *= window[static_cast<size_t> (n)];

    // OLA: add synthesis frame into accumulator at synthesisPos
    const int accSize = static_cast<int> (synthesisAcc.size ());
    for (int n = 0; n < kFFTSize; ++n)
        synthesisAcc[static_cast<size_t> ((synthesisPos + n) % accSize)] +=
            fftBuf[static_cast<size_t> (n)];

    // Read kHop normalised output samples into outputFifo, then zero that region
    for (int n = 0; n < kHop; ++n)
    {
        const int idx = (synthesisPos + n) % accSize;
        outputFifo[static_cast<size_t> (n)] = synthesisAcc[static_cast<size_t> (idx)] * normScale;
        synthesisAcc[static_cast<size_t> (idx)] = 0.f;
    }

    synthesisPos = (synthesisPos + kHop) % accSize;
    return out; // current call still returns the pre-advance output
}

// ── SpectralRepairModule ──────────────────────────────────────────────────────

SpectralRepairModule::SpectralRepairModule () = default;

void SpectralRepairModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    const int nCh = static_cast<int> (spec.numChannels);

    channels.resize (static_cast<size_t> (nCh));
    for (auto& ch : channels)
        ch.prepare (sampleRate);
}

void SpectralRepairModule::reset ()
{
    for (auto& ch : channels) ch.reset ();
}

void SpectralRepairModule::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const float sr        = static_cast<float> (sampleRate);
    const float attenGain = juce::Decibels::decibelsToGain (-attenDb.load ());
    const int   loIdx     = static_cast<int> (freqLo.load () * kFFTSize / sr);
    const int   hiIdx     = static_cast<int> (freqHi.load () * kFFTSize / sr);

    const int nSamples = buffer.getNumSamples ();
    const int nCh      = juce::jmin (buffer.getNumChannels (), static_cast<int> (channels.size ()));

    MagFrame spectroFrame;

    for (int i = 0; i < nSamples; ++i)
    {
        // Channel 0: process and capture spectrogram whenever the FFT fires
        if (nCh > 0)
        {
            const float out = channels[0].processSample (
                buffer.getSample (0, i), attenGain, loIdx, hiIdx,
                true, &spectroFrame);

            // processSample resets inputPos to 0 when the FFT fires
            if (channels[0].inputPos == 0)
            {
                int s1, b1, s2, b2;
                spectroFifo.prepareToWrite (1, s1, b1, s2, b2);
                if (b1 > 0)
                {
                    spectroRing[static_cast<size_t> (s1)] = spectroFrame;
                    spectroFifo.finishedWrite (1);
                }
            }

            buffer.setSample (0, i, out);
        }

        // Remaining channels
        for (int ch = 1; ch < nCh; ++ch)
        {
            const float out = channels[static_cast<size_t> (ch)].processSample (
                buffer.getSample (ch, i), attenGain, loIdx, hiIdx, false, nullptr);
            buffer.setSample (ch, i, out);
        }
    }
}

bool SpectralRepairModule::drainSpectro (MagFrame& out) noexcept
{
    int s1, b1, s2, b2;
    spectroFifo.prepareToRead (1, s1, b1, s2, b2);
    if (b1 == 0)
        return false;
    out = spectroRing[static_cast<size_t> (s1)];
    spectroFifo.finishedRead (1);
    return true;
}

void SpectralRepairModule::getState (juce::ValueTree& tree) const
{
    tree.setProperty ("freqLo",  freqLo.load (),  nullptr);
    tree.setProperty ("freqHi",  freqHi.load (),  nullptr);
    tree.setProperty ("attenDb", attenDb.load (), nullptr);
}

void SpectralRepairModule::setState (const juce::ValueTree& tree)
{
    freqLo.store  (static_cast<float> (tree.getProperty ("freqLo",    80.f)));
    freqHi.store  (static_cast<float> (tree.getProperty ("freqHi",  8000.f)));
    attenDb.store (static_cast<float> (tree.getProperty ("attenDb",   12.f)));
}
