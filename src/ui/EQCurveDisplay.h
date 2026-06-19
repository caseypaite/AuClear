#pragma once
#include <JuceHeader.h>
#include "../modules/ParametricEQModule.h"
#include <array>

/**
 * Draws the combined frequency response of a ParametricEQModule.
 * Recomputed on the GUI thread at 30 Hz from the module's atomic band params.
 */
class EQCurveDisplay : public juce::Component, private juce::Timer
{
  public:
    static constexpr int kPoints = 300;
    static constexpr float kMinHz = 20.f;
    static constexpr float kMaxHz = 20000.f;
    static constexpr float kDbRange = 30.f; // ±30 dB display

    explicit EQCurveDisplay (ParametricEQModule& eq) : eqRef (eq) { startTimerHz (30); }

    ~EQCurveDisplay () override { stopTimer (); }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds ().toFloat ();
        const float w = b.getWidth (), h = b.getHeight ();

        // Background
        g.setColour (juce::Colour (0xff12141a));
        g.fillRoundedRectangle (b, 4.f);

        // Grid lines
        g.setColour (juce::Colour (0xff1e2128));
        // Frequency grid: 100, 1k, 10k
        for (float hz : {100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f})
        {
            const float x = hzToX (hz, w);
            g.drawVerticalLine (juce::roundToInt (x + b.getX ()), b.getY (), b.getBottom ());
        }
        // dB grid: 0, ±6, ±12, ±18
        for (float db : {-18.f, -12.f, -6.f, 0.f, 6.f, 12.f, 18.f})
        {
            const float y = dbToY (db, h) + b.getY ();
            g.drawHorizontalLine (juce::roundToInt (y), b.getX (), b.getRight ());
        }

        // 0 dB line brighter
        g.setColour (juce::Colour (0xff2a2e37));
        const float zeroY = dbToY (0.f, h) + b.getY ();
        g.drawHorizontalLine (juce::roundToInt (zeroY), b.getX (), b.getRight ());

        // EQ curve
        if (curve.size () < 2)
            return;

        juce::Path p;
        bool first = true;
        for (int i = 0; i < kPoints; ++i)
        {
            const float x = b.getX () + (float)i / (kPoints - 1) * w;
            const float y = dbToY (curve[static_cast<size_t> (i)], h) + b.getY ();
            if (first)
            {
                p.startNewSubPath (x, y);
                first = false;
            }
            else
                p.lineTo (x, y);
        }

        // Fill under curve
        juce::Path fill = p;
        fill.lineTo (b.getRight (), zeroY);
        fill.lineTo (b.getX (), zeroY);
        fill.closeSubPath ();
        g.setColour (juce::Colour (0xff28e0c8).withAlpha (0.12f));
        g.fillPath (fill);

        // Curve line
        g.setColour (juce::Colour (0xff28e0c8));
        g.strokePath (p, juce::PathStrokeType (1.5f));

        // Frequency labels
        g.setColour (juce::Colour (0xff9aa0ab));
        g.setFont (9.f);
        for (auto [hz, label] : std::array<std::pair<float, const char*>, 5>{{{100.f, "100"},
                                                                              {500.f, "500"},
                                                                              {1000.f, "1k"},
                                                                              {5000.f, "5k"},
                                                                              {10000.f, "10k"}}})
        {
            const float x = hzToX (hz, w) + b.getX () - 10.f;
            g.drawText (label, juce::roundToInt (x), juce::roundToInt (b.getBottom () - 12), 20, 12,
                        juce::Justification::centred, false);
        }
    }

    void resized () override { recompute (); }

  private:
    void timerCallback () override
    {
        recompute ();
        repaint ();
    }

    void recompute ()
    {
        const float sr = 44100.f; // approximate — exact value not critical for display
        curve.resize (kPoints);

        for (int i = 0; i < kPoints; ++i)
        {
            const float hz = xToHz (static_cast<float> (i) / (kPoints - 1));
            float totalDb = 0.f;

            for (int b = 0; b < ParametricEQModule::kNumBands; ++b)
            {
                const auto idx = static_cast<size_t> (b);
                const auto bt =
                    static_cast<ParametricEQModule::BandType> (eqRef.bands[idx].type.load ());
                if (bt == ParametricEQModule::BandType::Off)
                    continue;

                const float freq = eqRef.bands[idx].freq.load ();
                const float gain = eqRef.bands[idx].gain.load ();
                const float q = juce::jmax (0.1f, eqRef.bands[idx].q.load ());

                BiquadFilter::Coeffs c;
                switch (bt)
                {
                case ParametricEQModule::BandType::Off:
                    break;
                case ParametricEQModule::BandType::Bell:
                    c = BiquadFilter::makeBell (sr, freq, q, gain);
                    break;
                case ParametricEQModule::BandType::LowShelf:
                    c = BiquadFilter::makeLowShelf (sr, freq, gain);
                    break;
                case ParametricEQModule::BandType::HighShelf:
                    c = BiquadFilter::makeHighShelf (sr, freq, gain);
                    break;
                case ParametricEQModule::BandType::LowPass:
                    c = BiquadFilter::makeLowPass (sr, freq, q);
                    break;
                case ParametricEQModule::BandType::HighPass:
                    c = BiquadFilter::makeHighPass (sr, freq, q);
                    break;
                case ParametricEQModule::BandType::Notch:
                    c = BiquadFilter::makeNotch (sr, freq, q);
                    break;
                }

                BiquadFilter tmpFilter;
                tmpFilter.setCoeffs (c);
                const float mag = tmpFilter.magnitudeAt (hz, sr);
                totalDb += (mag > 0.f ? juce::Decibels::gainToDecibels (mag) : -60.f);
            }

            curve[static_cast<size_t> (i)] = juce::jlimit (-kDbRange, kDbRange, totalDb);
        }
    }

    static float hzToX (float hz, float width) noexcept
    {
        return width * std::log (hz / kMinHz) / std::log (kMaxHz / kMinHz);
    }

    static float xToHz (float norm) noexcept { return kMinHz * std::pow (kMaxHz / kMinHz, norm); }

    static float dbToY (float db, float height) noexcept
    {
        return height * (0.5f - db / (2.f * kDbRange));
    }

    ParametricEQModule& eqRef;
    std::vector<float> curve;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQCurveDisplay)
};
