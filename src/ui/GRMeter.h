#pragma once
#include <JuceHeader.h>

/**
 * Vertical gain-reduction bar meter. Reads from an atomic<float> on a timer.
 * Range: 0 (no GR) to maxGR dB (full GR) drawn top-to-bottom.
 */
class GRMeter : public juce::Component, private juce::Timer
{
  public:
    explicit GRMeter (std::atomic<float>& grSource, float maxGRDb = 30.f)
        : source (grSource), maxGR (maxGRDb)
    {
        startTimerHz (30);
    }

    ~GRMeter () override { stopTimer (); }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds ().toFloat ();

        // Background
        g.setColour (juce::Colour (0xff0d0f12));
        g.fillRoundedRectangle (b, 3.f);

        const float gr = juce::jlimit (0.f, maxGR, -displayGR);
        const float fraction = gr / maxGR;
        const float fillH = b.getHeight () * fraction;

        // GR fill (teal → amber → red by amount)
        juce::Colour fillCol;
        if (gr < 6.f)
            fillCol = juce::Colour (0xff28e0c8);
        else if (gr < 15.f)
            fillCol = juce::Colour (0xffddaa00);
        else
            fillCol = juce::Colour (0xffdd3333);

        g.setColour (fillCol);
        g.fillRoundedRectangle (b.withTop (b.getY () + b.getHeight () - fillH), 3.f);

        // Scale lines every 6 dB
        g.setColour (juce::Colour (0xff2a2e37));
        for (float db = 6.f; db < maxGR; db += 6.f)
        {
            const float y = b.getY () + b.getHeight () * (1.f - db / maxGR);
            g.drawHorizontalLine (juce::roundToInt (y), b.getX (), b.getRight ());
        }

        // Label
        g.setColour (juce::Colour (0xff9aa0ab));
        g.setFont (9.f);
        g.drawText (juce::String (static_cast<int> (-gr)) + " dB", b.reduced (2).withHeight (12),
                    juce::Justification::centredTop, false);
    }

    void resized () override {}

  private:
    void timerCallback () override
    {
        const float gr = source.load (std::memory_order_relaxed);
        if (std::abs (gr - displayGR) > 0.05f)
        {
            displayGR = gr;
            repaint ();
        }
    }

    std::atomic<float>& source;
    float maxGR;
    float displayGR{0.f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GRMeter)
};
