#include "StereoWidthModule.h"
#include <cmath>

void StereoWidthModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    updateHpCoef (monoBelow.load ());
    reset ();
}

void StereoWidthModule::reset ()
{
    hpZ = 0.f;
}

void StereoWidthModule::updateHpCoef (float cutHz)
{
    if (cutHz < 1.f || sampleRate <= 0.0)
    {
        hpCoef = 0.f; // no HP filtering
        return;
    }
    // 1-pole HP: coef = exp(-2π * fc / fs), applied as y = coef*(y + x - xPrev)
    hpCoef = static_cast<float> (
        std::exp (-2.0 * juce::MathConstants<double>::pi * cutHz / sampleRate));
}

void StereoWidthModule::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int numCh  = buffer.getNumChannels ();
    if (numCh < 2) return; // mono passthrough

    const int numSmp = buffer.getNumSamples ();
    if (numSmp == 0) return;

    const float widthGain = width.load () * 0.01f; // 0..2
    const float sideMul   = widthGain;              // 0 = mono, 1 = unchanged, 2 = wide

    const float cutHz    = monoBelow.load ();
    const bool useHP = cutHz >= 1.f;
    if (useHP) updateHpCoef (cutHz);

    // Pan law: constant-power
    const float panVal    = juce::jlimit (-1.f, 1.f, pan.load ());
    const float panAngle  = (panVal + 1.f) * juce::MathConstants<float>::pi * 0.25f;
    const float gainL     = std::cos (panAngle);
    const float gainR     = std::sin (panAngle);

    float* dataL = buffer.getWritePointer (0);
    float* dataR = buffer.getWritePointer (1);

    for (int s = 0; s < numSmp; ++s)
    {
        const float l = dataL[s];
        const float r = dataR[s];

        // Encode M/S
        const float mid  = (l + r) * 0.5f;
        float       side = (l - r) * 0.5f;

        // Apply HP to Side channel for bass mono
        if (useHP && hpCoef > 0.f)
        {
            const float newHP = side - hpZ + hpCoef * hpZ;
            hpZ = side;
            side = newHP;
        }

        // Scale Side
        side *= sideMul;

        // Decode back to L/R
        const float outL = (mid + side) * gainL * juce::MathConstants<float>::sqrt2;
        const float outR = (mid - side) * gainR * juce::MathConstants<float>::sqrt2;

        dataL[s] = outL;
        dataR[s] = outR;
    }
}

void StereoWidthModule::getState (juce::ValueTree& tree) const
{
    tree.setProperty ("width",      width.load (),      nullptr);
    tree.setProperty ("monoBelow",  monoBelow.load (),  nullptr);
    tree.setProperty ("pan",        pan.load (),        nullptr);
}

void StereoWidthModule::setState (const juce::ValueTree& tree)
{
    width.store      (static_cast<float> (tree.getProperty ("width",     100.f)));
    monoBelow.store  (static_cast<float> (tree.getProperty ("monoBelow", 0.f)));
    pan.store        (static_cast<float> (tree.getProperty ("pan",       0.f)));
}
