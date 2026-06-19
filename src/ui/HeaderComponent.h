#pragma once
#include <JuceHeader.h>
#include <functional>

class HeaderComponent : public juce::Component
{
  public:
    HeaderComponent ();

    void paint (juce::Graphics& g) override;
    void resized () override;

    std::function<void ()> onBypassToggled;
    std::function<void ()> onUndoClicked;

    void setCpuLoad (float fraction)
    {
        cpuLoad = fraction;
        repaint ();
    }
    void setLatencyMs (double ms)
    {
        latencyMs = ms;
        repaint ();
    }
    void setBypassActive (bool active)
    {
        bypassButton.setToggleState (active, juce::dontSendNotification);
    }

  private:
    juce::TextButton bypassButton{"Bypass"};
    juce::TextButton undoButton{"Undo"};

    float cpuLoad{0.0f};
    double latencyMs{0.0};

    static constexpr juce::uint32 kBg = 0xff1e2128;
    static constexpr juce::uint32 kAccent = 0xff28e0c8;
    static constexpr juce::uint32 kTextHi = 0xffe8eaed;
    static constexpr juce::uint32 kTextLo = 0xff9aa0ab;
    static constexpr juce::uint32 kDivider = 0xff2a2e37;
};
