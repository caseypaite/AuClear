#pragma once
#include <JuceHeader.h>
#include "../dsp/GoniometerFifo.h"
#include <cmath>
#include <vector>

/**
 * Lissajous / stereo goniometer with persistence trail.
 *
 * Plots Mid = (L+R)/2 on the X axis and Side = (L-R)/2 on the Y axis
 * (standard Blumlein goniometer convention).  A fading trail is maintained
 * on a juce::Image that is dimmed each frame before new dots are painted.
 *
 * The stereo correlation coefficient  r = Σ(L·R) / √(Σ(L²)·Σ(R²)) is
 * displayed as a coloured bar below the Lissajous.
 *   r ≈ +1 → fully correlated (mono) → green
 *   r ≈  0 → uncorrelated              → yellow
 *   r ≈ −1 → anti-phase                → red
 */
class GoniometerDisplay : public juce::Component, private juce::Timer
{
  public:
    explicit GoniometerDisplay (GoniometerFifo& gf) : gonioFifo (gf)
    {
        startTimerHz (30);
    }
    ~GoniometerDisplay () override { stopTimer (); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds ();

        // ── Correlation bar (bottom 22 px) ──────────────────────────────────
        const int corrH = 22;
        const auto corrArea = b.removeFromBottom (corrH);

        g.setColour (juce::Colour (0xff0d0f12));
        g.fillRect (corrArea);

        // Coloured bar proportional to correlation
        const float corr  = juce::jlimit (-1.f, 1.f, displayCorr);
        const float tNorm = (corr + 1.f) * 0.5f; // 0..1
        const int bw = corrArea.getWidth ();

        // Gradient: red (left) → yellow (centre) → green (right)
        juce::ColourGradient grad (juce::Colour (0xffdd3333), static_cast<float> (corrArea.getX ()), 0.f,
                                   juce::Colour (0xff22cc44), static_cast<float> (corrArea.getRight ()), 0.f,
                                   false);
        grad.addColour (0.5, juce::Colour (0xffffcc00));
        g.setGradientFill (grad);
        g.fillRect (corrArea.reduced (0, 4));

        // Dark overlay to the right of the marker (dim the "not reached" part)
        const int markerX = corrArea.getX () + static_cast<int> (tNorm * bw);
        g.setColour (juce::Colour (0xa016181d));
        g.fillRect (juce::Rectangle<int> (markerX, corrArea.getY (), corrArea.getRight () - markerX, corrH));

        // Correlation value label
        g.setColour (juce::Colour (0xffe8eaed));
        g.setFont (9.f);
        g.drawText (juce::String (displayCorr, 2), corrArea.reduced (2),
                    juce::Justification::centred, false);

        // Marker needle
        g.setColour (juce::Colours::white);
        g.fillRect (markerX - 1, corrArea.getY () + 1, 2, corrH - 2);

        // ── Lissajous area ───────────────────────────────────────────────────
        g.setColour (juce::Colour (0xff0d0f12));
        g.fillRect (b);

        if (trailImg.isValid ())
            g.drawImage (trailImg, b.toFloat ());

        // Crosshair guides (dimly)
        const float cx = b.getCentreX (), cy = b.getCentreY ();
        const float r  = juce::jmin (b.getWidth (), b.getHeight ()) * 0.5f;
        g.setColour (juce::Colour (0xff2a2e37));
        g.drawLine (cx, static_cast<float> (b.getY ()),
                    cx, static_cast<float> (b.getBottom ()), 1.f);
        g.drawLine (static_cast<float> (b.getX ()), cy,
                    static_cast<float> (b.getRight ()), cy, 1.f);
        // ±45° lines (L and R axes in Blumlein orientation)
        g.drawLine (cx - r * 0.707f, cy + r * 0.707f,
                    cx + r * 0.707f, cy - r * 0.707f, 1.f);
        g.drawLine (cx - r * 0.707f, cy - r * 0.707f,
                    cx + r * 0.707f, cy + r * 0.707f, 1.f);

        // Axis labels
        g.setFont (8.f);
        g.setColour (juce::Colour (0xff4a5568));
        g.drawText ("L",  b.getX (),                b.getCentreY () - 5, 12, 10, juce::Justification::centred);
        g.drawText ("R",  b.getRight () - 12,        b.getCentreY () - 5, 12, 10, juce::Justification::centred);
        g.drawText ("M",  b.getCentreX () - 6,       b.getY (),           12, 10, juce::Justification::centred);
        g.drawText ("S",  b.getCentreX () - 6,       b.getBottom () - 10, 12, 10, juce::Justification::centred);
    }

    void resized () override
    {
        const auto b = getLocalBounds ().withTrimmedBottom (22);
        trailImg = juce::Image (juce::Image::RGB,
                                juce::jmax (1, b.getWidth ()),
                                juce::jmax (1, b.getHeight ()), true);
        trailImg.clear (trailImg.getBounds (), juce::Colour (0xff0d0f12));
    }

  private:
    void timerCallback () override
    {
        const int n = gonioFifo.drain (lBuf.data (), rBuf.data (),
                                       static_cast<int> (lBuf.size ()));

        if (!trailImg.isValid ())
            return;

        // Fade existing trail by drawing a semi-transparent background overlay
        {
            juce::Graphics gImg (trailImg);
            gImg.setColour (juce::Colour (0xff0d0f12).withAlpha (0.25f));
            gImg.fillAll ();
        }

        if (n > 0)
        {
            // Compute stereo correlation
            double sumLR = 0.0, sumL2 = 0.0, sumR2 = 0.0;
            for (int i = 0; i < n; ++i)
            {
                sumLR += static_cast<double> (lBuf[i]) * rBuf[i];
                sumL2 += static_cast<double> (lBuf[i]) * lBuf[i];
                sumR2 += static_cast<double> (rBuf[i]) * rBuf[i];
            }
            const double denom = std::sqrt (sumL2 * sumR2);
            displayCorr = (denom > 0.0)
                              ? juce::jlimit (-1.f, 1.f, static_cast<float> (sumLR / denom))
                              : 1.f;

            // Plot dots on trail image
            const float iw     = static_cast<float> (trailImg.getWidth ());
            const float ih     = static_cast<float> (trailImg.getHeight ());
            const float cx     = iw * 0.5f;
            const float cy     = ih * 0.5f;
            const float scale  = juce::jmin (cx, cy) * 0.88f;

            juce::Graphics gImg (trailImg);
            gImg.setColour (juce::Colour (0xff28e0c8)); // teal

            // Subsample to keep at most ~400 dots per frame
            const int step = juce::jmax (1, n / 400);
            for (int i = 0; i < n; i += step)
            {
                const float m = (lBuf[i] + rBuf[i]) * 0.5f;
                const float s = (lBuf[i] - rBuf[i]) * 0.5f;
                gImg.fillEllipse (cx + m * scale - 1.5f,
                                  cy - s * scale - 1.5f,
                                  3.f, 3.f);
            }
        }

        repaint ();
    }

    GoniometerFifo& gonioFifo;
    juce::Image     trailImg;
    float           displayCorr{1.f};

    // Drain buffers: pre-allocated on the heap to avoid large stack frames
    std::vector<float> lBuf{std::vector<float> (GoniometerFifo::kCapacity)};
    std::vector<float> rBuf{std::vector<float> (GoniometerFifo::kCapacity)};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GoniometerDisplay)
};
