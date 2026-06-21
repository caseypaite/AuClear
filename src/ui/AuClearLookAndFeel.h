#pragma once
#include <JuceHeader.h>
#include "AnalogPalette.h"

/**
 * Analog-hardware LookAndFeel.
 *
 * Visual theme: warm amber/gold on dark warm charcoal — think vintage Neve/SSL
 * console.  Knobs are drawn as physical potentiometers: brushed-metal body,
 * gradient shading, cream indicator line.  Buttons glow amber when active.
 */
class AuClearLookAndFeel : public juce::LookAndFeel_V4
{
  public:
    AuClearLookAndFeel ()
    {
        using C = juce::Colour;

        setColour (juce::ResizableWindow::backgroundColourId, C (AP::kBgBase));

        setColour (juce::Slider::thumbColourId,               C (AP::kAccentBr));
        setColour (juce::Slider::rotarySliderFillColourId,    C (AP::kAccentBr));
        setColour (juce::Slider::rotarySliderOutlineColourId, C (AP::kDiv));
        setColour (juce::Slider::trackColourId,               C (AP::kAccentBr));
        setColour (juce::Slider::backgroundColourId,          C (AP::kDiv));
        setColour (juce::Slider::textBoxTextColourId,         C (AP::kTxtHi));
        setColour (juce::Slider::textBoxBackgroundColourId,   C (AP::kBgCard));
        setColour (juce::Slider::textBoxOutlineColourId,      C (AP::kDiv));

        setColour (juce::Label::textColourId, C (AP::kTxtMid));

        setColour (juce::TextButton::buttonColourId,   C (AP::kBgCard));
        setColour (juce::TextButton::buttonOnColourId, C (AP::kAccent).withAlpha (0.25f));
        setColour (juce::TextButton::textColourOffId,  C (AP::kTxtMid));
        setColour (juce::TextButton::textColourOnId,   C (AP::kAccentBr));

        setColour (juce::ComboBox::backgroundColourId, C (AP::kBgCard));
        setColour (juce::ComboBox::textColourId,       C (AP::kTxtHi));
        setColour (juce::ComboBox::outlineColourId,    C (AP::kDiv));
        setColour (juce::ComboBox::arrowColourId,      C (AP::kAccent));

        setColour (juce::PopupMenu::backgroundColourId,
                   C (AP::kBgPanel));
        setColour (juce::PopupMenu::textColourId, C (AP::kTxtHi));
        setColour (juce::PopupMenu::highlightedBackgroundColourId,
                   C (AP::kAccent).withAlpha (0.22f));
        setColour (juce::PopupMenu::highlightedTextColourId, C (AP::kAccentBr));

        setColour (juce::TooltipWindow::backgroundColourId, C (AP::kBgCard));
        setColour (juce::TooltipWindow::textColourId,       C (AP::kTxtHi));
        setColour (juce::TooltipWindow::outlineColourId,    C (AP::kAccentDm));
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Rotary knob — analog potentiometer
    // ─────────────────────────────────────────────────────────────────────────
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override
    {
        const auto bounds =
            juce::Rectangle<float> ((float)x, (float)y, (float)width, (float)height)
                .reduced (4.f);
        const float radius = juce::jmin (bounds.getWidth (), bounds.getHeight ()) * 0.5f;
        const float cx     = bounds.getCentreX ();
        const float cy     = bounds.getCentreY ();
        const float angle  = rotaryStartAngle
                           + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // Arc track (full range, dim)
        {
            juce::Path p;
            const float arcR = radius - 3.f;
            p.addCentredArc (cx, cy, arcR, arcR, 0.f, rotaryStartAngle, rotaryEndAngle, true);
            g.setColour (juce::Colour (AP::kDiv));
            g.strokePath (p, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        }

        // Value arc (amber)
        {
            juce::Path p;
            const float arcR = radius - 3.f;
            p.addCentredArc (cx, cy, arcR, arcR, 0.f, rotaryStartAngle, angle, true);
            g.setColour (juce::Colour (AP::kAccentBr));
            g.strokePath (p, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        }

        // Knob body
        const float bodyR = radius * 0.68f;

        // Drop shadow
        g.setColour (juce::Colour (0x55000000));
        g.fillEllipse (cx - bodyR + 1.5f, cy - bodyR + 2.f, bodyR * 2.f, bodyR * 2.f);

        // Body gradient (lit from upper-left)
        {
            juce::ColourGradient grad (
                juce::Colour (AP::kKnobHi), cx - bodyR * 0.45f, cy - bodyR * 0.45f,
                juce::Colour (AP::kKnobLo), cx + bodyR * 0.45f, cy + bodyR * 0.45f,
                false);
            g.setGradientFill (grad);
            g.fillEllipse (cx - bodyR, cy - bodyR, bodyR * 2.f, bodyR * 2.f);
        }

        // Thin rim
        g.setColour (juce::Colour (AP::kKnobRim));
        g.drawEllipse (cx - bodyR, cy - bodyR, bodyR * 2.f, bodyR * 2.f, 1.2f);

        // Specular highlight (small spot, upper-left)
        {
            const float hx = cx - bodyR * 0.32f;
            const float hy = cy - bodyR * 0.35f;
            const float hr = bodyR * 0.22f;
            juce::ColourGradient spec (
                juce::Colour (0x40ffffff), hx, hy,
                juce::Colour (0x00ffffff), hx + hr, hy + hr,
                true);
            g.setGradientFill (spec);
            g.fillEllipse (hx - hr, hy - hr, hr * 2.f, hr * 2.f);
        }

        // Indicator line
        const float sinA = std::sin (angle);
        const float cosA = std::cos (angle);
        g.setColour (juce::Colour (AP::kKnobDot));
        g.drawLine (cx + sinA * bodyR * 0.18f, cy - cosA * bodyR * 0.18f,
                    cx + sinA * bodyR * 0.80f, cy - cosA * bodyR * 0.80f,
                    2.f);
        g.fillEllipse (cx + sinA * bodyR * 0.80f - 2.f,
                       cy - cosA * bodyR * 0.80f - 2.f, 4.f, 4.f);
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Button — backlit LED style
    // ─────────────────────────────────────────────────────────────────────────
    void drawButtonBackground (juce::Graphics& g, juce::Button& btn,
                               const juce::Colour& /*bg*/,
                               bool isHighlighted, bool isDown) override
    {
        auto bounds = btn.getLocalBounds ().toFloat ().reduced (0.5f);
        const bool isOn = btn.getToggleState ();

        juce::Colour fill = isOn ? juce::Colour (AP::kAccent).withAlpha (0.20f)
                                 : juce::Colour (AP::kBgCard);
        if (isDown)       fill = fill.darker (0.25f);
        else if (isHighlighted) fill = fill.brighter (0.10f);

        g.setColour (fill);
        g.fillRoundedRectangle (bounds, 4.f);

        g.setColour (isOn ? juce::Colour (AP::kAccent) : juce::Colour (AP::kDiv));
        g.drawRoundedRectangle (bounds, 4.f, isOn ? 1.2f : 0.8f);

        if (isOn)
        {
            g.setColour (juce::Colour (AP::kAccentBr));
            g.fillEllipse (bounds.getRight () - 8.f, bounds.getY () + 3.f, 5.f, 5.f);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& btn,
                         bool /*isHighlighted*/, bool /*isDown*/) override
    {
        const bool isOn = btn.getToggleState ();
        g.setFont (juce::FontOptions (12.f));
        g.setColour (isOn ? juce::Colour (AP::kAccentBr) : juce::Colour (AP::kTxtMid));
        g.drawFittedText (btn.getButtonText (),
                          btn.getLocalBounds ().reduced (4, 2),
                          juce::Justification::centred, 1);
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  PopupMenu
    // ─────────────────────────────────────────────────────────────────────────
    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (juce::Colour (AP::kBgPanel));
        g.setColour (juce::Colour (AP::kDiv));
        g.drawRect (0, 0, w, h, 1);
    }

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool /*isTicked*/, bool /*hasSubMenu*/,
                            const juce::String& text, const juce::String& /*shortcut*/,
                            const juce::Drawable* /*icon*/,
                            const juce::Colour* /*textColour*/) override
    {
        if (isSeparator)
        {
            g.setColour (juce::Colour (AP::kDiv));
            g.fillRect (area.getX () + 4, area.getCentreY (), area.getWidth () - 8, 1);
            return;
        }
        if (isHighlighted && isActive)
        {
            g.setColour (juce::Colour (AP::kAccent).withAlpha (0.22f));
            g.fillRect (area);
            g.setColour (juce::Colour (AP::kAccentBr));
            g.fillRect (area.getX (), area.getY (), 3, area.getHeight ());
        }
        g.setFont (juce::FontOptions (13.f));
        g.setColour (isActive
                         ? (isHighlighted ? juce::Colour (AP::kTxtHi) : juce::Colour (AP::kTxtMid))
                         : juce::Colour (AP::kTxtLo));
        g.drawText (text, area.reduced (10, 0), juce::Justification::centredLeft);
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  ComboBox
    // ─────────────────────────────────────────────────────────────────────────
    void drawComboBox (juce::Graphics& g, int w, int h, bool /*isDown*/,
                       int /*bX*/, int /*bY*/, int /*bW*/, int /*bH*/,
                       juce::ComboBox& /*box*/) override
    {
        const auto bounds = juce::Rectangle<float> (0.f, 0.f, (float)w, (float)h);
        g.setColour (juce::Colour (AP::kBgCard));
        g.fillRoundedRectangle (bounds, 4.f);
        g.setColour (juce::Colour (AP::kDiv));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 4.f, 1.f);

        const float as = 7.f;
        const float ax = (float)w - as - 7.f;
        const float ay = ((float)h - as * 0.55f) * 0.5f;
        juce::Path arrow;
        arrow.addTriangle (ax, ay, ax + as, ay, ax + as * 0.5f, ay + as * 0.55f);
        g.setColour (juce::Colour (AP::kAccent));
        g.fillPath (arrow);
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Tooltip
    // ─────────────────────────────────────────────────────────────────────────
    void drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h) override
    {
        const auto b = juce::Rectangle<float> (0.f, 0.f, (float)w, (float)h);
        g.setColour (juce::Colour (AP::kBgCard));
        g.fillRoundedRectangle (b, 4.f);
        g.setColour (juce::Colour (AP::kAccentDm));
        g.drawRoundedRectangle (b.reduced (0.5f), 4.f, 1.f);
        g.setColour (juce::Colour (AP::kTxtHi));
        g.setFont (juce::FontOptions (11.f));
        g.drawText (text, b.reduced (6.f, 3.f), juce::Justification::centred);
    }

    juce::Rectangle<int> getTooltipBounds (const juce::String& tip,
                                           juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override
    {
        const int w = juce::jmin (400, tip.length () * 8 + 20);
        const int h = 22;
        return juce::Rectangle<int> (screenPos.x + 12, screenPos.y - h - 4, w, h)
            .constrainedWithin (parentArea);
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Scrollbar
    // ─────────────────────────────────────────────────────────────────────────
    void drawScrollbar (juce::Graphics& g, juce::ScrollBar& /*bar*/,
                        int x, int y, int w, int h,
                        bool /*isVertical*/, int thumbStart, int thumbSize,
                        bool /*isMouseOver*/, bool /*isMouseDown*/) override
    {
        g.setColour (juce::Colour (AP::kBgBase));
        g.fillRect (x, y, w, h);
        g.setColour (juce::Colour (AP::kBgCard));
        g.fillRoundedRectangle ((float)(x + 2), (float)(y + thumbStart + 2),
                                (float)(w - 4), (float)(thumbSize - 4), 3.f);
    }

    int getScrollbarButtonSize (juce::ScrollBar&) override { return 0; }
};
