#pragma once
#include <JuceHeader.h>
#include <functional>
#include "AnalogPalette.h"

class HeaderComponent : public juce::Component
{
  public:
    HeaderComponent ();

    void paint (juce::Graphics& g) override;
    void resized () override;
    void mouseDown (const juce::MouseEvent& e) override;

    std::function<void ()>               onBypassToggled;
    std::function<void ()>               onUndoClicked;
    std::function<void (juce::String)>   onPresetChosen;   // preset name
    std::function<void (int)>            onThemeChanged;   // theme index 0–3

    void setCpuLoad (float fraction) { cpuLoad = fraction; repaint (); }
    void setLatencyMs (double ms)    { latencyMs = ms;     repaint (); }
    void setBypassActive (bool active)
    {
        bypassButton.setToggleState (active, juce::dontSendNotification);
    }

    void setCurrentPreset (const juce::String& name)
    {
        currentPreset = name;
        presetButton.setButtonText (name);
    }

    void setPresetList (const juce::StringArray& factory,
                        const juce::StringArray& user)
    {
        factoryPresets = factory;
        userPresets    = user;
    }

  private:
    void showPresetPopup ();
    void paintThemeSwatch (juce::Graphics& g, int themeIdx,
                           juce::Rectangle<int> bounds) const;
    juce::Rectangle<int> swatchBounds (int themeIdx) const;

    juce::TextButton bypassButton{"Bypass"};
    juce::TextButton undoButton{"Undo"};
    juce::TextButton presetButton{"Init"};   // preset selector

    float  cpuLoad{0.f};
    double latencyMs{0.0};
    juce::String      currentPreset{"Init"};
    juce::StringArray factoryPresets, userPresets;
};
