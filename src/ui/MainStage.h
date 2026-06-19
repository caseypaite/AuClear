#pragma once
#include <JuceHeader.h>
#include <memory>
#include "../engine/RackModule.h"

/**
 * Centre panel: shows the selected module's full control panel.
 * Routes to per-module Panel components based on ModuleType.
 */
class MainStage : public juce::Component
{
  public:
    MainStage () = default;

    void paint (juce::Graphics& g) override;
    void resized () override;
    void showModule (RackModule* module);

  private:
    RackModule* currentModule{nullptr};
    std::unique_ptr<juce::Component> activePanel;

    static constexpr juce::uint32 kBg = 0xff16181d;
    static constexpr juce::uint32 kPanel = 0xff1e2128;
    static constexpr juce::uint32 kAccent = 0xff28e0c8;
    static constexpr juce::uint32 kTextHi = 0xffe8eaed;
    static constexpr juce::uint32 kTextLo = 0xff9aa0ab;
    static constexpr juce::uint32 kDivider = 0xff2a2e37;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainStage)
};
