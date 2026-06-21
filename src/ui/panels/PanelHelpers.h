#pragma once
#include <JuceHeader.h>
#include <atomic>
#include "../AnalogPalette.h"

/**
 * Reusable building blocks for module control panels.
 */

// A rotary knob with label + live-value readout below.
struct KnobGroup
{
    juce::Slider slider;
    juce::Label label;
    juce::Label value;

    KnobGroup ()
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        label.setFont (juce::FontOptions (11.f));
        label.setColour (juce::Label::textColourId, juce::Colour (AP::kTxtLo));
        label.setJustificationType (juce::Justification::centred);
        value.setFont (juce::FontOptions (11.f));
        value.setColour (juce::Label::textColourId, juce::Colour (AP::kAccentBr));
        value.setJustificationType (juce::Justification::centred);
    }

    void addTo (juce::Component& parent)
    {
        parent.addAndMakeVisible (slider);
        parent.addAndMakeVisible (label);
        parent.addAndMakeVisible (value);
    }

    // bounds: square area; label and value are placed below the rotary
    void layout (juce::Rectangle<int> bounds, const juce::String& labelText)
    {
        auto b = bounds;
        label.setBounds (b.removeFromBottom (14));
        value.setBounds (b.removeFromBottom (14));
        slider.setBounds (b);
        label.setText (labelText, juce::dontSendNotification);
    }

    void updateValue (float v, const juce::String& suffix = "")
    {
        value.setText (juce::String (v, 2) + suffix, juce::dontSendNotification);
    }
};

// Connect a KnobGroup to an atomic<float> — bidirectional via lambda
inline void bindKnob (KnobGroup& kg, std::atomic<float>& param, double rangeMin, double rangeMax,
                      double step, const juce::String& suffix = "")
{
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
