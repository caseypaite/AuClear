#pragma once
#include <JuceHeader.h>
#include <vector>

class ProcessorRack;

/**
 * Preset save / load / enumerate for AuClear.
 *
 * Factory presets are embedded as XML strings and are always available.
 * User presets are stored as XML files in the user application data directory.
 */
class PresetManager
{
  public:
    struct PresetLists
    {
        juce::StringArray factory;
        juce::StringArray user;
    };

    explicit PresetManager (ProcessorRack& rackRef);

    PresetLists getLists () const;

    static juce::File getPresetDirectory ();

    bool load   (const juce::String& name);
    bool save   (const juce::String& name);
    bool remove (const juce::String& name);

  private:
    static const std::vector<std::pair<juce::String, juce::String>>& factoryTable ();

    ProcessorRack& rack;
};
