#include "GateModule.h"
#include <cmath>

void GateModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    const float sr = static_cast<float> (sampleRate);
    env.prepare (sr, attackMs.load (), releaseMs.load ());
    attackCoeff = 1.f - std::exp (-1.f / (sr * attackMs.load () * 0.001f));
    releaseCoeff = 1.f - std::exp (-1.f / (sr * releaseMs.load () * 0.001f));
    smoothGain = 1.f;
    holdCounter = 0.f;
    inHold = false;
}

void GateModule::reset ()
{
    env.reset ();
    smoothGain = 1.f;
    holdCounter = 0.f;
    inHold = false;
}

void GateModule::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const float sr = static_cast<float> (sampleRate);
    const float thresh = juce::Decibels::decibelsToGain (thresholdDb.load ());
    const float range = juce::Decibels::decibelsToGain (juce::jlimit (-96.f, 0.f, rangeDb.load ()));
    const float holdSamples = holdMs.load () * 0.001f * sr;

    attackCoeff = 1.f - std::exp (-1.f / (sr * juce::jmax (0.01f, attackMs.load ()) * 0.001f));
    releaseCoeff = 1.f - std::exp (-1.f / (sr * juce::jmax (1.f, releaseMs.load ()) * 0.001f));

    const int nSamples = buffer.getNumSamples ();
    const int nCh = buffer.getNumChannels ();

    float maxGR = 0.f;

    for (int i = 0; i < nSamples; ++i)
    {
        // Sidechain: peak of all channels
        float peak = 0.f;
        for (int ch = 0; ch < nCh; ++ch)
            peak = juce::jmax (peak, std::abs (buffer.getSample (ch, i)));

        const float envVal = env.process (peak);

        float targetGain;
        if (envVal >= thresh)
        {
            targetGain = 1.f;
            holdCounter = holdSamples;
            inHold = false;
        }
        else if (holdCounter > 0.f)
        {
            targetGain = 1.f;
            holdCounter -= 1.f;
            inHold = true;
        }
        else
        {
            targetGain = range;
            inHold = false;
        }

        const float coeff = (targetGain > smoothGain) ? attackCoeff : releaseCoeff;
        smoothGain += coeff * (targetGain - smoothGain);

        for (int ch = 0; ch < nCh; ++ch)
            buffer.setSample (ch, i, buffer.getSample (ch, i) * smoothGain);

        const float gr = (smoothGain > 0.f) ? 20.f * std::log10 (smoothGain) : -96.f;
        maxGR = juce::jmin (maxGR, gr);
    }

    currentGR.store (maxGR, std::memory_order_relaxed);
}

void GateModule::getState (juce::ValueTree& tree) const
{
    tree.setProperty ("threshold", thresholdDb.load (), nullptr);
    tree.setProperty ("range", rangeDb.load (), nullptr);
    tree.setProperty ("attack", attackMs.load (), nullptr);
    tree.setProperty ("hold", holdMs.load (), nullptr);
    tree.setProperty ("release", releaseMs.load (), nullptr);
}

void GateModule::setState (const juce::ValueTree& tree)
{
    thresholdDb.store (static_cast<float> (tree.getProperty ("threshold", -40.f)));
    rangeDb.store (static_cast<float> (tree.getProperty ("range", -80.f)));
    attackMs.store (static_cast<float> (tree.getProperty ("attack", 1.f)));
    holdMs.store (static_cast<float> (tree.getProperty ("hold", 50.f)));
    releaseMs.store (static_cast<float> (tree.getProperty ("release", 200.f)));
}
