#include "RackModuleCard.h"

RackModuleCard::RackModuleCard (RackModule* module, Listener* lst)
    : moduleRef (module), listener (lst)
{
    addAndMakeVisible (enableButton);
    enableButton.setClickingTogglesState (true);
    enableButton.setToggleState (!module->bypassed.load(), juce::dontSendNotification);
    enableButton.onClick = [this]
    {
        const bool enabled = enableButton.getToggleState();
        moduleRef->bypassed.store (!enabled);
        repaint();
    };

    addAndMakeVisible (removeButton);
    removeButton.setButtonText ("x");
    removeButton.onClick = [this] { if (listener) listener->cardRemoved (this); };

    setRepaintsOnMouseActivity (true);
}

void RackModuleCard::paint (juce::Graphics& g)
{
    // Background
    juce::Colour bg = juce::Colour (kBg);
    if (selected) bg = juce::Colour (kBgSelect);
    else if (hovered) bg = juce::Colour (kBgHover);
    g.fillAll (bg);

    // Bottom divider
    g.setColour (juce::Colour (kDivider));
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    // Selected left accent bar
    if (selected)
    {
        g.setColour (juce::Colour (kAccent));
        g.fillRect (0, 0, 3, getHeight() - 1);
    }

    // Drag handle dots (skip the first 4px if selected bar)
    drawDragHandle (g, { selected ? 4 : 0, 0, 16, getHeight() });

    // Module name
    const bool enabled = !moduleRef->bypassed.load();
    g.setFont (juce::Font (juce::FontOptions (14.0f).withStyle (selected ? "Bold" : "Regular")));
    g.setColour (juce::Colour (enabled ? kTextHi : kTextLo));
    // Name area: after handle (16) + enable button (24) + 4 gap
    auto nameX = 16 + 28 + 4;
    // Leave room for remove button on right
    auto nameW = getWidth() - nameX - 36;
    g.drawText (moduleRef->name(), nameX, 0, nameW, getHeight() - 16,
                juce::Justification::centredLeft);

    // Sub-text: current value summary
    g.setFont (juce::FontOptions (11.0f));
    g.setColour (juce::Colour (kTextLo));
    g.drawText (moduleRef->name() == "Gain"
                    ? juce::String (moduleRef->bypassed ? "bypassed" : "active")
                    : juce::String(),
                nameX, getHeight() / 2, nameW, getHeight() / 2 - 4,
                juce::Justification::centredLeft);

    // Insert indicators
    if (insertAbove)
    {
        g.setColour (juce::Colour (kInsert));
        g.fillRect (0, 0, getWidth(), 2);
    }
    if (insertBelow)
    {
        g.setColour (juce::Colour (kInsert));
        g.fillRect (0, getHeight() - 2, getWidth(), 2);
    }
}

void RackModuleCard::drawDragHandle (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (juce::Colour (0xff4a5060));
    const float dotR = 1.5f;
    const float cx   = area.getCentreX() - dotR;
    const float top  = area.getCentreY() - 7.0f;
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 2; ++col)
            g.fillEllipse (cx + col * 5.0f, top + row * 5.0f, dotR * 2.0f, dotR * 2.0f);
}

void RackModuleCard::resized()
{
    // Enable button: left side
    enableButton.setBounds (16, (getHeight() - 22) / 2, 22, 22);
    // Remove button: right side
    removeButton.setBounds (getWidth() - 30, (getHeight() - 20) / 2, 22, 20);
}

void RackModuleCard::mouseDown (const juce::MouseEvent& e)
{
    mouseDownPos = e.getPosition();
    dragging = false;
}

void RackModuleCard::mouseDrag (const juce::MouseEvent& e)
{
    if (!dragging)
    {
        const auto delta = e.getPosition() - mouseDownPos;
        if (std::abs (delta.getY()) > kDragThreshold || std::abs (delta.getX()) > kDragThreshold)
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
    }
    else
    {
        // Simple click — check it wasn't on a button
        if (!enableButton.getBounds().contains (e.getPosition())
            && !removeButton.getBounds().contains (e.getPosition()))
        {
            if (listener) listener->cardSelected (this);
        }
    }
}

void RackModuleCard::mouseEnter (const juce::MouseEvent&) { hovered = true;  repaint(); }
void RackModuleCard::mouseExit  (const juce::MouseEvent&) { hovered = false; repaint(); }
