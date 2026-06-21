#pragma once
#include <JuceHeader.h>
#include <atomic>
#include "../AnalogPalette.h"

/**
 * Reusable building blocks for module control panels.
 */

/**
 * Theme-aware label that reads AP:: at paint time.
 * isValueLabel=true → accent bright colour; false → dim text colour.
 * This means the label text always matches the active theme without
 * needing a repaint-triggered setColour() call.
 */
class KnobLabel : public juce::Label
{
  public:
    bool isValueLabel{false};
    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (isValueLabel ? AP::kAccentBr : AP::kTxtLo));
        g.setFont (getFont ());
        g.drawText (getText (), getLocalBounds (), getJustificationType (), true);
    }
};

// A rotary knob with label + live-value readout below.
struct KnobGroup
{
    juce::Slider slider;
    KnobLabel    label;
    KnobLabel    value;

    // Set by bindKnob; used to auto-generate tooltip in layout()
    juce::String tooltipSuffix;
    juce::String tooltipOverride;

    KnobGroup ()
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        label.setFont (juce::FontOptions (11.f));
        label.setJustificationType (juce::Justification::centred);
        value.setFont (juce::FontOptions (11.f));
        value.setJustificationType (juce::Justification::centred);
        value.isValueLabel = true;
    }

    void addTo (juce::Component& parent)
    {
        parent.addAndMakeVisible (slider);
        parent.addAndMakeVisible (label);
        parent.addAndMakeVisible (value);
    }

    // bounds: square area; label and value are placed below the rotary.
    // Also generates the tooltip from label text + range set by bindKnob.
    void layout (juce::Rectangle<int> bounds, const juce::String& labelText)
    {
        auto b = bounds;
        label.setBounds (b.removeFromBottom (14));
        value.setBounds (b.removeFromBottom (14));
        slider.setBounds (b);
        label.setText (labelText, juce::dontSendNotification);

        if (tooltipOverride.isNotEmpty ())
        {
            slider.setTooltip (tooltipOverride);
        }
        else if (slider.getTooltip ().isEmpty ())
        {
            const auto range = slider.getRange ();
            slider.setTooltip (labelText + ": "
                + juce::String (range.getStart (), 1) + " \xe2\x80\x93 "
                + juce::String (range.getEnd (),   1) + tooltipSuffix);
        }
    }

    void updateValue (float v, const juce::String& suffix = "")
    {
        value.setText (juce::String (v, 2) + suffix, juce::dontSendNotification);
    }
};

// Connect a KnobGroup to an atomic<float> — bidirectional via lambda.
// Tooltip is auto-generated in layout() from label text + range; override via overrideTooltip.
inline void bindKnob (KnobGroup& kg, std::atomic<float>& param, double rangeMin, double rangeMax,
                      double step, const juce::String& suffix = "",
                      const juce::String& overrideTooltip = {})
{
    kg.tooltipSuffix   = suffix;
    kg.tooltipOverride = overrideTooltip;

    kg.slider.setRange (rangeMin, rangeMax, step);
    kg.slider.setValue (static_cast<double> (param.load ()), juce::dontSendNotification);
    kg.updateValue (static_cast<float> (kg.slider.getValue ()), suffix);

    kg.slider.onValueChange = [&param, &kg, suffix]
    {
        param.store (static_cast<float> (kg.slider.getValue ()));
        kg.updateValue (static_cast<float> (kg.slider.getValue ()), suffix);
    };
}

inline juce::Colour panelBg ()  { return juce::Colour (AP::kBgBase);   }
inline juce::Colour accent ()   { return juce::Colour (AP::kAccentBr); }
inline juce::Colour textHi ()   { return juce::Colour (AP::kTxtHi);    }
inline juce::Colour textLo ()   { return juce::Colour (AP::kTxtLo);    }
inline juce::Colour panelCol () { return juce::Colour (AP::kBgPanel);  }
inline juce::Colour divider ()  { return juce::Colour (AP::kDiv);      }
