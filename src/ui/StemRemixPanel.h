#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../engine/StemState.h"
#include "panels/PanelHelpers.h"
#include <array>
#include <memory>

// ─── Single stem channel strip ────────────────────────────────────────────────

class StemChannelStrip : public juce::Component
{
  public:
    static constexpr int kHdrH  = 22;
    static constexpr int kWaveH = 24;

    StemChannelStrip (juce::String stemName, juce::Colour col_, StemState& state)
        : name (std::move (stemName)), col (col_), st (state)
    {
        auto styleToggle = [&] (juce::TextButton& btn, juce::Colour onCol)
        {
            btn.setClickingTogglesState (true);
            btn.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff2a2e37));
            btn.setColour (juce::TextButton::buttonOnColourId, onCol);
            btn.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xff9aa0ab));
            btn.setColour (juce::TextButton::textColourOnId,   juce::Colours::black);
            addAndMakeVisible (btn);
        };

        styleToggle (soloBtn, juce::Colour (0xffffcf00));
        soloBtn.setButtonText ("S");
        soloBtn.onClick = [this] { st.soloed.store (soloBtn.getToggleState (), std::memory_order_relaxed); };

        styleToggle (muteBtn, juce::Colour (0xffff5252));
        muteBtn.setButtonText ("M");
        muteBtn.onClick = [this] { st.muted.store (muteBtn.getToggleState (), std::memory_order_relaxed); };

        auto styleSlider = [&] (juce::Slider& s)
        {
            s.setSliderStyle (juce::Slider::LinearHorizontal);
            s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            s.setColour (juce::Slider::thumbColourId,      col);
            s.setColour (juce::Slider::trackColourId,      juce::Colour (0xff2a2e37));
            s.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff1a1d24));
            addAndMakeVisible (s);
        };

        styleSlider (gainSlider);
        gainSlider.setRange (0.0, 2.0, 0.01);
        gainSlider.setValue (1.0, juce::dontSendNotification);
        gainSlider.onValueChange = [this]
        { st.gain.store ((float)gainSlider.getValue (), std::memory_order_relaxed); };

        styleSlider (panSlider);
        panSlider.setRange (-1.0, 1.0, 0.01);
        panSlider.setValue (0.0, juce::dontSendNotification);
        panSlider.onValueChange = [this]
        { st.pan.store ((float)panSlider.getValue (), std::memory_order_relaxed); };

        for (auto* lbl : {&gainLbl, &panLbl})
        {
            lbl->setFont (juce::FontOptions (9.f));
            lbl->setColour (juce::Label::textColourId, juce::Colour (0xff6a7080));
            lbl->setJustificationType (juce::Justification::centredRight);
            addAndMakeVisible (*lbl);
        }
        gainLbl.setText ("Gain", juce::dontSendNotification);
        panLbl.setText  ("Pan",  juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1a1d24));

        const auto hdr = getLocalBounds ().removeFromTop (kHdrH);
        g.setColour (col.withAlpha (0.88f));
        g.fillRect (hdr);
        g.setFont (juce::Font (juce::FontOptions (10.f).withStyle ("Bold")));
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.drawText (name.toUpperCase (), hdr.reduced (5, 0), juce::Justification::centredLeft);

        // Activity bar — filled proportional to gain, shows "live" feel
        const auto bar = getLocalBounds ().withTop (kHdrH).removeFromTop (kWaveH).reduced (3, 4);
        g.setColour (juce::Colour (0xff13151a));
        g.fillRect (bar);
        const float g_val = juce::jmin (1.f, st.gain.load (std::memory_order_relaxed) / 2.f);
        if (g_val > 0.f && ! st.muted.load (std::memory_order_relaxed))
        {
            g.setColour (col.withAlpha (0.5f));
            g.fillRect (bar.withWidth ((int)(bar.getWidth () * g_val)));
        }

        g.setColour (juce::Colour (0xff2a2e37));
        g.fillRect (getWidth () - 1, 0, 1, getHeight ());
    }

    void resized () override
    {
        auto b = getLocalBounds ();
        b.removeFromTop (kHdrH + kWaveH);
        b.reduce (5, 3);

        auto smRow = b.removeFromTop (20);
        soloBtn.setBounds (smRow.removeFromLeft (24).withSizeKeepingCentre (22, 17));
        smRow.removeFromLeft (3);
        muteBtn.setBounds (smRow.removeFromLeft (24).withSizeKeepingCentre (22, 17));
        b.removeFromTop (4);

        auto gr = b.removeFromTop (17);
        gainLbl.setBounds (gr.removeFromLeft (28));
        gainSlider.setBounds (gr);
        b.removeFromTop (3);

        auto pr = b.removeFromTop (17);
        panLbl.setBounds (pr.removeFromLeft (28));
        panSlider.setBounds (pr);
    }

  private:
    juce::String  name;
    juce::Colour  col;
    StemState&    st;

    juce::TextButton soloBtn{"S"}, muteBtn{"M"};
    juce::Slider     gainSlider, panSlider;
    juce::Label      gainLbl, panLbl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StemChannelStrip)
};

// ─── Stem Remix Panel ─────────────────────────────────────────────────────────
// Shares the same audio source as the rest of the processor (no separate file
// picker).  The real-time Demucs inference is driven by the live processBlock
// stream; this panel only controls the model, the enable state, and the per-stem
// mix parameters.

class StemRemixPanel : public juce::Component,
                       private juce::Timer
{
  public:
    explicit StemRemixPanel (AuClearAudioProcessor& p) : proc (p)
    {
        setOpaque (true);

        // ── Header controls ───────────────────────────────────────────────────
        loadModelBtn.setButtonText ("Load Model...");
        loadModelBtn.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff2a2e37));
        loadModelBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffe8eaed));
        loadModelBtn.onClick = [this] { browseModel (); };
        addAndMakeVisible (loadModelBtn);

        enableBtn.setButtonText ("Enable");
        enableBtn.setClickingTogglesState (true);
        enableBtn.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff2a2e37));
        enableBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff28e0c8).withAlpha (0.3f));
        enableBtn.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xff9aa0ab));
        enableBtn.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xff28e0c8));
        enableBtn.setEnabled (false);
        enableBtn.onClick = [this]
        {
            proc.getRealtimeStemProcessor ().setEnabled (enableBtn.getToggleState ());
            repaint ();
        };
        addAndMakeVisible (enableBtn);

        modelLabel.setFont (juce::FontOptions (10.f));
        modelLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0ab));
        modelLabel.setText ("No model", juce::dontSendNotification);
        addAndMakeVisible (modelLabel);

        statusLabel.setFont (juce::FontOptions (10.f));
        statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0ab));
        statusLabel.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (statusLabel);

        latencyLabel.setFont (juce::FontOptions (9.f));
        latencyLabel.setColour (juce::Label::textColourId, juce::Colour (0xff6a7080));
        latencyLabel.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (latencyLabel);

        // ── Channel strips ────────────────────────────────────────────────────
        static constexpr struct { const char* name; juce::uint32 col; } kDefs[4] = {
            {"drums",  0xff4a9eff},
            {"bass",   0xffff7043},
            {"other",  0xffab47bc},
            {"vocals", 0xffec407a},
        };
        auto& sp = proc.getRealtimeStemProcessor ();
        for (int i = 0; i < 4; ++i)
        {
            strips[i] = std::make_unique<StemChannelStrip> (
                kDefs[i].name, juce::Colour (kDefs[i].col), sp.stems[i]);
            addAndMakeVisible (*strips[i]);
        }

        // ── Sidebar ───────────────────────────────────────────────────────────
        dryMixSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        dryMixSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        dryMixSlider.setRange (0.0, 1.0, 0.01);
        dryMixSlider.setValue (0.0, juce::dontSendNotification);
        dryMixSlider.setColour (juce::Slider::thumbColourId, juce::Colour (0xff28e0c8));
        dryMixSlider.setColour (juce::Slider::trackColourId, juce::Colour (0xff2a2e37));
        dryMixSlider.onValueChange = [this]
        { proc.getRealtimeStemProcessor ().dryMix.store ((float)dryMixSlider.getValue (),
                                                          std::memory_order_relaxed); };
        addAndMakeVisible (dryMixSlider);

        dryMixLabel.setText ("Dry Mix", juce::dontSendNotification);
        dryMixLabel.setFont (juce::FontOptions (9.f));
        dryMixLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0ab));
        dryMixLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (dryMixLabel);

        startTimerHz (5); // status refresh
    }

    ~StemRemixPanel () override { stopTimer (); }

    // ── Component ─────────────────────────────────────────────────────────────
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff13151a));
        g.setColour (juce::Colour (0xff2a2e37));
        g.fillRect (0, 0, getWidth (), 1); // top divider

        g.setColour (juce::Colour (0xff1e2128));
        g.fillRect (0, 1, getWidth (), kHeaderH - 1);

        g.setFont (juce::Font (juce::FontOptions (11.f).withStyle ("Bold")));
        g.setColour (juce::Colour (0xff28e0c8));
        g.drawText ("STEM REMIX", 8, 0, 96, kHeaderH, juce::Justification::centredLeft);

        // Active indicator dot
        const bool active = proc.getRealtimeStemProcessor ().isActive ();
        g.setColour (active ? juce::Colour (0xff28e0c8) : juce::Colour (0xff3a3e47));
        g.fillEllipse (102.f, (float)(kHeaderH - 7) / 2.f, 7.f, 7.f);

        // Strip/sidebar divider
        g.setColour (juce::Colour (0xff2a2e37));
        g.fillRect (getWidth () - sidebarWidth () - 1, kHeaderH, 1, getHeight () - kHeaderH);
    }

    void resized () override
    {
        auto b = getLocalBounds ();
        b.removeFromTop (1); // divider line

        // Header row
        {
            auto hdr = getLocalBounds ().withTop (1).removeFromTop (kHeaderH);
            hdr.removeFromLeft (114); // "STEM REMIX" + dot
            enableBtn.setBounds     (hdr.removeFromRight (58).withSizeKeepingCentre (54, 20));
            hdr.removeFromRight (4);
            loadModelBtn.setBounds  (hdr.removeFromRight (102).withSizeKeepingCentre (98, 20));
            hdr.removeFromRight (4);
            statusLabel.setBounds   (hdr.removeFromRight (120).withSizeKeepingCentre (118, 14));
            hdr.removeFromRight (4);
            latencyLabel.setBounds  (hdr.removeFromRight (100).withSizeKeepingCentre (98, 12));
            modelLabel.setBounds    (hdr.reduced (4, 3));
        }

        b.removeFromTop (kHeaderH);
        b.removeFromTop (3);

        // Strips (left) + sidebar (right)
        const int sw = sidebarWidth ();
        auto sidebar = b.removeFromRight (sw);
        b.removeFromLeft (1);

        // Sidebar
        sidebar.reduce (6, 6);
        dryMixLabel.setBounds  (sidebar.removeFromTop (14));
        sidebar.removeFromTop  (3);
        dryMixSlider.setBounds (sidebar.removeFromTop (16));

        // Four equal strips
        if (b.getWidth () > 0)
        {
            const int sw4 = b.getWidth () / 4;
            for (auto& strip : strips)
                strip->setBounds (b.removeFromLeft (sw4));
        }
    }

  private:
    static constexpr int kHeaderH = 28;
    int sidebarWidth () const noexcept { return juce::jmax (130, getWidth () / 5); }

    // ── Model ─────────────────────────────────────────────────────────────────
    void browseModel ()
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Select Demucs ONNX model",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "*.onnx");
        fileChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult ();
                if (! f.existsAsFile ()) return;

                modelLabel.setText ("Loading...", juce::dontSendNotification);
                enableBtn.setEnabled (false);

                const bool ok = proc.loadStemModel (f);

                modelLabel.setText (ok ? f.getFileNameWithoutExtension () : "Load failed",
                                    juce::dontSendNotification);
                enableBtn.setEnabled (ok);

                if (ok)
                {
                    const double latSec = (double)proc.getRealtimeStemProcessor ().latencySamples ()
                                          / juce::jmax (1.0, proc.getSampleRate ());
                    latencyLabel.setText (juce::String (latSec, 1) + "s lag",
                                          juce::dontSendNotification);
                }
            });
    }

    // ── Timer ─────────────────────────────────────────────────────────────────
    void timerCallback () override
    {
        auto& sp = proc.getRealtimeStemProcessor ();
        statusLabel.setText (sp.getStatusString (), juce::dontSendNotification);

        // Repaint strips at 5 Hz to animate the gain bar
        if (sp.isActive ())
            for (auto& s : strips) s->repaint ();

        repaint (); // refresh active dot
    }

    // ── Members ───────────────────────────────────────────────────────────────
    AuClearAudioProcessor& proc;

    std::array<std::unique_ptr<StemChannelStrip>, 4> strips;

    juce::TextButton openSourceBtn, loadModelBtn, enableBtn;
    juce::Label      modelLabel, statusLabel, latencyLabel;

    juce::Slider dryMixSlider;
    juce::Label  dryMixLabel;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StemRemixPanel)
};
