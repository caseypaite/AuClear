#include "HeaderComponent.h"

HeaderComponent::HeaderComponent ()
{
    addAndMakeVisible (bypassButton);
    bypassButton.setClickingTogglesState (true);
    bypassButton.onClick = [this]
    {
        if (onBypassToggled)
            onBypassToggled ();
    };

    addAndMakeVisible (undoButton);
    undoButton.onClick = [this]
    {
        if (onUndoClicked)
            onUndoClicked ();
    };
}

void HeaderComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (kBg));

    // Bottom divider
    g.setColour (juce::Colour (kDivider));
    g.fillRect (0, getHeight () - 1, getWidth (), 1);

    // Brand name
    g.setFont (juce::Font (juce::FontOptions (20.0f).withStyle ("Bold")));
    g.setColour (juce::Colour (kAccent));
    g.drawText ("AuClear", 16, 0, 120, getHeight (), juce::Justification::centredLeft);

    // Status: CPU + latency
    const auto statusStr =
        juce::String::formatted ("CPU %.0f%%   lat %.1f ms", cpuLoad * 100.0f, latencyMs);
    g.setFont (juce::FontOptions (12.0f));
    g.setColour (juce::Colour (kTextLo));
    g.drawText (statusStr, 0, 0, getWidth () - 8, getHeight (), juce::Justification::centredRight);
}

void HeaderComponent::resized ()
{
    auto b = getLocalBounds ().reduced (8, 6);
    b.removeFromLeft (130); // brand name
    bypassButton.setBounds (b.removeFromLeft (70));
    b.removeFromLeft (6);
    undoButton.setBounds (b.removeFromLeft (56));
}
