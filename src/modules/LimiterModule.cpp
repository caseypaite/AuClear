#include "LimiterModule.h"
#include <cmath>

void LimiterModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    numChannels = static_cast<int> (spec.numChannels);

    const float lookahead = juce::jlimit (0.1f, 20.f, lookaheadMs.load ());
    latSamples = static_cast<int> (lookahead * 0.001f * sampleRate);
    lookaheadBuf = latSamples + static_cast<int> (spec.maximumBlockSize) + 4;

    delayLines.resize (static_cast<size_t> (numChannels));
    for (auto& dl : delayLines)
    {
        dl.assign (static_cast<size_t> (lookaheadBuf), 0.f);
    }
    gainLine.assign (static_cast<size_t> (lookaheadBuf), 1.f);
    writePos = 0;
    smoothGain = 1.f;

    const float rel = juce::jmax (1.f, releaseMs.load ());
    releaseCoeff = 1.f - std::exp (-1.f / (static_cast<float> (sampleRate) * rel * 0.001f));
}

void LimiterModule::reset ()
{
    for (auto& dl : delayLines)
        std::fill (dl.begin (), dl.end (), 0.f);
    std::fill (gainLine.begin (), gainLine.end (), 1.f);
    writePos = 0;
    smoothGain = 1.f;
}

void LimiterModule::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (delayLines.empty () || lookaheadBuf == 0)
        return;

    const float ceiling = juce::Decibels::decibelsToGain (ceilingDb.load ());
    const float rel = juce::jmax (1.f, releaseMs.load ());
    releaseCoeff = 1.f - std::exp (-1.f / (static_cast<float> (sampleRate) * rel * 0.001f));

    const int nSamples = buffer.getNumSamples ();
    const int nCh = std::min (buffer.getNumChannels (), numChannels);
    float maxGR = 0.f;

    for (int i = 0; i < nSamples; ++i)
    {
        // Write current samples into delay line
        for (int ch = 0; ch < nCh; ++ch)
            delayLines[static_cast<size_t> (ch)][static_cast<size_t> (writePos)] =
                buffer.getSample (ch, i);

        // Find peak at lookahead position (the 'future' relative to read position)
        float peak = 0.f;
        for (int ch = 0; ch < nCh; ++ch)
            peak = juce::jmax (
                peak,
                std::abs (delayLines[static_cast<size_t> (ch)][static_cast<size_t> (writePos)]));

        // Gain needed to keep peak at or below ceiling
        const float targetGain = (peak > ceiling && peak > 0.f) ? ceiling / peak : 1.f;

        // Fast attack (instant), smooth release
        if (targetGain < smoothGain)
            smoothGain = targetGain;
        else
            smoothGain += releaseCoeff * (targetGain - smoothGain);

        gainLine[static_cast<size_t> (writePos)] = smoothGain;

        // Read delayed output
        const int readPos = (writePos - latSamples + lookaheadBuf) % lookaheadBuf;
        const float g = gainLine[static_cast<size_t> (readPos)];

        for (int ch = 0; ch < nCh; ++ch)
            buffer.setSample (
                ch, i, delayLines[static_cast<size_t> (ch)][static_cast<size_t> (readPos)] * g);

        const float grDb = g < 1.f && g > 0.f ? juce::Decibels::gainToDecibels (g) : 0.f;
        maxGR = juce::jmin (maxGR, grDb);

        writePos = (writePos + 1) % lookaheadBuf;
    }

    currentGR.store (maxGR, std::memory_order_relaxed);
}

void LimiterModule::getState (juce::ValueTree& tree) const
{
    tree.setProperty ("ceiling", ceilingDb.load (), nullptr);
    tree.setProperty ("release", releaseMs.load (), nullptr);
    tree.setProperty ("lookahead", lookaheadMs.load (), nullptr);
}

void LimiterModule::setState (const juce::ValueTree& tree)
{
    ceilingDb.store (static_cast<float> (tree.getProperty ("ceiling", -0.3f)));
    releaseMs.store (static_cast<float> (tree.getProperty ("release", 50.f)));
    lookaheadMs.store (static_cast<float> (tree.getProperty ("lookahead", 2.f)));
}
