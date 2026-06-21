#include "MultibandCompressorModule.h"
#include <cmath>

MultibandCompressorModule::MultibandCompressorModule () = default;

void MultibandCompressorModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    const float sr = static_cast<float> (sampleRate);

    for (auto& s : stages) { s.lp.reset (); s.hp.reset (); }

    const float xos[3] = { xo1.load (), xo2.load (), xo3.load () };
    for (int i = 0; i < 3; ++i)
    {
        stages[static_cast<size_t> (i)].lp.setLP (sr, xos[i]);
        stages[static_cast<size_t> (i)].hp.setHP (sr, xos[i]);
        lastXo[i] = xos[i];
    }

    for (int b = 0; b < kNumBands; ++b)
    {
        auto& band = bands[static_cast<size_t> (b)];
        envs[static_cast<size_t> (b)].setAttack  (sr, juce::jmax (0.1f, band.attackMs.load ()));
        envs[static_cast<size_t> (b)].setRelease (sr, juce::jmax (1.f,  band.releaseMs.load ()));
    }
    smoothGR.fill (0.f);

    for (auto& buf : bandBufs)
        buf.setSize (static_cast<int> (spec.numChannels),
                     static_cast<int> (spec.maximumBlockSize));
}

void MultibandCompressorModule::reset ()
{
    for (auto& s : stages) { s.lp.reset (); s.hp.reset (); }
    for (auto& e : envs) e.reset ();
    smoothGR.fill (0.f);
    std::fill (lastXo, lastXo + 3, -1.f);
}

void MultibandCompressorModule::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const float sr       = static_cast<float> (sampleRate);
    const int   nSamples = buffer.getNumSamples ();
    const int   nCh      = juce::jmin (buffer.getNumChannels (), 2); // crossover wired for stereo

    // Detect and apply crossover changes
    const float xos[3] = { xo1.load (), xo2.load (), xo3.load () };
    for (int i = 0; i < 3; ++i)
    {
        if (xos[i] != lastXo[i])
        {
            stages[static_cast<size_t> (i)].lp.setLP (sr, xos[i]);
            stages[static_cast<size_t> (i)].hp.setHP (sr, xos[i]);
            lastXo[i] = xos[i];
        }
    }

    // Snapshot band params (avoid per-sample atomics)
    float thresh[4], rat[4], atk[4], rel[4], makeup[4];
    bool  bEnabled[4];
    for (int b = 0; b < kNumBands; ++b)
    {
        auto& band     = bands[static_cast<size_t> (b)];
        thresh[b]      = band.thresholdDb.load ();
        rat[b]         = juce::jmax (1.f, band.ratio.load ());
        atk[b]         = juce::jmax (0.1f, band.attackMs.load ());
        rel[b]         = juce::jmax (1.f,  band.releaseMs.load ());
        makeup[b]      = juce::Decibels::decibelsToGain (band.makeupDb.load ());
        bEnabled[b]    = band.enabled.load ();
        envs[static_cast<size_t> (b)].setAttack  (sr, atk[b]);
        envs[static_cast<size_t> (b)].setRelease (sr, rel[b]);
    }

    const float outGain = juce::Decibels::decibelsToGain (outputDb.load ());
    float maxGR[4] = {};

    for (int i = 0; i < nSamples; ++i)
    {
        // Split each channel into 4 bands via the LR4 cascade tree
        float band[4][2] = {};
        for (int ch = 0; ch < nCh; ++ch)
        {
            const float x = buffer.getSample (ch, i);
            const float s1 = stages[0].hp.process (x,  ch);
            const float s2 = stages[1].hp.process (s1, ch);
            band[0][ch] = stages[0].lp.process (x,  ch);
            band[1][ch] = stages[1].lp.process (s1, ch);
            band[2][ch] = stages[2].lp.process (s2, ch);
            band[3][ch] = stages[2].hp.process (s2, ch);
        }

        // Per-band compression: linked stereo (peak drives GR)
        float bandGain[4];
        for (int b = 0; b < kNumBands; ++b)
        {
            float peak = 0.f;
            for (int ch = 0; ch < nCh; ++ch)
                peak = juce::jmax (peak, std::abs (band[b][ch]));

            const float envLin = envs[static_cast<size_t> (b)].process (peak);
            const float envDb  = envLin > 0.f ? juce::Decibels::gainToDecibels (envLin) : -96.f;

            float gr = 0.f;
            const float over = envDb - thresh[b];
            if (over > 0.f)
                gr = over * (1.f / rat[b] - 1.f);

            const float targetGRGain = juce::Decibels::decibelsToGain (gr);

            const float smoothGRGain = juce::Decibels::decibelsToGain (smoothGR[b]);
            const float coeff = targetGRGain < smoothGRGain
                ? 1.f - std::exp (-1.f / (sr * atk[b] * 0.001f))
                : 1.f - std::exp (-1.f / (sr * rel[b] * 0.001f));
            const float newGRGain = smoothGRGain + coeff * (targetGRGain - smoothGRGain);
            smoothGR[b]  = newGRGain > 0.f ? juce::Decibels::gainToDecibels (newGRGain) : -96.f;

            bandGain[b]  = bEnabled[b] ? newGRGain * makeup[b] : 1.f;
            maxGR[b]     = juce::jmin (maxGR[b], smoothGR[b]);
        }

        // Sum bands back to output
        for (int ch = 0; ch < nCh; ++ch)
        {
            float out = 0.f;
            for (int b = 0; b < kNumBands; ++b)
                out += band[b][ch] * bandGain[b];
            buffer.setSample (ch, i, out * outGain);
        }
    }

    for (int b = 0; b < kNumBands; ++b)
        bands[static_cast<size_t> (b)].currentGR.store (maxGR[b], std::memory_order_relaxed);
}

void MultibandCompressorModule::getState (juce::ValueTree& tree) const
{
    tree.setProperty ("output", outputDb.load (), nullptr);
    tree.setProperty ("xo1",    xo1.load (),      nullptr);
    tree.setProperty ("xo2",    xo2.load (),      nullptr);
    tree.setProperty ("xo3",    xo3.load (),      nullptr);
    for (int b = 0; b < kNumBands; ++b)
    {
        const auto& band = bands[static_cast<size_t> (b)];
        const auto  p    = "b" + juce::String (b) + "_";
        tree.setProperty (p + "enabled", band.enabled.load ()      ? 1 : 0, nullptr);
        tree.setProperty (p + "thresh",  band.thresholdDb.load (),          nullptr);
        tree.setProperty (p + "ratio",   band.ratio.load (),                nullptr);
        tree.setProperty (p + "attack",  band.attackMs.load (),             nullptr);
        tree.setProperty (p + "release", band.releaseMs.load (),            nullptr);
        tree.setProperty (p + "makeup",  band.makeupDb.load (),             nullptr);
    }
}

void MultibandCompressorModule::setState (const juce::ValueTree& tree)
{
    outputDb.store (static_cast<float> (tree.getProperty ("output", 0.f)));
    xo1.store      (static_cast<float> (tree.getProperty ("xo1",  250.f)));
    xo2.store      (static_cast<float> (tree.getProperty ("xo2", 1000.f)));
    xo3.store      (static_cast<float> (tree.getProperty ("xo3", 4000.f)));
    for (int b = 0; b < kNumBands; ++b)
    {
        auto&      band = bands[static_cast<size_t> (b)];
        const auto p    = "b" + juce::String (b) + "_";
        band.enabled.store     (static_cast<int>  (tree.getProperty (p + "enabled",  1)) != 0);
        band.thresholdDb.store (static_cast<float>(tree.getProperty (p + "thresh", -20.f)));
        band.ratio.store       (static_cast<float>(tree.getProperty (p + "ratio",    4.f)));
        band.attackMs.store    (static_cast<float>(tree.getProperty (p + "attack",  10.f)));
        band.releaseMs.store   (static_cast<float>(tree.getProperty (p + "release",100.f)));
        band.makeupDb.store    (static_cast<float>(tree.getProperty (p + "makeup",   0.f)));
    }
}
