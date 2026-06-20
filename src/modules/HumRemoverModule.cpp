#include "HumRemoverModule.h"
#include <cmath>

HumRemoverModule::HumRemoverModule () = default;

void HumRemoverModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    dirty      = true;
    reset ();
}

void HumRemoverModule::reset ()
{
    for (auto& ch : notchFilters)
        for (auto& f : ch)
            f.reset ();
    dirty = true;
}

void HumRemoverModule::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (bypassed.load ()) return;

    const float f0 = fundamental.load ();
    const float d  = depth.load ();
    const int   nh = (int) harmonics.load ();

    if (std::abs (f0 - lastFundamental) > 0.1f || std::abs (d - lastDepth) > 0.01f
        || nh != lastHarmonics || dirty)
    {
        lastFundamental = f0;
        lastDepth       = d;
        lastHarmonics   = nh;
        dirty           = false;
        rebuildFilters ();
    }

    const int numCh = std::min (buffer.getNumChannels (), 2);
    const int n     = buffer.getNumSamples ();

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* data = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            float s = data[i];
            for (int h = 0; h < lastHarmonics && h < kMaxHarmonics; ++h)
                s = notchFilters[(size_t) ch][(size_t) h].process (s);
            data[i] = s;
        }
    }
}

void HumRemoverModule::rebuildFilters ()
{
    // Use Bell (peaking) filters with negative gain for a configurable-depth
    // notch: depth 0 = passthrough, depth 60 dB ≈ near-perfect notch.
    // Q=10 keeps the notch narrow enough to avoid audible tonal damage.
    constexpr float kQ = 10.f;

    const float f0 = lastFundamental;
    const float d  = -std::abs (lastDepth); // negative = cut
    const int   nh = std::min (lastHarmonics, kMaxHarmonics);
    const float sr = (float) sampleRate;

    for (int ch = 0; ch < 2; ++ch)
    {
        for (int h = 0; h < nh; ++h)
        {
            const float freq = f0 * (float) (h + 1);
            if (freq >= sr * 0.499f) // above Nyquist — bypass
            {
                notchFilters[(size_t) ch][(size_t) h].setCoeffs ({1.f, 0.f, 0.f, 0.f, 0.f});
                continue;
            }
            notchFilters[(size_t) ch][(size_t) h].setCoeffs (
                BiquadFilter::makeBell (sr, freq, kQ, d));
        }
        // Passthrough for unused stages
        for (int h = nh; h < kMaxHarmonics; ++h)
            notchFilters[(size_t) ch][(size_t) h].setCoeffs ({1.f, 0.f, 0.f, 0.f, 0.f});
    }
}

void HumRemoverModule::getState (juce::ValueTree& tree) const
{
    tree.setProperty ("fundamental", (float) fundamental.load (), nullptr);
    tree.setProperty ("depth",       (float) depth.load (),       nullptr);
    tree.setProperty ("harmonics",   (float) harmonics.load (),   nullptr);
}

void HumRemoverModule::setState (const juce::ValueTree& tree)
{
    fundamental.store ((float) tree.getProperty ("fundamental", 50.f));
    depth.store       ((float) tree.getProperty ("depth",       30.f));
    harmonics.store   ((float) tree.getProperty ("harmonics",   4.f));
    dirty = true;
}
