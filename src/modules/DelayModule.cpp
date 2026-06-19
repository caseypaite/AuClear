#include "DelayModule.h"
#include <cmath>

void DelayModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    const int nCh = static_cast<int> (spec.numChannels);

    bufLen = static_cast<int> (kMaxTimeMs * 0.001f * static_cast<float> (sampleRate)) + 8;
    delayBuf.assign (static_cast<size_t> (nCh),
                     std::vector<float> (static_cast<size_t> (bufLen), 0.f));
    writePos = 0;
    lpZ.fill (0.f);
}

void DelayModule::reset ()
{
    for (auto& ch : delayBuf)
        std::fill (ch.begin (), ch.end (), 0.f);
    writePos = 0;
    lpZ.fill (0.f);
}

void DelayModule::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const float delayTime = juce::jlimit (0.f, kMaxTimeMs, timeMs.load ());
    const int delaySamples =
        static_cast<int> (delayTime * 0.001f * static_cast<float> (sampleRate));
    const float fb = juce::jlimit (0.f, 0.99f, feedback.load ());
    const float wetMix = juce::jlimit (0.f, 1.f, mix.load ());
    const bool pp = pingPong.load ();

    // One-pole LP on feedback for high-frequency damping
    const float lp = juce::jlimit (100.f, 20000.f, lpCutoff.load ());
    const float lpCoeff = 1.f - std::exp (-2.f * juce::MathConstants<float>::pi * lp /
                                          static_cast<float> (sampleRate));

    const int nSamples = buffer.getNumSamples ();
    const int nCh = std::min (buffer.getNumChannels (), static_cast<int> (delayBuf.size ()));

    for (int i = 0; i < nSamples; ++i)
    {
        const int readPos = (writePos - delaySamples + bufLen) % bufLen;

        for (int ch = 0; ch < nCh; ++ch)
        {
            const auto idx = static_cast<size_t> (ch);
            const float dry = buffer.getSample (ch, i);

            // In ping-pong, each channel reads from the other
            const int readCh = pp ? (1 - ch) : ch;
            const float delayOut =
                delayBuf[static_cast<size_t> (readCh)][static_cast<size_t> (readPos)];

            // Apply LP to feedback
            lpZ[idx] += lpCoeff * (delayOut - lpZ[idx]);

            delayBuf[idx][static_cast<size_t> (writePos)] = dry + lpZ[idx] * fb;

            buffer.setSample (ch, i, dry + wetMix * (delayOut - dry));
        }

        writePos = (writePos + 1) % bufLen;
    }
}

void DelayModule::getState (juce::ValueTree& tree) const
{
    tree.setProperty ("time", timeMs.load (), nullptr);
    tree.setProperty ("feedback", feedback.load (), nullptr);
    tree.setProperty ("mix", mix.load (), nullptr);
    tree.setProperty ("pingPong", pingPong.load (), nullptr);
    tree.setProperty ("lpCutoff", lpCutoff.load (), nullptr);
}

void DelayModule::setState (const juce::ValueTree& tree)
{
    timeMs.store (static_cast<float> (tree.getProperty ("time", 250.f)));
    feedback.store (static_cast<float> (tree.getProperty ("feedback", 0.4f)));
    mix.store (static_cast<float> (tree.getProperty ("mix", 0.5f)));
    pingPong.store (static_cast<bool> (tree.getProperty ("pingPong", false)));
    lpCutoff.store (static_cast<float> (tree.getProperty ("lpCutoff", 6000.f)));
}
