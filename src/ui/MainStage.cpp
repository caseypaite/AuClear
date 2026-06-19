#include "MainStage.h"
#include "../modules/GainModule.h"

MainStage::MainStage ()
{
    // Gain knob
    gainSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    gainSlider.setRange (-60.0, 12.0, 0.1);
    gainSlider.setDoubleClickReturnValue (true, 0.0);
    gainSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    gainSlider.onValueChange = [this]
    {
        if (auto* gm = dynamic_cast<GainModule*> (currentModule))
        {
            gm->gainDb.store ((float)gainSlider.getValue ());
            gainValueLabel.setText (juce::String (gainSlider.getValue (), 1) + " dB",
                                    juce::dontSendNotification);
        }
    };

    gainLabel.setText ("Gain", juce::dontSendNotification);
    gainLabel.setJustificationType (juce::Justification::centred);
    gainLabel.setColour (juce::Label::textColourId, juce::Colour (kTextLo));
    gainLabel.setFont (juce::FontOptions (12.0f));

    gainValueLabel.setJustificationType (juce::Justification::centred);
    gainValueLabel.setColour (juce::Label::textColourId, juce::Colour (kAccent));
    gainValueLabel.setFont (juce::Font (juce::FontOptions (13.0f).withStyle ("Bold")));

    addChildComponent (gainSlider);
    addChildComponent (gainLabel);
    addChildComponent (gainValueLabel);
}

void MainStage::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (kBg));

    if (currentModule == nullptr)
    {
        g.setColour (juce::Colour (kTextLo));
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("Select a module to edit it here", getLocalBounds (),
                    juce::Justification::centred);
        return;
    }

    // Module panel header
    auto header = getLocalBounds ().removeFromTop (44);
    g.setColour (juce::Colour (kPanel));
    g.fillRect (header);
    g.setColour (juce::Colour (kDivider));
    g.fillRect (0, header.getBottom () - 1, getWidth (), 1);

    g.setFont (juce::Font (juce::FontOptions (16.0f).withStyle ("Bold")));
    g.setColour (juce::Colour (kTextHi));
    g.drawText (currentModule->name (), header.reduced (16, 0), juce::Justification::centredLeft);
}

void MainStage::resized ()
{
    if (!gainSlider.isVisible ())
        return;

    const int cx = getWidth () / 2;
    const int cy = getHeight () / 2;

    gainSlider.setBounds (cx - 60, cy - 70, 120, 120);
    gainLabel.setBounds (cx - 60, cy + 54, 120, 18);
    gainValueLabel.setBounds (cx - 60, cy + 72, 120, 18);
}

void MainStage::showModule (RackModule* module)
{
    currentModule = module;

    gainSlider.setVisible (false);
    gainLabel.setVisible (false);
    gainValueLabel.setVisible (false);

    if (auto* gm = dynamic_cast<GainModule*> (module))
    {
        const float db = gm->gainDb.load ();
        gainSlider.setValue (db, juce::dontSendNotification);
        gainValueLabel.setText (juce::String (db, 1) + " dB", juce::dontSendNotification);
        gainSlider.setVisible (true);
        gainLabel.setVisible (true);
        gainValueLabel.setVisible (true);
    }

    resized ();
    repaint ();
}
