#pragma once
#include <JuceHeader.h>
#include "../dsp/SpectrumFifo.h"
#include "AnalogPalette.h"
#include <array>

/**
 * Real-time spectrum analyzer display.  Drains a SpectrumFifo at 30 Hz and
 * renders log-frequency magnitude on a dark background.
 */
class SpectrumAnalyzer : public juce::Component, private juce::Timer
{
  public:
    static constexpr float kMinHz = 20.f;
    static constexpr float kMaxHz = 20000.f;
    static constexpr float kFloor = -80.f;
    static constexpr float kCeiling = 0.f;

    explicit SpectrumAnalyzer (SpectrumFifo& fifo, double sampleRate = 44100.0)
        : fifoRef (fifo), sr (sampleRate)
    {
        smoothed.fill (kFloor);
        peaks.fill (kFloor);
        startTimerHz (30);
    }

    ~SpectrumAnalyzer () override { stopTimer (); }

    void setSampleRate (double newSr) noexcept { sr = newSr; }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds ().toFloat ();
        const float w = b.getWidth (), h = b.getHeight ();

        g.setColour (juce::Colour (AP::kBgDeep));
        g.fillRoundedRectangle (b, 4.f);

        // Grid
        g.setColour (juce::Colour (AP::kBgCard));
        for (float hz : {100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f})
        {
            const float x = b.getX () + hzToX (hz, w);
            g.drawVerticalLine (juce::roundToInt (x), b.getY (), b.getBottom ());
        }
        for (float db : {-60.f, -48.f, -36.f, -24.f, -12.f})
        {
            const float y = b.getY () + dbToY (db, h);
            g.drawHorizontalLine (juce::roundToInt (y), b.getX (), b.getRight ());
        }

        if (smoothed.size () == 0)
            return;

        // Fill path
        juce::Path p;
        bool first = true;
        const int nBins = SpectrumFifo::kNumBins;

        for (int i = 1; i < nBins; ++i)
        {
            const float binHz = static_cast<float> (i) * static_cast<float> (sr) /
                                static_cast<float> (SpectrumFifo::kFFTSize);
            if (binHz < kMinHz || binHz > kMaxHz)
                continue;

            const float x = b.getX () + hzToX (binHz, w);
            const float y = b.getY () + dbToY (smoothed[static_cast<size_t> (i)], h);

            if (first)
            {
                p.startNewSubPath (x, b.getBottom ());
                p.lineTo (x, y);
                first = false;
            }
            else
                p.lineTo (x, y);
        }
        p.lineTo (b.getRight (), b.getBottom ());
        p.closeSubPath ();

        g.setColour (juce::Colour (AP::kAccentBr).withAlpha (0.20f));
        g.fillPath (p);

        // Outline
        juce::Path outline;
        first = true;
        for (int i = 1; i < nBins; ++i)
        {
            const float binHz = static_cast<float> (i) * static_cast<float> (sr) /
                                static_cast<float> (SpectrumFifo::kFFTSize);
            if (binHz < kMinHz || binHz > kMaxHz)
                continue;
            const float x = b.getX () + hzToX (binHz, w);
            const float y = b.getY () + dbToY (smoothed[static_cast<size_t> (i)], h);
            if (first)
            {
                outline.startNewSubPath (x, y);
                first = false;
            }
            else
                outline.lineTo (x, y);
        }
        g.setColour (juce::Colour (AP::kAccentBr));
        g.strokePath (outline, juce::PathStrokeType (1.f));

        // Peak hold dots
        g.setColour (juce::Colour (AP::kTxtHi).withAlpha (0.6f));
        for (int i = 1; i < nBins; ++i)
        {
            const float binHz = static_cast<float> (i) * static_cast<float> (sr) /
                                static_cast<float> (SpectrumFifo::kFFTSize);
            if (binHz < kMinHz || binHz > kMaxHz)
                continue;
            if (peaks[static_cast<size_t> (i)] > kFloor + 3.f)
            {
                const float x = b.getX () + hzToX (binHz, w);
                const float y = b.getY () + dbToY (peaks[static_cast<size_t> (i)], h);
                g.fillRect (x - 0.5f, y - 0.5f, 1.f, 1.f);
            }
        }

        // Frequency axis labels
        g.setColour (juce::Colour (AP::kTxtLo));
        g.setFont (9.f);
        for (auto [hz, label] : std::array<std::pair<float, const char*>, 5>{{{100.f, "100"},
                                                                              {500.f, "500"},
                                                                              {1000.f, "1k"},
                                                                              {5000.f, "5k"},
                                                                              {10000.f, "10k"}}})
        {
            const float x = b.getX () + hzToX (hz, w) - 10.f;
            g.drawText (label, juce::roundToInt (x), juce::roundToInt (b.getBottom () - 12), 20, 12,
                        juce::Justification::centred, false);
        }
    }

    void resized () override {}

  private:
    void timerCallback () override
    {
        SpectrumFifo::Bins newBins;
        bool gotNew = false;
        while (fifoRef.drain (newBins))
            gotNew = true;

        if (!gotNew)
            return;

        // Smooth + peak hold
        static constexpr float kDecay = 0.85f;
        static constexpr float kPeakDecay = 0.995f;
        static int holdFrames = 0;
        ++holdFrames;

        for (int i = 0; i < SpectrumFifo::kNumBins; ++i)
        {
            const auto idx = static_cast<size_t> (i);
            const float v = newBins[idx];
            smoothed[idx] = juce::jmax (v, smoothed[idx] * kDecay);
            if (v > peaks[idx])
            {
                peaks[idx] = v;
                holdFrames = 0;
            }
            else if (holdFrames > 90)
            {
                peaks[idx] = peaks[idx] * kPeakDecay + kFloor * (1.f - kPeakDecay);
            }
        }

        repaint ();
    }

    float hzToX (float hz, float w) const noexcept
    {
        return w * std::log (hz / kMinHz) / std::log (kMaxHz / kMinHz);
    }

    static float dbToY (float db, float h) noexcept
    {
        return h * (1.f - (juce::jlimit (kFloor, kCeiling, db) - kFloor) / (kCeiling - kFloor));
    }

    SpectrumFifo& fifoRef;
    double sr;
    std::array<float, SpectrumFifo::kNumBins> smoothed{};
    std::array<float, SpectrumFifo::kNumBins> peaks{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyzer)
};
