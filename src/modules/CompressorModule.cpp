#include "CompressorModule.h"
#include <cmath>

void CompressorModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    const float sr = static_cast<float> (sampleRate);
    env.prepare (sr, attackMs.load (), releaseMs.load ());
    smoothGR = 0.f;
    dryBuf.setSize ((int)spec.numChannels, (int)spec.maximumBlockSize);
}

void CompressorModule::reset ()
{
    env.reset ();
    smoothGR = 0.f;
}

void CompressorModule::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const float sr = static_cast<float> (sampleRate);
    env.setAttack (sr, juce::jmax (0.1f, attackMs.load ()));
    env.setRelease (sr, juce::jmax (1.f, releaseMs.load ()));

    const float thresh = thresholdDb.load ();
    const float rat = juce::jmax (1.f, ratio.load ());
    const float knee = juce::jmax (0.f, kneeDb.load ());
    const float makeup = juce::Decibels::decibelsToGain (makeupDb.load ());
    const float wetMix = juce::jlimit (0.f, 1.f, mix.load ());

    const int nSamples = buffer.getNumSamples ();
    const int nCh = buffer.getNumChannels ();

    // Copy dry signal
    for (int ch = 0; ch < nCh; ++ch)
        dryBuf.copyFrom (ch, 0, buffer, ch, 0, nSamples);

    float maxGR = 0.f;

    for (int i = 0; i < nSamples; ++i)
    {
        // Sidechain: peak of all channels in dB
        float peak = 0.f;
        for (int ch = 0; ch < nCh; ++ch)
            peak = juce::jmax (peak, std::abs (buffer.getSample (ch, i)));

        const float envLin = env.process (peak);
        const float envDb = envLin > 0.f ? juce::Decibels::gainToDecibels (envLin) : -96.f;

        // Gain computer with soft knee
        float gr = 0.f;
        const float over = envDb - thresh;
        if (knee > 0.f && over > -knee * 0.5f && over < knee * 0.5f)
        {
            // Soft knee region
            const float x = over + knee * 0.5f;
            gr = (1.f / rat - 1.f) * x * x / (2.f * knee);
        }
        else if (over > 0.f)
        {
            gr = over * (1.f / rat - 1.f);
        }

        const float targetGRGain = juce::Decibels::decibelsToGain (gr);

        // Smooth GR
        const float coeff =
            targetGRGain < juce::Decibels::decibelsToGain (smoothGR)
                ? 1.f - std::exp (-1.f / (sr * juce::jmax (0.1f, attackMs.load ()) * 0.001f))
                : 1.f - std::exp (-1.f / (sr * juce::jmax (1.f, releaseMs.load ()) * 0.001f));

        const float smoothGRGain = juce::Decibels::decibelsToGain (smoothGR);
        const float newGRGain = smoothGRGain + coeff * (targetGRGain - smoothGRGain);
        smoothGR = newGRGain > 0.f ? juce::Decibels::gainToDecibels (newGRGain) : -96.f;

        const float gainToApply = newGRGain * makeup;

        for (int ch = 0; ch < nCh; ++ch)
        {
            const float wet = buffer.getSample (ch, i) * gainToApply;
            const float dry = dryBuf.getSample (ch, i);
            buffer.setSample (ch, i, dry + wetMix * (wet - dry));
        }

        maxGR = juce::jmin (maxGR, smoothGR);
    }

    currentGR.store (maxGR, std::memory_order_relaxed);
}

void CompressorModule::getState (juce::ValueTree& tree) const
{
    tree.setProperty ("threshold", thresholdDb.load (), nullptr);
    tree.setProperty ("ratio", ratio.load (), nullptr);
    tree.setProperty ("knee", kneeDb.load (), nullptr);
    tree.setProperty ("attack", attackMs.load (), nullptr);
    tree.setProperty ("release", releaseMs.load (), nullptr);
    tree.setProperty ("makeup", makeupDb.load (), nullptr);
    tree.setProperty ("mix", mix.load (), nullptr);
    tree.setProperty ("character", character.load (), nullptr);
}

void CompressorModule::setState (const juce::ValueTree& tree)
{
    thresholdDb.store (static_cast<float> (tree.getProperty ("threshold", -20.f)));
    ratio.store (static_cast<float> (tree.getProperty ("ratio", 4.f)));
    kneeDb.store (static_cast<float> (tree.getProperty ("knee", 6.f)));
    attackMs.store (static_cast<float> (tree.getProperty ("attack", 10.f)));
    releaseMs.store (static_cast<float> (tree.getProperty ("release", 100.f)));
    makeupDb.store (static_cast<float> (tree.getProperty ("makeup", 0.f)));
    mix.store (static_cast<float> (tree.getProperty ("mix", 1.f)));
    character.store (static_cast<int> (tree.getProperty ("character", 0)));
}
