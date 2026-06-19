#include "ParametricEQModule.h"

ParametricEQModule::ParametricEQModule ()
{
    // Default band layout: HP, LowShelf, 4×Bell, HighShelf, LP
    bands[0].type.store ((int)BandType::HighPass);
    bands[0].freq.store (80.f);
    bands[0].q.store (0.707f);

    bands[1].type.store ((int)BandType::LowShelf);
    bands[1].freq.store (100.f);
    bands[1].gain.store (0.f);

    bands[2].type.store ((int)BandType::Bell);
    bands[2].freq.store (400.f);
    bands[2].gain.store (0.f);
    bands[2].q.store (1.f);

    bands[3].type.store ((int)BandType::Bell);
    bands[3].freq.store (1000.f);
    bands[3].gain.store (0.f);
    bands[3].q.store (1.f);

    bands[4].type.store ((int)BandType::Bell);
    bands[4].freq.store (3000.f);
    bands[4].gain.store (0.f);
    bands[4].q.store (1.f);

    bands[5].type.store ((int)BandType::Bell);
    bands[5].freq.store (8000.f);
    bands[5].gain.store (0.f);
    bands[5].q.store (1.f);

    bands[6].type.store ((int)BandType::HighShelf);
    bands[6].freq.store (12000.f);
    bands[6].gain.store (0.f);

    bands[7].type.store ((int)BandType::LowPass);
    bands[7].freq.store (18000.f);
    bands[7].q.store (0.707f);
}

void ParametricEQModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    for (auto& ch : filters)
        for (auto& f : ch)
            f.reset ();
    updateCoefficients ();
}

void ParametricEQModule::reset ()
{
    for (auto& ch : filters)
        for (auto& f : ch)
            f.reset ();
}

void ParametricEQModule::updateCoefficients ()
{
    const float sr = static_cast<float> (sampleRate);

    for (int b = 0; b < kNumBands; ++b)
    {
        const auto bt = static_cast<BandType> (bands[static_cast<size_t> (b)].type.load ());
        const float freq = bands[static_cast<size_t> (b)].freq.load ();
        const float gain = bands[static_cast<size_t> (b)].gain.load ();
        const float q = juce::jmax (0.1f, bands[static_cast<size_t> (b)].q.load ());

        BiquadFilter::Coeffs c;
        switch (bt)
        {
        case BandType::Off:
            c = {};
            break;
        case BandType::Bell:
            c = BiquadFilter::makeBell (sr, freq, q, gain);
            break;
        case BandType::LowShelf:
            c = BiquadFilter::makeLowShelf (sr, freq, gain);
            break;
        case BandType::HighShelf:
            c = BiquadFilter::makeHighShelf (sr, freq, gain);
            break;
        case BandType::LowPass:
            c = BiquadFilter::makeLowPass (sr, freq, q);
            break;
        case BandType::HighPass:
            c = BiquadFilter::makeHighPass (sr, freq, q);
            break;
        case BandType::Notch:
            c = BiquadFilter::makeNotch (sr, freq, q);
            break;
        }

        for (auto& ch : filters)
            ch[static_cast<size_t> (b)].setCoeffs (c);
    }
}

void ParametricEQModule::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    updateCoefficients ();

    const int nSamples = buffer.getNumSamples ();
    const int nCh = std::min (buffer.getNumChannels (), 2);

    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < nSamples; ++i)
        {
            float s = data[i];
            for (int b = 0; b < kNumBands; ++b)
            {
                const auto bt = static_cast<BandType> (bands[static_cast<size_t> (b)].type.load ());
                if (bt != BandType::Off)
                    s = filters[static_cast<size_t> (ch)][static_cast<size_t> (b)].process (s);
            }
            data[i] = s;
        }
    }
}

void ParametricEQModule::getState (juce::ValueTree& tree) const
{
    for (int b = 0; b < kNumBands; ++b)
    {
        juce::ValueTree bandTree ("Band");
        bandTree.setProperty ("type", bands[static_cast<size_t> (b)].type.load (), nullptr);
        bandTree.setProperty ("freq", bands[static_cast<size_t> (b)].freq.load (), nullptr);
        bandTree.setProperty ("gain", bands[static_cast<size_t> (b)].gain.load (), nullptr);
        bandTree.setProperty ("q", bands[static_cast<size_t> (b)].q.load (), nullptr);
        tree.addChild (bandTree, -1, nullptr);
    }
}

void ParametricEQModule::setState (const juce::ValueTree& tree)
{
    int b = 0;
    for (const auto& child : tree)
    {
        if (b >= kNumBands)
            break;
        auto idx = static_cast<size_t> (b);
        bands[idx].type.store (static_cast<int> (child.getProperty ("type", 0)));
        bands[idx].freq.store (static_cast<float> (child.getProperty ("freq", 1000.f)));
        bands[idx].gain.store (static_cast<float> (child.getProperty ("gain", 0.f)));
        bands[idx].q.store (static_cast<float> (child.getProperty ("q", 0.707f)));
        ++b;
    }
}
