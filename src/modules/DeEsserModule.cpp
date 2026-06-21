#include "DeEsserModule.h"
#include <cmath>

void DeEsserModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    const float sr = (float)sampleRate;
    const auto hpC = BiquadFilter::makeHighPass (sr, freq.load (), 0.707f);
    for (auto& f : detectHp) f.setCoeffs (hpC);
    env.prepare (sr, attackMs.load (), releaseMs.load ());
    smoothGR = 0.f;
}

void DeEsserModule::reset ()
{
    for (auto& f : detectHp) f.reset ();
    env.reset ();
    smoothGR = 0.f;
}

void DeEsserModule::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const float sr    = (float)sampleRate;
    const int nSamps  = buffer.getNumSamples ();
    const int nCh     = juce::jmin (2, buffer.getNumChannels ());
    const bool doListen = listen.load (std::memory_order_relaxed);

    const float f   = juce::jlimit (200.f, 18000.f, freq.load ());
    const auto  hpC = BiquadFilter::makeHighPass (sr, f, 0.707f);
    for (auto& hp : detectHp) hp.setCoeffs (hpC);

    env.setAttack  (sr, juce::jmax (0.1f, attackMs.load ()));
    env.setRelease (sr, juce::jmax (5.f,  releaseMs.load ()));

    const float thresh = thresholdDb.load ();
    const float range  = juce::jmax (0.f, rangeDb.load ());

    float maxGR = 0.f;

    for (int i = 0; i < nSamps; ++i)
    {
        // Run HP detection filter per channel; keep peak across channels
        float hpOut[2] = {};
        float peak = 0.f;
        for (int ch = 0; ch < nCh; ++ch)
        {
            hpOut[ch] = detectHp[(size_t)ch].process (buffer.getSample (ch, i));
            peak = juce::jmax (peak, std::abs (hpOut[ch]));
        }

        // Envelope follower → dB
        const float envLin = env.process (peak);
        const float envDb  = envLin > 1e-9f ? juce::Decibels::gainToDecibels (envLin) : -96.f;

        // Infinite-ratio gain computer, clamped to `range`
        const float over = envDb - thresh;
        const float targetGR = over > 0.f ? -juce::jmin (range, over) : 0.f;

        // Smooth GR (attack faster than release)
        const float coeff = targetGR < smoothGR
            ? 1.f - std::exp (-1.f / (sr * juce::jmax (0.1f, attackMs.load ()) * 0.001f))
            : 1.f - std::exp (-1.f / (sr * juce::jmax (5.f, releaseMs.load ()) * 0.001f));
        smoothGR += coeff * (targetGR - smoothGR);
        maxGR = juce::jmin (maxGR, smoothGR);

        if (doListen)
        {
            // Let the user hear exactly what the detector sees
            for (int ch = 0; ch < nCh; ++ch)
                buffer.setSample (ch, i, hpOut[ch]);
        }
        else
        {
            const float gain = juce::Decibels::decibelsToGain (smoothGR);
            for (int ch = 0; ch < nCh; ++ch)
                buffer.setSample (ch, i, buffer.getSample (ch, i) * gain);
        }
    }

    currentGR.store (maxGR, std::memory_order_relaxed);
}

void DeEsserModule::getState (juce::ValueTree& tree) const
{
    tree.setProperty ("freq",      freq.load (),       nullptr);
    tree.setProperty ("threshold", thresholdDb.load (), nullptr);
    tree.setProperty ("range",     rangeDb.load (),    nullptr);
    tree.setProperty ("attack",    attackMs.load (),    nullptr);
    tree.setProperty ("release",   releaseMs.load (),   nullptr);
}

void DeEsserModule::setState (const juce::ValueTree& tree)
{
    freq.store        (static_cast<float> (tree.getProperty ("freq",      7500.f)));
    thresholdDb.store (static_cast<float> (tree.getProperty ("threshold", -20.f)));
    rangeDb.store     (static_cast<float> (tree.getProperty ("range",     12.f)));
    attackMs.store    (static_cast<float> (tree.getProperty ("attack",    1.f)));
    releaseMs.store   (static_cast<float> (tree.getProperty ("release",   60.f)));
}
