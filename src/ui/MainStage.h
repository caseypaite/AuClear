#pragma once
#include <JuceHeader.h>
#include <memory>
#include "../engine/RackModule.h"
#include "AnalogPalette.h"

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

    static constexpr juce::uint32 kBg      = AP::kBgBase;
    static constexpr juce::uint32 kPanel   = AP::kBgPanel;
    static constexpr juce::uint32 kAccent  = AP::kAccentBr;
    static constexpr juce::uint32 kTextHi  = AP::kTxtHi;
    static constexpr juce::uint32 kTextLo  = AP::kTxtLo;
    static constexpr juce::uint32 kDivider = AP::kDiv;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainStage)
};
