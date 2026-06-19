#pragma once
#include <JuceHeader.h>

class AuClearLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AuClearLookAndFeel()
    {
        using C = juce::Colour;
        setColour (juce::ResizableWindow::backgroundColourId,   C (0xff16181d));
        setColour (juce::Slider::thumbColourId,                 C (0xff28e0c8));
        setColour (juce::Slider::rotarySliderFillColourId,      C (0xff28e0c8));
        setColour (juce::Slider::rotarySliderOutlineColourId,   C (0xff2a2e37));
        setColour (juce::Slider::trackColourId,                 C (0xff28e0c8));
        setColour (juce::Slider::backgroundColourId,            C (0xff2a2e37));
        setColour (juce::Label::textColourId,                   C (0xff9aa0ab));
        setColour (juce::TextButton::buttonColourId,            C (0xff1e2128));
        setColour (juce::TextButton::buttonOnColourId,          C (0xff28e0c8).withAlpha (0.2f));
        setColour (juce::TextButton::textColourOffId,           C (0xff9aa0ab));
        setColour (juce::TextButton::textColourOnId,            C (0xff28e0c8));
        setColour (juce::ComboBox::backgroundColourId,          C (0xff1e2128));
        setColour (juce::ComboBox::textColourId,                C (0xffe8eaed));
        setColour (juce::ComboBox::outlineColourId,             C (0xff2a2e37));
        setColour (juce::PopupMenu::backgroundColourId,         C (0xff1e2128));
        setColour (juce::PopupMenu::textColourId,               C (0xffe8eaed));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, C (0xff28e0c8).withAlpha (0.2f));
        setColour (juce::PopupMenu::highlightedTextColourId,    C (0xff28e0c8));
    }

    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override
    {
        const auto bounds = juce::Rectangle<float> ((float)x, (float)y,
                                                    (float)width, (float)height).reduced (6.0f);
        const float radius  = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const float cx      = bounds.getCentreX();
        const float cy      = bounds.getCentreY();
        const float trackR  = radius - 4.0f;
        const float angle   = rotaryStartAngle + sliderPosProportional
                              * (rotaryEndAngle - rotaryStartAngle);

        // Track
        {
            juce::Path p;
            p.addCentredArc (cx, cy, trackR, trackR, 0.0f,
                             rotaryStartAngle, rotaryEndAngle, true);
            g.setColour (juce::Colour (0xff2a2e37));
            g.strokePath (p, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        }

        // Value arc
        {
            juce::Path p;
            p.addCentredArc (cx, cy, trackR, trackR, 0.0f,
                             rotaryStartAngle, angle, true);
            g.setColour (juce::Colour (0xff28e0c8));
            g.strokePath (p, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        }

        // Thumb
        const float tx = cx + trackR * std::sin (angle);
        const float ty = cy - trackR * std::cos (angle);
        g.setColour (juce::Colour (0xff28e0c8));
        g.fillEllipse (tx - 4.5f, ty - 4.5f, 9.0f, 9.0f);

        // Center fill
        g.setColour (juce::Colour (0xff16181d));
        const float innerR = radius - 14.0f;
        if (innerR > 2.0f)
            g.fillEllipse (cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        auto baseColour = backgroundColour
            .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f);

        if (shouldDrawButtonAsDown)   baseColour = baseColour.darker (0.3f);
        if (shouldDrawButtonAsHighlighted) baseColour = baseColour.brighter (0.1f);

        g.setColour (baseColour);
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (juce::Colour (0xff2a2e37));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
    }
};
