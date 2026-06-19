#include "GainModule.h"

GainModule::GainModule() = default;

void GainModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    gain.prepare (spec);
    gain.setRampDurationSeconds (0.01);
    gain.setGainDecibels (gainDb.load());
}

void GainModule::reset()
{
    gain.reset();
}

void GainModule::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    gain.setGainDecibels (gainDb.load (std::memory_order_relaxed));
    auto block = juce::dsp::AudioBlock<float> (buffer);
    gain.process (juce::dsp::ProcessContextReplacing<float> (block));
}

void GainModule::getState (juce::ValueTree& tree) const
{
    tree.setProperty ("gainDb", gainDb.load(), nullptr);
}

void GainModule::setState (const juce::ValueTree& tree)
{
    gainDb.store ((float) tree.getProperty ("gainDb", 0.0f));
    gain.setGainDecibels (gainDb.load());
}
