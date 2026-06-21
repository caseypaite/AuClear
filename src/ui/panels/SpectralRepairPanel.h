#pragma once
#include <JuceHeader.h>
#include "PanelHelpers.h"
#include "../../modules/SpectralRepairModule.h"
#include <cmath>

// Spectrogram display: rolling horizontal image updated at 30 Hz.
// Each column = one FFT frame, each row = one frequency bin (log scale).
// Magnitude mapped to colour via a 5-stop gradient:
//   -80 dB → black,  -50 dB → dark blue,  -25 dB → teal,
//   -10 dB → yellow, 0 dB → white.
class SpectrogramDisplay : public juce::Component, private juce::Timer
{
  public:
    explicit SpectrogramDisplay (SpectralRepairModule& m,
                                 std::atomic<float>&  freqLoRef,
                                 std::atomic<float>&  freqHiRef)
        : module (m), freqLo (freqLoRef), freqHi (freqHiRef)
    {
        startTimerHz (30);
    }
    ~SpectrogramDisplay () override { stopTimer (); }

    void setSampleRate (double sr) { sampleRate = sr; }

    void paint (juce::Graphics& g) override
    {
        if (spectroImg.isValid ())
            g.drawImage (spectroImg, getLocalBounds ().toFloat ());
        else
            g.fillAll (juce::Colour (0xff0d0f12));

        // Frequency-range highlight overlay
        if (sampleRate > 0.0)
        {
            const float nyq   = static_cast<float> (sampleRate) * 0.5f;
            const float flo   = juce::jlimit (20.f, nyq, freqLo.load ());
            const float fhi   = juce::jlimit (20.f, nyq, freqHi.load ());
            const float lo20  = std::log2 (20.f / nyq);
            const float hi20  = std::log2 (nyq  / nyq); // = 0
            const float loPos = 1.f - std::log2 (flo / nyq) / lo20;
            const float hiPos = 1.f - std::log2 (fhi / nyq) / lo20;

            const float h   = static_cast<float> (getHeight ());
            const float y1  = (1.f - hiPos) * h;
            const float y2  = (1.f - loPos) * h;
            g.setColour (juce::Colour (0x3028e0c8));
            g.fillRect (juce::Rectangle<float> (0.f, y1, static_cast<float> (getWidth ()), y2 - y1));
            // Border lines
            g.setColour (juce::Colour (0x9028e0c8));
            g.drawHorizontalLine (juce::roundToInt (y1), 0.f, static_cast<float> (getWidth ()));
            g.drawHorizontalLine (juce::roundToInt (y2), 0.f, static_cast<float> (getWidth ()));
            (void) hi20;
        }
    }

    void resized () override
    {
        spectroImg = juce::Image (juce::Image::RGB, juce::jmax (1, getWidth ()),
                                  juce::jmax (1, getHeight ()), true);
        spectroImg.clear (spectroImg.getBounds (), juce::Colour (0xff0d0f12));
        writeCol = 0;
    }

  private:
    void timerCallback () override
    {
        SpectralRepairModule::MagFrame frame;
        bool got = false;
        while (module.drainSpectro (frame))
            got = true; // keep latest frame

        if (!got || !spectroImg.isValid ())
            return;

        const int w = spectroImg.getWidth ();
        const int h = spectroImg.getHeight ();

        // Scroll image left by 1 pixel
        juce::Image::BitmapData bmp (spectroImg, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < h; ++y)
        {
            auto* row = reinterpret_cast<uint32_t*> (bmp.getLinePointer (y));
            std::copy (row + 1, row + w, row);
        }

        // Paint new column at the right edge (log-frequency mapping)
        const int col = w - 1;
        for (int py = 0; py < h; ++py)
        {
            // Map pixel row to frequency (log scale, 20 Hz → Nyquist)
            const float nyq    = sampleRate > 0.0 ? static_cast<float> (sampleRate) * 0.5f : 22050.f;
            const float tNorm  = 1.f - static_cast<float> (py) / static_cast<float> (h);
            const float freq   = 20.f * std::pow (nyq / 20.f, tNorm);
            const int   binIdx = juce::jlimit (0, SpectralRepairModule::kNumBins - 1,
                                               static_cast<int> (freq * SpectralRepairModule::kFFTSize / (nyq * 2.f)));
            const float mag    = frame[static_cast<size_t> (binIdx)];
            const float db     = mag > 0.f ? juce::Decibels::gainToDecibels (mag) : -80.f;

            spectroImg.setPixelAt (col, py, dbToColour (db));
        }

        repaint ();
    }

    static juce::Colour dbToColour (float db) noexcept
    {
        // 5-stop gradient  −80→black, −50→dark blue, −25→teal, −10→yellow, 0→white
        struct Stop { float db; uint8_t r, g, b; };
        constexpr Stop stops[] = {
            {-80.f,   0,   0,   0},
            {-50.f,   0,  20, 100},
            {-25.f,   0, 180, 180},
            {-10.f, 220, 200,   0},
            {  0.f, 255, 255, 255},
        };
        constexpr int n = 5;
        if (db <= stops[0].db)   return juce::Colour (0xff000000);
        if (db >= stops[n-1].db) return juce::Colour (0xffffffff);
        for (int i = 0; i < n - 1; ++i)
        {
            if (db < stops[i+1].db)
            {
                const float t = (db - stops[i].db) / (stops[i+1].db - stops[i].db);
                return juce::Colour::fromRGB (
                    static_cast<uint8_t> (stops[i].r + t * (stops[i+1].r - stops[i].r)),
                    static_cast<uint8_t> (stops[i].g + t * (stops[i+1].g - stops[i].g)),
                    static_cast<uint8_t> (stops[i].b + t * (stops[i+1].b - stops[i].b)));
            }
        }
        return juce::Colour (0xff000000);
    }

    SpectralRepairModule&  module;
    std::atomic<float>&    freqLo;
    std::atomic<float>&    freqHi;
    double                 sampleRate{44100.0};
    juce::Image            spectroImg;
    int                    writeCol{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrogramDisplay)
};

// ─────────────────────────────────────────────────────────────────────────────

class SpectralRepairPanel : public juce::Component, private juce::Timer
{
  public:
    explicit SpectralRepairPanel (SpectralRepairModule& m)
        : module (m), spectro (m, m.freqLo, m.freqHi)
    {
        addAndMakeVisible (spectro);

        freqLoKnob.addTo (*this);
        freqHiKnob.addTo (*this);
        attenKnob.addTo  (*this);

        bindKnob (freqLoKnob, m.freqLo,    20.0, 10000.0,  10.0, " Hz");
        bindKnob (freqHiKnob, m.freqHi,   100.0, 20000.0,  10.0, " Hz");
        bindKnob (attenKnob,  m.attenDb,     0.0,    60.0,   0.5, " dB");

        latencyLabel.setFont (juce::FontOptions (11.f));
        latencyLabel.setColour (juce::Label::textColourId, textLo ());
        latencyLabel.setJustificationType (juce::Justification::centred);
        latencyLabel.setText ("Latency: " + juce::String (SpectralRepairModule::kLatency) + " smp",
                              juce::dontSendNotification);
        addAndMakeVisible (latencyLabel);

        startTimerHz (10);
    }
    ~SpectralRepairPanel () override { stopTimer (); }

    void setSampleRate (double sr) { spectro.setSampleRate (sr); }

    void resized () override
    {
        auto b = getLocalBounds ();

        // Bottom strip: controls
        auto ctrl = b.removeFromBottom (90);
        latencyLabel.setBounds (ctrl.removeFromBottom (20).reduced (4, 0));
        ctrl.removeFromBottom (4);

        const int knobW = ctrl.getWidth () / 3;
        freqLoKnob.layout (ctrl.removeFromLeft (knobW),    "Freq Lo");
        freqHiKnob.layout (ctrl.removeFromLeft (knobW),    "Freq Hi");
        attenKnob.layout  (ctrl,                           "Attenuate");

        // Spectrogram fills the rest
        spectro.setBounds (b);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (panelBg ());
        g.setColour (divider ());
        g.fillRect (0, getHeight () - 90, getWidth (), 1);
    }

  private:
    void timerCallback () override
    {
        freqLoKnob.updateValue (module.freqLo.load (),  " Hz");
        freqHiKnob.updateValue (module.freqHi.load (),  " Hz");
        attenKnob.updateValue  (module.attenDb.load (), " dB");
    }

    SpectralRepairModule& module;
    SpectrogramDisplay    spectro;
    KnobGroup             freqLoKnob, freqHiKnob, attenKnob;
    juce::Label           latencyLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectralRepairPanel)
};
