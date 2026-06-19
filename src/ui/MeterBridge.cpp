#include "MeterBridge.h"

MeterBridge::MeterBridge (ProcessorRack& r) : rack (r)
{
    startTimerHz (kHz);
}

MeterBridge::~MeterBridge ()
{
    stopTimer ();
}

void MeterBridge::timerCallback ()
{
    bool repaintNeeded = false;

    auto drainAndDecay = [&] (MeterBus& bus, MeterValues& display)
    {
        MeterValues fresh{display.peakL * 0.85f, display.peakR * 0.85f, display.rmsL * 0.85f,
                          display.rmsR * 0.85f};
        if (bus.drain (fresh))
            repaintNeeded = true;
        display = fresh;
    };

    drainAndDecay (rack.inputMeters (), displayIn);
    drainAndDecay (rack.outputMeters (), displayOut);

    // Peak hold
    if (++peakHoldTick >= kHoldFrames)
    {
        peakHoldTick = 0;
        inPeakHoldL = displayIn.peakL;
        inPeakHoldR = displayIn.peakR;
        outPeakHoldL = displayOut.peakL;
        outPeakHoldR = displayOut.peakR;
    }

    inPeakHoldL = std::max (inPeakHoldL, displayIn.peakL);
    inPeakHoldR = std::max (inPeakHoldR, displayIn.peakR);
    outPeakHoldL = std::max (outPeakHoldL, displayOut.peakL);
    outPeakHoldR = std::max (outPeakHoldR, displayOut.peakR);

    if (repaintNeeded)
        repaint ();
}

void MeterBridge::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (kPanel));

    // Top divider
    g.setColour (juce::Colour (kDivider));
    g.fillRect (0, 0, getWidth (), 1);

    auto b = getLocalBounds ().reduced (8, 6).toFloat ();

    // Labels
    g.setFont (juce::FontOptions (11.0f));
    g.setColour (juce::Colour (kTextLo));
    g.drawText ("IN", b.removeFromLeft (20), juce::Justification::centred);
    b.removeFromLeft (4);

    const float meterW = 8.0f;
    const float meterGap = 3.0f;
    const float groupGap = 24.0f;

    // Input L
    drawMeterBar (g, b.removeFromLeft (meterW), displayIn.peakL, inPeakHoldL);
    b.removeFromLeft (meterGap);
    // Input R
    drawMeterBar (g, b.removeFromLeft (meterW), displayIn.peakR, inPeakHoldR);
    b.removeFromLeft (groupGap);

    g.setFont (juce::FontOptions (11.0f));
    g.setColour (juce::Colour (kTextLo));
    g.drawText ("OUT", b.removeFromLeft (26), juce::Justification::centred);
    b.removeFromLeft (4);

    // Output L
    drawMeterBar (g, b.removeFromLeft (meterW), displayOut.peakL, outPeakHoldL);
    b.removeFromLeft (meterGap);
    // Output R
    drawMeterBar (g, b.removeFromLeft (meterW), displayOut.peakR, outPeakHoldR);
}

void MeterBridge::drawMeterBar (juce::Graphics& g, juce::Rectangle<float> bounds, float peakDb,
                                float holdDb) const
{
    // Background
    g.setColour (juce::Colour (0xff0d0f12));
    g.fillRoundedRectangle (bounds, 2.0f);

    const float range = kCeil - kFloor;
    auto levelFraction = [&] (float db)
    { return juce::jlimit (0.0f, 1.0f, (db - kFloor) / range); };

    const float frac = levelFraction (peakDb);
    const float holdF = levelFraction (holdDb);

    if (frac > 0.0f)
    {
        const float barH = bounds.getHeight () * frac;
        const float barY = bounds.getBottom () - barH;
        auto bar = juce::Rectangle<float> (bounds.getX (), barY, bounds.getWidth (), barH);

        // Green section (up to -6 dBFS ~ 90% of range starting at kFloor)
        const float greenFrac = juce::jlimit (0.0f, 1.0f, (-6.0f - kFloor) / range);
        const float amberFrac = juce::jlimit (0.0f, 1.0f, (0.0f - kFloor) / range);

        if (frac <= greenFrac)
        {
            g.setColour (juce::Colour (kGreen));
            g.fillRoundedRectangle (bar, 2.0f);
        }
        else if (frac <= amberFrac)
        {
            const float greenH = bounds.getHeight () * greenFrac;
            g.setColour (juce::Colour (kGreen));
            g.fillRoundedRectangle (
                {bar.getX (), bounds.getBottom () - greenH, bar.getWidth (), greenH}, 2.0f);
            g.setColour (juce::Colour (kAmber));
            g.fillRoundedRectangle ({bar.getX (), barY, bar.getWidth (), barH - greenH}, 2.0f);
        }
        else
        {
            const float greenH = bounds.getHeight () * greenFrac;
            const float amberH = bounds.getHeight () * amberFrac;
            g.setColour (juce::Colour (kGreen));
            g.fillRoundedRectangle (
                {bar.getX (), bounds.getBottom () - greenH, bar.getWidth (), greenH}, 2.0f);
            g.setColour (juce::Colour (kAmber));
            g.fillRoundedRectangle (
                {bar.getX (), bounds.getBottom () - amberH, bar.getWidth (), amberH - greenH},
                2.0f);
            g.setColour (juce::Colour (kRed));
            g.fillRoundedRectangle ({bar.getX (), barY, bar.getWidth (), barH - amberH}, 2.0f);
        }
    }

    // Peak hold line
    if (holdDb > kFloor)
    {
        const float holdY = bounds.getBottom () - bounds.getHeight () * holdF;
        g.setColour (juce::Colour (kHold));
        g.fillRect (bounds.getX (), holdY - 1.0f, bounds.getWidth (), 2.0f);
    }
}

void MeterBridge::resized () {}
