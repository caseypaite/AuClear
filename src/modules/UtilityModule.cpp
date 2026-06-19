#include "UtilityModule.h"
#include <cmath>

void UtilityModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    smoothGainL = juce::Decibels::decibelsToGain (gainDb.load ());
    smoothGainR = smoothGainL;
}

void UtilityModule::reset ()
{
    smoothGainL = juce::Decibels::decibelsToGain (gainDb.load ());
    smoothGainR = smoothGainL;
}

void UtilityModule::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const float targetGain = juce::Decibels::decibelsToGain (gainDb.load ());
    const float pan_ = juce::jlimit (-1.f, 1.f, pan.load ());
    // Equal-power pan: left = cos(angle), right = sin(angle), angle 0..pi/2
    const float angle = (pan_ + 1.f) * 0.25f * juce::MathConstants<float>::pi;
    const float targetL = targetGain * std::cos (angle) * (invertPhaseL.load () ? -1.f : 1.f);
    const float targetR = targetGain * std::sin (angle) * (invertPhaseR.load () ? -1.f : 1.f);

    // 5 ms smooth
    const float coeff = 1.f - std::exp (-1.f / (static_cast<float> (sampleRate) * 0.005f));
    const bool mono = monoSum.load ();

    const int nSamples = buffer.getNumSamples ();
    const int nCh = buffer.getNumChannels ();

    for (int i = 0; i < nSamples; ++i)
    {
        smoothGainL += coeff * (targetL - smoothGainL);
        smoothGainR += coeff * (targetR - smoothGainR);

        if (mono && nCh >= 2)
        {
            const float m = (buffer.getSample (0, i) + buffer.getSample (1, i)) * 0.5f;
            buffer.setSample (0, i, m * smoothGainL);
            buffer.setSample (1, i, m * smoothGainR);
        }
        else
        {
            if (nCh >= 1)
                buffer.setSample (0, i, buffer.getSample (0, i) * smoothGainL);
            if (nCh >= 2)
                buffer.setSample (1, i, buffer.getSample (1, i) * smoothGainR);
        }
    }
}

void UtilityModule::getState (juce::ValueTree& tree) const
{
    tree.setProperty ("gain", gainDb.load (), nullptr);
    tree.setProperty ("pan", pan.load (), nullptr);
    tree.setProperty ("phaseL", invertPhaseL.load (), nullptr);
    tree.setProperty ("phaseR", invertPhaseR.load (), nullptr);
    tree.setProperty ("mono", monoSum.load (), nullptr);
}

void UtilityModule::setState (const juce::ValueTree& tree)
{
    gainDb.store (static_cast<float> (tree.getProperty ("gain", 0.f)));
    pan.store (static_cast<float> (tree.getProperty ("pan", 0.f)));
    invertPhaseL.store (static_cast<bool> (tree.getProperty ("phaseL", false)));
    invertPhaseR.store (static_cast<bool> (tree.getProperty ("phaseR", false)));
    monoSum.store (static_cast<bool> (tree.getProperty ("mono", false)));
}
