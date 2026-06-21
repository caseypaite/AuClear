#include "MainStage.h"
#include "panels/GainPanel.h"
#include "panels/EQPanel.h"
#include "panels/GatePanel.h"
#include "panels/CompressorPanel.h"
#include "panels/LimiterPanel.h"
#include "panels/ReverbPanel.h"
#include "panels/DelayPanel.h"
#include "panels/SaturatorPanel.h"
#include "panels/UtilityPanel.h"
#include "panels/DenoisePanel.h"
#include "panels/HumRemoverPanel.h"
#include "../modules/GainModule.h"
#include "../modules/ParametricEQModule.h"
#include "../modules/GateModule.h"
#include "../modules/CompressorModule.h"
#include "../modules/LimiterModule.h"
#include "../modules/ReverbModule.h"
#include "../modules/DelayModule.h"
#include "../modules/SaturatorModule.h"
#include "../modules/UtilityModule.h"
#include "../modules/DenoiseModule.h"
#include "../modules/HumRemoverModule.h"
#include "../modules/DeEsserModule.h"
#include "../modules/DynamicEQModule.h"
#include "../modules/MultibandCompressorModule.h"
#include "../modules/SpectralRepairModule.h"
#include "panels/DeEsserPanel.h"
#include "panels/DynamicEQPanel.h"
#include "panels/MultibandCompressorPanel.h"
#include "panels/SpectralRepairPanel.h"

void MainStage::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (kBg));

    if (currentModule == nullptr)
    {
        g.setColour (juce::Colour (kTextLo));
        g.setFont (juce::FontOptions (14.f));
        g.drawText ("Select a module to edit it here", getLocalBounds (),
                    juce::Justification::centred);
        return;
    }

    // Module panel header bar
    auto hdr = getLocalBounds ().removeFromTop (44);
    g.setColour (juce::Colour (kPanel));
    g.fillRect (hdr);
    g.setColour (juce::Colour (kDivider));
    g.fillRect (0, hdr.getBottom () - 1, getWidth (), 1);

    g.setFont (juce::Font (juce::FontOptions (16.f).withStyle ("Bold")));
    g.setColour (juce::Colour (kTextHi));
    g.drawText (currentModule->name (), hdr.reduced (16, 0), juce::Justification::centredLeft);

    // Module type badge
    g.setFont (juce::FontOptions (11.f));
    g.setColour (juce::Colour (kAccent));
    g.drawText (currentModule->uid.substring (0, 8), hdr.reduced (16, 0),
                juce::Justification::centredRight);
}

void MainStage::resized ()
{
    if (activePanel)
    {
        auto b = getLocalBounds ();
        if (currentModule)
            b.removeFromTop (44);
        activePanel->setBounds (b);
    }
}

void MainStage::showModule (RackModule* module)
{
    currentModule = module;

    if (activePanel)
    {
        removeChildComponent (activePanel.get ());
        activePanel.reset ();
    }

    if (module != nullptr)
    {
        switch (module->type ())
        {
        case ModuleType::Gain:
            if (auto* m = dynamic_cast<GainModule*> (module))
                activePanel = std::make_unique<GainPanel> (*m);
            break;

        case ModuleType::ParametricEQ:
            if (auto* m = dynamic_cast<ParametricEQModule*> (module))
                activePanel = std::make_unique<EQPanel> (*m);
            break;

        case ModuleType::Gate:
            if (auto* m = dynamic_cast<GateModule*> (module))
                activePanel = std::make_unique<GatePanel> (*m);
            break;

        case ModuleType::Compressor:
            if (auto* m = dynamic_cast<CompressorModule*> (module))
                activePanel = std::make_unique<CompressorPanel> (*m);
            break;

        case ModuleType::Limiter:
            if (auto* m = dynamic_cast<LimiterModule*> (module))
                activePanel = std::make_unique<LimiterPanel> (*m);
            break;

        case ModuleType::Reverb:
            if (auto* m = dynamic_cast<ReverbModule*> (module))
                activePanel = std::make_unique<ReverbPanel> (*m);
            break;

        case ModuleType::Delay:
            if (auto* m = dynamic_cast<DelayModule*> (module))
                activePanel = std::make_unique<DelayPanel> (*m);
            break;

        case ModuleType::Saturator:
            if (auto* m = dynamic_cast<SaturatorModule*> (module))
                activePanel = std::make_unique<SaturatorPanel> (*m);
            break;

        case ModuleType::Utility:
            if (auto* m = dynamic_cast<UtilityModule*> (module))
                activePanel = std::make_unique<UtilityPanel> (*m);
            break;

        case ModuleType::Denoise:
            if (auto* m = dynamic_cast<DenoiseModule*> (module))
                activePanel = std::make_unique<DenoisePanel> (*m);
            break;

        case ModuleType::HumRemover:
            if (auto* m = dynamic_cast<HumRemoverModule*> (module))
                activePanel = std::make_unique<HumRemoverPanel> (*m);
            break;

        case ModuleType::DeEsser:
            if (auto* m = dynamic_cast<DeEsserModule*> (module))
                activePanel = std::make_unique<DeEsserPanel> (*m);
            break;

        case ModuleType::DynamicEQ:
            if (auto* m = dynamic_cast<DynamicEQModule*> (module))
                activePanel = std::make_unique<DynamicEQPanel> (*m);
            break;

        case ModuleType::MultibandComp:
            if (auto* m = dynamic_cast<MultibandCompressorModule*> (module))
                activePanel = std::make_unique<MultibandCompressorPanel> (*m);
            break;

        case ModuleType::SpectralRepair:
            if (auto* m = dynamic_cast<SpectralRepairModule*> (module))
                activePanel = std::make_unique<SpectralRepairPanel> (*m);
            break;
        }

        if (activePanel)
            addAndMakeVisible (*activePanel);
    }

    resized ();
    repaint ();
}
