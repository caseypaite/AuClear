#include "ReverbModule.h"

void ReverbModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 2;
    reverb.prepare (monoSpec);

    wetBuf.setSize (2, (int)spec.maximumBlockSize);

    const int maxPD = static_cast<int> (0.5 * spec.sampleRate); // 500 ms max
    preDelayBuf.assign (2, std::vector<float> (static_cast<size_t> (maxPD), 0.f));
    pdWritePos = 0;
}

void ReverbModule::reset ()
{
    reverb.reset ();
    for (auto& ch : preDelayBuf)
        std::fill (ch.begin (), ch.end (), 0.f);
    pdWritePos = 0;
}

void ReverbModule::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::dsp::Reverb::Parameters p;
    p.roomSize = juce::jlimit (0.f, 1.f, roomSize.load ());
    p.damping = juce::jlimit (0.f, 1.f, damping.load ());
    p.width = juce::jlimit (0.f, 1.f, width.load ());
    p.wetLevel = 1.f;
    p.dryLevel = 0.f;
    reverb.setParameters (p);

    const int nSamples = buffer.getNumSamples ();
    const int nCh = std::min (buffer.getNumChannels (), 2);
    const float wet = juce::jlimit (0.f, 1.f, wetDryMix.load ());
    const float dry = 1.f - wet;

    const int pdLen = static_cast<int> (preDelayBuf[0].size ());
    const int pdSamp = juce::jlimit (
        0, pdLen - 1,
        static_cast<int> (preDelay.load () * 0.001f * static_cast<float> (sampleRate)));

    // Build wet input with pre-delay
    for (int ch = 0; ch < nCh; ++ch)
    {
        for (int i = 0; i < nSamples; ++i)
        {
            preDelayBuf[static_cast<size_t> (ch)][static_cast<size_t> (pdWritePos)] =
                buffer.getSample (ch, i);
            const int readPos = (pdWritePos - pdSamp + pdLen) % pdLen;
            wetBuf.setSample (ch, i,
                              preDelayBuf[static_cast<size_t> (ch)][static_cast<size_t> (readPos)]);
            pdWritePos = (pdWritePos + 1) % pdLen;
        }
    }

    // Apply reverb
    auto block = juce::dsp::AudioBlock<float> (wetBuf).getSubBlock (0, (size_t)nSamples);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    reverb.process (ctx);

    // Mix wet with dry
    for (int ch = 0; ch < nCh; ++ch)
        for (int i = 0; i < nSamples; ++i)
            buffer.setSample (ch, i,
                              dry * buffer.getSample (ch, i) + wet * wetBuf.getSample (ch, i));
}

void ReverbModule::getState (juce::ValueTree& tree) const
{
    tree.setProperty ("roomSize", roomSize.load (), nullptr);
    tree.setProperty ("damping", damping.load (), nullptr);
    tree.setProperty ("width", width.load (), nullptr);
    tree.setProperty ("preDelay", preDelay.load (), nullptr);
    tree.setProperty ("wetDry", wetDryMix.load (), nullptr);
}

void ReverbModule::setState (const juce::ValueTree& tree)
{
    roomSize.store (static_cast<float> (tree.getProperty ("roomSize", 0.5f)));
    damping.store (static_cast<float> (tree.getProperty ("damping", 0.5f)));
    width.store (static_cast<float> (tree.getProperty ("width", 1.f)));
    preDelay.store (static_cast<float> (tree.getProperty ("preDelay", 0.f)));
    wetDryMix.store (static_cast<float> (tree.getProperty ("wetDry", 0.3f)));
}
