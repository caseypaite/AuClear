#include "HeaderComponent.h"

static constexpr int kSwatchSize = 14;
static constexpr int kSwatchGap  = 4;

HeaderComponent::HeaderComponent ()
{
    addAndMakeVisible (bypassButton);
    bypassButton.setClickingTogglesState (true);
    bypassButton.onClick = [this] { if (onBypassToggled) onBypassToggled (); };

    addAndMakeVisible (undoButton);
    undoButton.onClick = [this] { if (onUndoClicked) onUndoClicked (); };

    addAndMakeVisible (presetButton);
    presetButton.onClick = [this] { showPresetPopup (); };
    presetButton.setTooltip ("Click to load or save a preset");
}

void HeaderComponent::paint (juce::Graphics& g)
{
    juce::ColourGradient bg (
        juce::Colour (AP::kBgPanel), 0.f, 0.f,
        juce::Colour (AP::kBgBase),  0.f, (float) getHeight (), false);
    g.setGradientFill (bg);
    g.fillAll ();

    g.setColour (juce::Colour (AP::kDiv));
    g.fillRect (0, getHeight () - 1, getWidth (), 1);

    g.setFont (juce::Font (juce::FontOptions (18.f).withStyle ("Bold")));
    g.setColour (juce::Colour (AP::kAccentBr));
    g.drawText ("AuClear", 14, 0, 100, getHeight (), juce::Justification::centredLeft);

    const auto statusStr =
        juce::String::formatted ("CPU %.0f%%   lat %.1f ms", cpuLoad * 100.f, latencyMs);
    g.setFont (juce::FontOptions (11.f));
    g.setColour (juce::Colour (AP::kTxtLo));

    // Leave room for theme swatches on the right
    const int swatchRegionW = AP::kNumThemes * (kSwatchSize + kSwatchGap) + 8;
    g.drawText (statusStr, 0, 0, getWidth () - swatchRegionW - 8, getHeight (),
                juce::Justification::centredRight);

    // Theme swatches
    const int currentTheme = AP::getTheme ();
    for (int i = 0; i < AP::kNumThemes; ++i)
    {
        paintThemeSwatch (g, i, swatchBounds (i));
        if (i == currentTheme)
        {
            g.setColour (juce::Colour (AP::kAccentBr));
            g.drawRoundedRectangle (swatchBounds (i).toFloat (), 3.f, 1.5f);
        }
    }
}

void HeaderComponent::paintThemeSwatch (juce::Graphics& g, int themeIdx,
                                        juce::Rectangle<int> bounds) const
{
    const auto& t = AP::kThemes[themeIdx];
    // Two-tone swatch: left half = bg base, right half = accent bright
    const auto left  = bounds.removeFromLeft (bounds.getWidth () / 2);
    const auto right = bounds;
    g.setColour (juce::Colour (t.kBgBase));
    g.fillRoundedRectangle (left.toFloat ().withWidth (left.getWidth () + 1.f), 3.f);
    g.setColour (juce::Colour (t.kAccentBr));
    g.fillRoundedRectangle (right.toFloat (), 3.f);

    // Tooltip-like name label on hover would be ideal but label overlay is enough.
    // We add a named tooltip via mouseMove — handled in mouseEnter in future.
}

juce::Rectangle<int> HeaderComponent::swatchBounds (int themeIdx) const
{
    const int swatchRegionW = AP::kNumThemes * (kSwatchSize + kSwatchGap);
    const int startX = getWidth () - swatchRegionW - 4;
    const int cy = getHeight () / 2 - kSwatchSize / 2;
    return { startX + themeIdx * (kSwatchSize + kSwatchGap), cy, kSwatchSize, kSwatchSize };
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

void HeaderComponent::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < AP::kNumThemes; ++i)
    {
        if (swatchBounds (i).contains (e.getPosition ()))
        {
            if (onThemeChanged) onThemeChanged (i);
            repaint ();
            return;
        }
    }
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
            if (result == 0) return;

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
                    juce::ModalCallbackFunction::create ([this, aw] (int r)
                    {
                        if (r == 1)
                        {
                            const auto name = aw->getTextEditorContents ("name").trim ();
                            if (name.isNotEmpty () && onPresetChosen)
                                onPresetChosen ("__save__:" + name);
                        }
                        delete aw;
                    }), true);
                return;
            }
            else if (result == 9002)
            {
                if (onPresetChosen) onPresetChosen ("__delete__:" + currentPreset);
                return;
            }

            if (chosen.isNotEmpty () && onPresetChosen)
                onPresetChosen (chosen);
        });
}
