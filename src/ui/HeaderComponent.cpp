#include "HeaderComponent.h"

HeaderComponent::HeaderComponent ()
{
    addAndMakeVisible (bypassButton);
    bypassButton.setClickingTogglesState (true);
    bypassButton.onClick = [this]
    {
        if (onBypassToggled) onBypassToggled ();
    };

    addAndMakeVisible (undoButton);
    undoButton.onClick = [this]
    {
        if (onUndoClicked) onUndoClicked ();
    };

    addAndMakeVisible (presetButton);
    presetButton.onClick = [this] { showPresetPopup (); };
    presetButton.setTooltip ("Click to load or save a preset");
}

void HeaderComponent::paint (juce::Graphics& g)
{
    // Background with subtle gradient for depth
    juce::ColourGradient bg (
        juce::Colour (AP::kBgPanel), 0.f, 0.f,
        juce::Colour (AP::kBgBase),  0.f, (float)getHeight (),
        false);
    g.setGradientFill (bg);
    g.fillAll ();

    // Bottom divider
    g.setColour (juce::Colour (AP::kDiv));
    g.fillRect (0, getHeight () - 1, getWidth (), 1);

    // Brand name
    g.setFont (juce::Font (juce::FontOptions (18.f).withStyle ("Bold")));
    g.setColour (juce::Colour (AP::kAccentBr));
    g.drawText ("AuClear", 14, 0, 100, getHeight (), juce::Justification::centredLeft);

    // Status: CPU + latency (right-aligned, leave room for buttons)
    const auto statusStr =
        juce::String::formatted ("CPU %.0f%%   lat %.1f ms", cpuLoad * 100.f, latencyMs);
    g.setFont (juce::FontOptions (11.f));
    g.setColour (juce::Colour (AP::kTxtLo));
    g.drawText (statusStr, 0, 0, getWidth () - 8, getHeight (),
                juce::Justification::centredRight);
}

void HeaderComponent::resized ()
{
    auto b = getLocalBounds ().reduced (8, 6);
    b.removeFromLeft (120); // brand name region

    bypassButton.setBounds (b.removeFromLeft (70));
    b.removeFromLeft (6);
    undoButton.setBounds (b.removeFromLeft (52));
    b.removeFromLeft (8);
    presetButton.setBounds (b.removeFromLeft (130));
}

void HeaderComponent::showPresetPopup ()
{
    juce::PopupMenu menu;

    if (factoryPresets.size () > 0)
    {
        menu.addSectionHeader ("Factory");
        int id = 1;
        for (const auto& name : factoryPresets)
            menu.addItem (id++, name, true, name == currentPreset);
    }

    if (userPresets.size () > 0)
    {
        menu.addSeparator ();
        menu.addSectionHeader ("User");
        int id = 1001;
        for (const auto& name : userPresets)
            menu.addItem (id++, name, true, name == currentPreset);
    }

    menu.addSeparator ();
    menu.addItem (9001, "Save preset...");
    if (userPresets.contains (currentPreset))
        menu.addItem (9002, "Delete \"" + currentPreset + "\"");

    menu.showMenuAsync (
        juce::PopupMenu::Options ().withTargetComponent (presetButton),
        [this] (int result)
        {
            if (result == 0)
                return;

            juce::String chosen;
            if (result >= 1 && result < 1000)
                chosen = factoryPresets[result - 1];
            else if (result >= 1001 && result < 9000)
                chosen = userPresets[result - 1001];
            else if (result == 9001)
            {
                auto* aw = new juce::AlertWindow ("Save Preset",
                                                  "Enter a name for this preset:",
                                                  juce::MessageBoxIconType::NoIcon);
                aw->addTextEditor ("name", currentPreset, "");
                aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
                aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                aw->enterModalState (
                    true,
                    juce::ModalCallbackFunction::create (
                        [this, aw] (int r)
                        {
                            if (r == 1)
                            {
                                const auto name = aw->getTextEditorContents ("name").trim ();
                                if (name.isNotEmpty () && onPresetChosen)
                                    onPresetChosen ("__save__:" + name);
                            }
                            delete aw;
                        }),
                    true);
                return;
            }
            else if (result == 9002)
            {
                if (onPresetChosen)
                    onPresetChosen ("__delete__:" + currentPreset);
                return;
            }

            if (chosen.isNotEmpty () && onPresetChosen)
                onPresetChosen (chosen);
        });
}
