#include "DenoiseModule.h"

DenoiseModule::DenoiseModule () = default;

void DenoiseModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    engine.prepare (spec.sampleRate, (int)spec.maximumBlockSize);
}

void DenoiseModule::reset ()
{
    engine.reset ();
}

void DenoiseModule::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (bypassed.load ())
        return;
    engine.process (buffer, strength.load (), listenToRemoved.load ());
}

void DenoiseModule::setModelFile (const juce::File& file)
{
    currentModelFile = file;
    const bool ok = engine.loadModel (file);
    if (modelStatusChanged)
        juce::MessageManager::callAsync (
            [this]
            {
                if (modelStatusChanged)
                    modelStatusChanged ();
            });
    (void)ok;
}

void DenoiseModule::getState (juce::ValueTree& tree) const
{
    tree.setProperty ("strength", (float)strength.load (), nullptr);
    tree.setProperty ("listenToRemoved", (bool)listenToRemoved.load (), nullptr);
    tree.setProperty ("modelPath", currentModelFile.getFullPathName (), nullptr);
}

void DenoiseModule::setState (const juce::ValueTree& tree)
{
    strength.store ((float)tree.getProperty ("strength", 1.0f));
    listenToRemoved.store ((bool)tree.getProperty ("listenToRemoved", false));

    const auto path = tree.getProperty ("modelPath", "").toString ();
    if (path.isNotEmpty ())
    {
        const juce::File f (path);
        if (f.existsAsFile ())
            setModelFile (f);
    }
}
