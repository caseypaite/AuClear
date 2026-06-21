#include "TransientShaperModule.h"

void TransientShaperModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    // Fast follower: 3 ms attack, 30 ms release
    fastAttCoef = envCoef (3.f,   sampleRate);
    fastRelCoef = envCoef (30.f,  sampleRate);

    // Slow follower: 150 ms attack, 400 ms release
    slowAttCoef = envCoef (150.f, sampleRate);
    slowRelCoef = envCoef (400.f, sampleRate);

    reset ();
}

void TransientShaperModule::reset ()
{
    fastEnv.fill (0.f);
    slowEnv.fill (0.f);
    displayGainDb = 0.f;
    gainRide.store (0.f);
}

void TransientShaperModule::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int numCh  = buffer.getNumChannels ();
    const int numSmp = buffer.getNumSamples ();
    if (numCh == 0 || numSmp == 0) return;

    const float attSens  = attackSens.load ();
    const float susSens  = sustainSens.load ();
    const float outGainLin = dbToLinear (outputGain.load ());

    // Compute per-sample linked-stereo gain
    float sumGainDb = 0.f;

    for (int s = 0; s < numSmp; ++s)
    {
        // Pick loudest channel for the sidechain
        float peak = 0.f;
        for (int ch = 0; ch < numCh; ++ch)
            peak = std::max (peak, std::abs (buffer.getSample (ch, s)));

        // Use channel 0 envelope state for the sidechain (linked stereo)
        const float fa = peak > fastEnv[0] ? fastAttCoef : fastRelCoef;
        fastEnv[0] = fa * fastEnv[0] + (1.f - fa) * peak;

        const float sa = peak > slowEnv[0] ? slowAttCoef : slowRelCoef;
        slowEnv[0] = sa * slowEnv[0] + (1.f - sa) * peak;

        // Transient amount: positive on attacks, negative in sustain/tail
        const float transient = fastEnv[0] - slowEnv[0];  // [-1, +1] range
        const float sustain   = slowEnv[0];                // [0, +1]

        // Map sensitivity (±12 dB) via the detected envelope components
        // attackSens scales the transient differential (attack vs. release feel)
        // sustainSens scales the body level
        const float gainDb = attSens * transient * 3.f + susSens * sustain * 3.f;
        const float gainLin = dbToLinear (gainDb) * outGainLin;

        // Apply linked stereo gain
        for (int ch = 0; ch < numCh; ++ch)
            buffer.setSample (ch, s, buffer.getSample (ch, s) * gainLin);

        sumGainDb += gainDb;
    }

    // Smooth display gain for the GUI (running average of this block + decay)
    const float blockGainDb = sumGainDb / static_cast<float> (numSmp);
    displayGainDb = 0.8f * displayGainDb + 0.2f * blockGainDb;
    gainRide.store (displayGainDb);
}

void TransientShaperModule::getState (juce::ValueTree& tree) const
{
    tree.setProperty ("attack",  attackSens.load (),  nullptr);
    tree.setProperty ("sustain", sustainSens.load (), nullptr);
    tree.setProperty ("output",  outputGain.load (),  nullptr);
}

void TransientShaperModule::setState (const juce::ValueTree& tree)
{
    attackSens.store  (static_cast<float> (tree.getProperty ("attack",  0.f)));
    sustainSens.store (static_cast<float> (tree.getProperty ("sustain", 0.f)));
    outputGain.store  (static_cast<float> (tree.getProperty ("output",  0.f)));
}
