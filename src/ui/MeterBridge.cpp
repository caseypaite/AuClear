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

    // LUFS / True-peak
    auto& lm = rack.loudnessMeter ();
    lufsM = lm.momentaryLUFS.load (std::memory_order_relaxed);
    lufsS = lm.shortTermLUFS.load (std::memory_order_relaxed);
    lufsI = lm.integratedLUFS.load (std::memory_order_relaxed);
    const float tp = lm.truePeak.load (std::memory_order_relaxed);
    tpDb = (tp > 0.f) ? juce::Decibels::gainToDecibels (tp) : -100.f;

    if (repaintNeeded)
        repaint ();
}

void MeterBridge::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (AP::kBgPanel));

    // Top divider
    g.setColour (juce::Colour (AP::kDiv));
    g.fillRect (0, 0, getWidth (), 1);

    auto b = getLocalBounds ().reduced (8, 6).toFloat ();

    // ── I/O Meters ──────────────────────────────────────────────────────────
    g.setFont (juce::FontOptions (11.f));
    g.setColour (juce::Colour (AP::kTxtLo));
    g.drawText ("IN", b.removeFromLeft (20), juce::Justification::centred);
    b.removeFromLeft (4);

    const float mW = 8.f, mGap = 3.f, grpGap = 20.f;

    drawMeterBar (g, b.removeFromLeft (mW), displayIn.peakL, inPeakHoldL);
    b.removeFromLeft (mGap);
    drawMeterBar (g, b.removeFromLeft (mW), displayIn.peakR, inPeakHoldR);
    b.removeFromLeft (grpGap);

    g.setFont (juce::FontOptions (11.f));
    g.setColour (juce::Colour (AP::kTxtLo));
    g.drawText ("OUT", b.removeFromLeft (26), juce::Justification::centred);
    b.removeFromLeft (4);

    drawMeterBar (g, b.removeFromLeft (mW), displayOut.peakL, outPeakHoldL);
    b.removeFromLeft (mGap);
    drawMeterBar (g, b.removeFromLeft (mW), displayOut.peakR, outPeakHoldR);
    b.removeFromLeft (grpGap);

    // ── LUFS / True-peak ────────────────────────────────────────────────────
    auto lufsArea = b.removeFromRight (b.getWidth ()); // consume remaining width

    auto drawLufsCell = [&] (const juce::String& lbl, float val, juce::Colour col)
    {
        auto cell = lufsArea.removeFromLeft (70.f);
        g.setColour (juce::Colour (AP::kTxtLo));
        g.setFont (juce::FontOptions (9.f));
        g.drawText (lbl, cell.removeFromTop (12), juce::Justification::centredLeft);
        g.setColour (col);
        g.setFont (juce::Font (juce::FontOptions (12.f).withStyle ("Bold")));
        const juce::String txt = (val > -69.f) ? juce::String (val, 1) + " LU" : "  ---";
        g.drawText (txt, cell, juce::Justification::centredLeft);
    };

    drawLufsCell ("M", lufsM, juce::Colour (AP::kAccentBr));
    drawLufsCell ("S", lufsS, juce::Colour (AP::kAccentBr));
    drawLufsCell ("I", lufsI, juce::Colour (AP::kTxtHi));

    // True-peak
    {
        auto cell = lufsArea.removeFromLeft (70.f);
        g.setColour (juce::Colour (AP::kTxtLo));
        g.setFont (juce::FontOptions (9.f));
        g.drawText ("TP", cell.removeFromTop (12), juce::Justification::centredLeft);
        g.setColour (tpDb > -1.f ? juce::Colour (AP::kRed) : juce::Colour (AP::kAccentBr));
        g.setFont (juce::Font (juce::FontOptions (12.f).withStyle ("Bold")));
        const juce::String tpTxt = (tpDb > -100.f) ? juce::String (tpDb, 1) + " dB" : "  ---";
        g.drawText (tpTxt, cell, juce::Justification::centredLeft);
    }
}

void MeterBridge::drawMeterBar (juce::Graphics& g, juce::Rectangle<float> bounds, float peakDb,
                                float holdDb) const
{
    g.setColour (juce::Colour (AP::kBgDeep));
    g.fillRoundedRectangle (bounds, 2.f);

    const float range = kCeil - kFloor;
    auto levelFrac = [&] (float db) { return juce::jlimit (0.f, 1.f, (db - kFloor) / range); };

    const float frac = levelFrac (peakDb);
    const float holdF = levelFrac (holdDb);

    if (frac > 0.f)
    {
        const float barH = bounds.getHeight () * frac;
        const float barY = bounds.getBottom () - barH;
        auto bar = juce::Rectangle<float> (bounds.getX (), barY, bounds.getWidth (), barH);

        const float greenFrac = levelFrac (-6.f);
        const float amberFrac = levelFrac (0.f);

        if (frac <= greenFrac)
        {
            g.setColour (juce::Colour (AP::kGreen));
            g.fillRoundedRectangle (bar, 2.f);
        }
        else if (frac <= amberFrac)
        {
            const float gH = bounds.getHeight () * greenFrac;
            g.setColour (juce::Colour (AP::kGreen));
            g.fillRoundedRectangle ({bar.getX (), bounds.getBottom () - gH, bar.getWidth (), gH},
                                    2.f);
            g.setColour (juce::Colour (AP::kAmber));
            g.fillRoundedRectangle ({bar.getX (), barY, bar.getWidth (), barH - gH}, 2.f);
        }
        else
        {
            const float gH = bounds.getHeight () * greenFrac;
            const float aH = bounds.getHeight () * amberFrac;
            g.setColour (juce::Colour (AP::kGreen));
            g.fillRoundedRectangle ({bar.getX (), bounds.getBottom () - gH, bar.getWidth (), gH},
                                    2.f);
            g.setColour (juce::Colour (AP::kAmber));
            g.fillRoundedRectangle (
                {bar.getX (), bounds.getBottom () - aH, bar.getWidth (), aH - gH}, 2.f);
            g.setColour (juce::Colour (AP::kRed));
            g.fillRoundedRectangle ({bar.getX (), barY, bar.getWidth (), barH - aH}, 2.f);
        }
    }

    if (holdDb > kFloor)
    {
        const float holdY = bounds.getBottom () - bounds.getHeight () * holdF;
        g.setColour (juce::Colour (AP::kTxtHi));
        g.fillRect (bounds.getX (), holdY - 1.f, bounds.getWidth (), 2.f);
    }
}

void MeterBridge::resized () {}
