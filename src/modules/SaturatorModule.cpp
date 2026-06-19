#include "SaturatorModule.h"
#include <cmath>

float SaturatorModule::softClip (float x, float drv) noexcept
{
    const float d = juce::jlimit (1.f, 20.f, drv);
    return std::tanh (d * x) / std::tanh (d);
}

float SaturatorModule::tapeClip (float x, float drv) noexcept
{
    // Tape: subtle compression + odd-order harmonics
    const float d = juce::jlimit (1.f, 10.f, drv);
    const float driven = d * x;
    if (std::abs (driven) >= 1.f)
        return (driven > 0.f ? 1.f : -1.f) * (2.f / 3.f);
    return driven - (driven * driven * driven) / 3.f;
}

float SaturatorModule::tubeClip (float x, float drv) noexcept
{
    // Tube: asymmetric, slightly different positive/negative clipping
    const float d = juce::jlimit (1.f, 15.f, drv);
    const float driven = d * x;
    if (driven >= 0.f)
        return driven / (1.f + driven);
    else
        return driven / (1.f - 0.5f * driven);
}

void SaturatorModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    toneZ.fill (0.f);
    dryBuf.setSize ((int)spec.numChannels, (int)spec.maximumBlockSize);
}

void SaturatorModule::reset ()
{
    toneZ.fill (0.f);
}

void SaturatorModule::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const float drv = juce::jlimit (1.f, 20.f, drive.load ());
    const float tone_ = juce::jlimit (0.f, 1.f, tone.load ());
    const float wetMix = juce::jlimit (0.f, 1.f, mix.load ());
    const auto st = static_cast<SatType> (satType.load ());

    // Tone: one-pole shelf — positive tone boosts highs in output
    // Cutoff sweeps 500 Hz (tone=0) to 6000 Hz (tone=1)
    const float toneFc = 500.f + tone_ * 5500.f;
    const float toneAlpha = 1.f - std::exp (-2.f * juce::MathConstants<float>::pi * toneFc /
                                            static_cast<float> (sampleRate));

    const int nSamples = buffer.getNumSamples ();
    const int nCh = std::min (buffer.getNumChannels (), 2);

    // Save dry
    for (int ch = 0; ch < nCh; ++ch)
        dryBuf.copyFrom (ch, 0, buffer, ch, 0, nSamples);

    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        const auto idx = static_cast<size_t> (ch);

        for (int i = 0; i < nSamples; ++i)
        {
            float s = data[i];

            switch (st)
            {
            case SatType::Soft:
                s = softClip (s, drv);
                break;
            case SatType::Tape:
                s = tapeClip (s, drv);
                break;
            case SatType::Tube:
                s = tubeClip (s, drv);
                break;
            }

            // Tone filter (LP → blended with original for tone shaping)
            toneZ[idx] += toneAlpha * (s - toneZ[idx]);
            // tone=0: pure LP output; tone=1: more high content (original saturated)
            s = toneZ[idx] + tone_ * (s - toneZ[idx]);

            // Gain compensation: lower the output by drive factor to normalise
            s /= juce::jmax (1.f, std::log2 (drv + 1.f));

            data[i] = dryBuf.getSample (ch, i) + wetMix * (s - dryBuf.getSample (ch, i));
        }
    }
}

void SaturatorModule::getState (juce::ValueTree& tree) const
{
    tree.setProperty ("drive", drive.load (), nullptr);
    tree.setProperty ("tone", tone.load (), nullptr);
    tree.setProperty ("mix", mix.load (), nullptr);
    tree.setProperty ("satType", satType.load (), nullptr);
}

void SaturatorModule::setState (const juce::ValueTree& tree)
{
    drive.store (static_cast<float> (tree.getProperty ("drive", 1.f)));
    tone.store (static_cast<float> (tree.getProperty ("tone", 0.5f)));
    mix.store (static_cast<float> (tree.getProperty ("mix", 0.5f)));
    satType.store (static_cast<int> (tree.getProperty ("satType", 0)));
}
