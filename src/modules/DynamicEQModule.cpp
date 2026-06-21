#include "DynamicEQModule.h"
#include <cmath>

static constexpr float kDetectBoostDb = 12.f; // boost applied during detection

DynamicEQModule::DynamicEQModule ()
{
    // Spread default frequencies across the audible spectrum
    bands[0].freq.store (100.f);
    bands[1].freq.store (500.f);
    bands[2].freq.store (2500.f);
    bands[3].freq.store (8000.f);
}

void DynamicEQModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    const float sr = (float)sampleRate;
    for (int b = 0; b < kNumBands; ++b)
    {
        const float f = bands[b].freq.load ();
        const float q = bands[b].q.load ();
        dsp[b].detect.setCoeffs (BiquadFilter::makeBell (sr, f, q, kDetectBoostDb));
        const float totalGain = bands[b].gainDb.load ();
        for (auto& filt : dsp[b].proc)
            filt.setCoeffs (BiquadFilter::makeBell (sr, f, q, totalGain));
        dsp[b].env.prepare (sr, bands[b].attackMs.load (), bands[b].releaseMs.load ());
        dsp[b].smoothGR = 0.f;
    }
}

void DynamicEQModule::reset ()
{
    for (auto& d : dsp) d.reset ();
}

void DynamicEQModule::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const float sr    = (float)sampleRate;
    const int nSamps  = buffer.getNumSamples ();
    const int nCh     = juce::jmin (2, buffer.getNumChannels ());

    for (int b = 0; b < kNumBands; ++b)
    {
        auto& band = bands[b];
        auto& d    = dsp[b];

        if (! band.enabled.load (std::memory_order_relaxed))
        {
            band.currentGR.store (0.f, std::memory_order_relaxed);
            continue;
        }

        const float f       = juce::jlimit (20.f, 20000.f, band.freq.load ());
        const float q       = juce::jlimit (0.1f, 10.f,   band.q.load ());
        const float gainDb  = band.gainDb.load ();
        const float thresh  = band.threshDb.load ();
        const float range   = juce::jmax (0.f, band.rangeDb.load ());

        d.env.setAttack  (sr, juce::jmax (0.1f, band.attackMs.load ()));
        d.env.setRelease (sr, juce::jmax (1.f,  band.releaseMs.load ()));

        // Update detection filter coefficients
        d.detect.setCoeffs (BiquadFilter::makeBell (sr, f, q, kDetectBoostDb));

        // ── Per-sample: detection + GR compute ────────────────────────────────
        float blockGR = 0.f;

        for (int i = 0; i < nSamps; ++i)
        {
            // Mono detection: sum channels, halve amplitude
            float mono = 0.f;
            for (int ch = 0; ch < nCh; ++ch)
                mono += buffer.getSample (ch, i);
            mono /= (float)nCh;

            const float detected = d.detect.process (mono);
            const float envLin   = d.env.process (std::abs (detected));
            const float envDb    = envLin > 1e-9f ? juce::Decibels::gainToDecibels (envLin) : -96.f;

            // Correct for the +12 dB detect boost so threshold is calibrated to the input level
            const float calibratedDb = envDb - kDetectBoostDb;

            const float over = range > 0.f ? calibratedDb - thresh : 0.f;
            const float targetGR = over > 0.f ? -juce::jmin (range, over) : 0.f;

            const float coeff = targetGR < d.smoothGR
                ? 1.f - std::exp (-1.f / (sr * juce::jmax (0.1f, band.attackMs.load ()) * 0.001f))
                : 1.f - std::exp (-1.f / (sr * juce::jmax (1.f,  band.releaseMs.load ()) * 0.001f));
            d.smoothGR += coeff * (targetGR - d.smoothGR);
            blockGR = juce::jmin (blockGR, d.smoothGR);
        }

        band.currentGR.store (blockGR, std::memory_order_relaxed);

        // Update processing filter with block-level GR (smooth enough — no per-sample update needed)
        const float totalGain = gainDb + d.smoothGR;
        const auto procCoeffs = BiquadFilter::makeBell (sr, f, q, totalGain);
        for (auto& filt : d.proc) filt.setCoeffs (procCoeffs);

        // ── Apply processing filters per channel ──────────────────────────────
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int i = 0; i < nSamps; ++i)
                data[i] = d.proc[(size_t)ch].process (data[i]);
        }
    }
}

void DynamicEQModule::getState (juce::ValueTree& tree) const
{
    for (int b = 0; b < kNumBands; ++b)
    {
        const auto& band = bands[b];
        const juce::String px = "b" + juce::String (b) + "_";
        tree.setProperty (px + "enabled", band.enabled.load (), nullptr);
        tree.setProperty (px + "freq",    band.freq.load (),    nullptr);
        tree.setProperty (px + "q",       band.q.load (),       nullptr);
        tree.setProperty (px + "gain",    band.gainDb.load (),  nullptr);
        tree.setProperty (px + "thresh",  band.threshDb.load (), nullptr);
        tree.setProperty (px + "range",   band.rangeDb.load (), nullptr);
        tree.setProperty (px + "attack",  band.attackMs.load (), nullptr);
        tree.setProperty (px + "release", band.releaseMs.load (), nullptr);
    }
}

void DynamicEQModule::setState (const juce::ValueTree& tree)
{
    static const float defFreqs[4] = {100.f, 500.f, 2500.f, 8000.f};
    for (int b = 0; b < kNumBands; ++b)
    {
        auto& band = bands[b];
        const juce::String px = "b" + juce::String (b) + "_";
        band.enabled.store (static_cast<bool>  (tree.getProperty (px + "enabled", true)));
        band.freq.store    (static_cast<float> (tree.getProperty (px + "freq",    defFreqs[b])));
        band.q.store       (static_cast<float> (tree.getProperty (px + "q",       1.f)));
        band.gainDb.store  (static_cast<float> (tree.getProperty (px + "gain",    0.f)));
        band.threshDb.store (static_cast<float> (tree.getProperty (px + "thresh", 0.f)));
        band.rangeDb.store (static_cast<float> (tree.getProperty (px + "range",   0.f)));
        band.attackMs.store (static_cast<float> (tree.getProperty (px + "attack", 10.f)));
        band.releaseMs.store (static_cast<float> (tree.getProperty (px + "release", 200.f)));
    }
}
