#include "RackModuleCard.h"

RackModuleCard::RackModuleCard (RackModule* module, Listener* lst)
    : moduleRef (module), listener (lst)
{
    addAndMakeVisible (enableButton);
    enableButton.setClickingTogglesState (true);
    enableButton.setToggleState (!module->bypassed.load (), juce::dontSendNotification);
    enableButton.setTooltip ("Enable / bypass this module");
    enableButton.onClick = [this]
    {
        moduleRef->bypassed.store (!enableButton.getToggleState ());
        repaint ();
    };

    addAndMakeVisible (removeButton);
    removeButton.setTooltip ("Remove module");
    removeButton.onClick = [this]
    {
        if (listener) listener->cardRemoved (this);
    };

    // Wet/dry mix slider
    mixSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    mixSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    mixSlider.setRange (0.0, 1.0, 0.01);
    mixSlider.setValue (static_cast<double> (module->wetDry.load ()),
                        juce::dontSendNotification);
    mixSlider.setTooltip ("Wet/dry mix (parallel processing)");
    mixSlider.onValueChange = [this]
    {
        moduleRef->wetDry.store (static_cast<float> (mixSlider.getValue ()));
    };
    addAndMakeVisible (mixSlider);

    setRepaintsOnMouseActivity (true);
}

void RackModuleCard::paint (juce::Graphics& g)
{
    juce::Colour bg = juce::Colour (AP::kBgCard);
    if (selected)      bg = juce::Colour (AP::kBgSelect);
    else if (hovered)  bg = juce::Colour (AP::kBgHover);
    g.fillAll (bg);

    // Bottom divider
    g.setColour (juce::Colour (AP::kDiv));
    g.fillRect (0, getHeight () - 1, getWidth (), 1);

    // Selected left accent bar
    if (selected)
    {
        g.setColour (juce::Colour (AP::kAccentBr));
        g.fillRect (0, 0, 3, kTopRowH);
    }

    // Drag handle
    drawDragHandle (g, {selected ? 4 : 0, 0, 16, kTopRowH});

    // Module name
    const bool enabled = !moduleRef->bypassed.load ();
    g.setFont (juce::Font (juce::FontOptions (13.f).withStyle (selected ? "Bold" : "Regular")));
    g.setColour (juce::Colour (enabled ? AP::kTxtHi : AP::kTxtLo));
    const int nameX = 16 + 28 + 4;
    const int nameW = getWidth () - nameX - 34;
    g.drawText (moduleRef->name (), nameX, 0, nameW, kTopRowH, juce::Justification::centredLeft);

    // Mix row: "Mix" label + amber bar
    const juce::Rectangle<int> mixRow (0, kTopRowH, getWidth (), kMixRowH);
    g.setColour (juce::Colour (AP::kBgDeep));
    g.fillRect (mixRow);

    const float wet = moduleRef->wetDry.load ();
    if (wet < 0.995f)
    {
        g.setColour (juce::Colour (AP::kAccentDm));
        g.setFont (juce::FontOptions (9.f));
        const juce::String label = juce::String (juce::roundToInt (wet * 100)) + "% wet";
        g.drawText (label, mixRow.reduced (40, 0), juce::Justification::centredRight);
    }

    // Insert indicators
    if (insertAbove) { g.setColour (juce::Colour (AP::kAccent)); g.fillRect (0, 0, getWidth (), 2); }
    if (insertBelow) { g.setColour (juce::Colour (AP::kAccent)); g.fillRect (0, getHeight () - 2, getWidth (), 2); }
}

void RackModuleCard::drawDragHandle (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (juce::Colour (0xff4a5060));
    const float dotR = 1.5f;
    const float cx   = area.getCentreX () - dotR;
    const float top  = area.getCentreY () - 7.f;
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 2; ++col)
            g.fillEllipse (cx + col * 5.f, top + row * 5.f, dotR * 2.f, dotR * 2.f);
}

void RackModuleCard::resized ()
{
    enableButton.setBounds (16, (kTopRowH - 22) / 2, 22, 22);
    removeButton.setBounds (getWidth () - 28, (kTopRowH - 20) / 2, 22, 20);

    // Mix slider: from nameX to just before the remove button
    const int nameX = 16 + 28 + 4;
    mixSlider.setBounds (nameX, kTopRowH, getWidth () - nameX - 4, kMixRowH);
}

void RackModuleCard::mouseDown (const juce::MouseEvent& e)
{
    // Don't start a drag if the click is in the mix slider row
    if (e.getPosition ().getY () >= kTopRowH)
        return;

    mouseDownPos = e.getPosition ();
    dragging = false;

    // Right-click → context menu
    if (e.mods.isRightButtonDown ())
    {
        showContextMenu ();
    }
}

void RackModuleCard::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getMouseDownPosition ().getY () >= kTopRowH)
        return;

    if (!dragging)
    {
        const auto delta = e.getPosition () - mouseDownPos;
        if (std::abs (delta.getY ()) > kDragThreshold || std::abs (delta.getX ()) > kDragThreshold)
        {
            dragging = true;
            if (listener) listener->cardDragStarted (this, e);
        }
    }
    else
    {
        if (listener) listener->cardDragged (this, e);
    }
}

void RackModuleCard::mouseUp (const juce::MouseEvent& e)
{
    if (dragging)
    {
        dragging = false;
        if (listener) listener->cardDragEnded (this, e);
        return;
    }

    if (e.getPosition ().getY () >= kTopRowH)
        return;

    if (e.mods.isRightButtonDown ())
        return; // handled in mouseDown

    if (!enableButton.getBounds ().contains (e.getPosition ()) &&
        !removeButton.getBounds ().contains (e.getPosition ()))
    {
        if (listener) listener->cardSelected (this);
    }
}

void RackModuleCard::mouseEnter (const juce::MouseEvent&) { hovered = true;  repaint (); }
void RackModuleCard::mouseExit  (const juce::MouseEvent&) { hovered = false; repaint (); }

void RackModuleCard::showContextMenu ()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Duplicate module");
    menu.addItem (2, "Reset to defaults");
    menu.addSeparator ();
    menu.addItem (3, "Remove");

    menu.showMenuAsync (juce::PopupMenu::Options ().withTargetComponent (this),
        [this] (int result)
        {
            if (result == 1)
            {
                if (listener) listener->cardDuplicated (this);
            }
            else if (result == 2)
            {
                // Reset: re-create a fresh module of the same type and restore its state
                // We achieve this by clearing the state tree and setting empty
                juce::ValueTree empty ("Module");
                moduleRef->setState (empty);
                moduleRef->wetDry.store (1.f);
                mixSlider.setValue (1.0, juce::dontSendNotification);
                repaint ();
            }
            else if (result == 3)
            {
                if (listener) listener->cardRemoved (this);
            }
        });
}
