#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../engine/StemState.h"
#include "panels/PanelHelpers.h"
#include "panels/DownloadThread.h"
#include <array>
#include <memory>
#include <vector>

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
        gainSlider.onValueChange = [this]
        { st.gain.store ((float)gainSlider.getValue (), std::memory_order_relaxed); };

        styleSlider (panSlider);
        panSlider.setRange (-1.0, 1.0, 0.01);
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

        syncWithState ();
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

    void syncWithState ()
    {
        gainSlider.setValue (st.gain.load (), juce::dontSendNotification);
        panSlider.setValue (st.pan.load (), juce::dontSendNotification);
        soloBtn.setToggleState (st.soloed.load (), juce::dontSendNotification);
        muteBtn.setToggleState (st.muted.load (), juce::dontSendNotification);
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

class StemRemixPanel : public juce::Component,
                       private juce::Timer
{
  public:
    struct ModelOption
    {
        juce::String name;
        juce::String filename;
        juce::String url;
        bool isRemote;
        juce::File localFile;
    };

    explicit StemRemixPanel (AuClearAudioProcessor& p) : proc (p)
    {
        setOpaque (true);

        // ── Header controls ───────────────────────────────────────────────────
        modelSelector.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2a2e37));
        modelSelector.setColour (juce::ComboBox::textColourId, juce::Colour (0xffe8eaed));
        addAndMakeVisible (modelSelector);

        browseBtn.setButtonText ("Browse...");
        browseBtn.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff2a2e37));
        browseBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffe8eaed));
        browseBtn.onClick = [this] { browseModel (); };
        addAndMakeVisible (browseBtn);

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
            strips[(size_t)i] = std::make_unique<StemChannelStrip> (
                kDefs[(size_t)i].name, juce::Colour (kDefs[(size_t)i].col), sp.stems[(size_t)i]);
            addAndMakeVisible (*strips[(size_t)i]);
        }

        // ── Sidebar ───────────────────────────────────────────────────────────
        dryMixSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        dryMixSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        dryMixSlider.setRange (0.0, 1.0, 0.01);
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

        modelSelector.onChange = [this]
        {
            int selectedIdx = modelSelector.getSelectedId () - 2;
            if (selectedIdx >= 0 && selectedIdx < (int)modelOptions.size ())
            {
                const auto& opt = modelOptions[(size_t)selectedIdx];
                if (opt.isRemote)
                {
                    startDownload (opt);
                }
                else
                {
                    loadModelFile (opt.localFile);
                }
            }
            else
            {
                proc.unloadStemModel ();
                enableBtn.setEnabled (false);
                latencyLabel.setText ("", juce::dontSendNotification);
            }
        };

        // Listen to processor model changes
        proc.getRealtimeStemProcessor ().modelStatusChanged = [this]
        {
            populateModelSelector ();
            auto& sProcessor = proc.getRealtimeStemProcessor ();
            enableBtn.setEnabled (sProcessor.isModelLoaded ());
            if (sProcessor.isModelLoaded ())
            {
                const double latSec = (double)sProcessor.latencySamples ()
                                      / juce::jmax (1.0, proc.getSampleRate ());
                latencyLabel.setText (juce::String (latSec, 1) + "s lag",
                                      juce::dontSendNotification);
            }
            else
            {
                latencyLabel.setText ("", juce::dontSendNotification);
            }
        };

        syncWithProcessor ();
        populateModelSelector ();
        startTimerHz (5); // status refresh
    }

    ~StemRemixPanel () override
    {
        stopTimer ();
        if (downloadThread != nullptr)
            downloadThread->stopThread (4000);
        proc.getRealtimeStemProcessor ().modelStatusChanged = nullptr;
    }

    void syncWithProcessor ()
    {
        auto& sp = proc.getRealtimeStemProcessor ();
        enableBtn.setToggleState (sp.isEnabled (), juce::dontSendNotification);
        enableBtn.setEnabled (sp.isModelLoaded ());
        dryMixSlider.setValue (sp.dryMix.load (), juce::dontSendNotification);

        for (int i = 0; i < 4; ++i)
        {
            if (strips[(size_t)i] != nullptr)
                strips[(size_t)i]->syncWithState ();
        }
    }

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
            browseBtn.setBounds     (hdr.removeFromRight (70).withSizeKeepingCentre (66, 20));
            hdr.removeFromRight (4);
            modelSelector.setBounds (hdr.removeFromRight (160).withSizeKeepingCentre (156, 20));
            hdr.removeFromRight (4);
            statusLabel.setBounds   (hdr.removeFromRight (100).withSizeKeepingCentre (98, 14));
            hdr.removeFromRight (4);
            latencyLabel.setBounds  (hdr.reduced (4, 3));
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

    juce::File getSharedModelsDir () const
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                       .getChildFile (".auclear")
                       .getChildFile ("models");
        if (! dir.exists ())
            dir.createDirectory ();
        return dir;
    }

    void loadModelFile (const juce::File& f)
    {
        enableBtn.setEnabled (false);
        const bool ok = proc.loadStemModel (f);
        enableBtn.setEnabled (ok);

        if (ok)
        {
            const double latSec = (double)proc.getRealtimeStemProcessor ().latencySamples ()
                                  / juce::jmax (1.0, proc.getSampleRate ());
            latencyLabel.setText (juce::String (latSec, 1) + "s lag",
                                  juce::dontSendNotification);
        }
        else
        {
            latencyLabel.setText ("Load failed", juce::dontSendNotification);
        }
    }

    void browseModel ()
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Select Demucs ONNX model",
            getSharedModelsDir (),
            "*.onnx");
        fileChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult ();
                if (! f.existsAsFile ()) return;

                auto destFile = getSharedModelsDir ().getChildFile (f.getFileName ());
                if (f.getFullPathName () != destFile.getFullPathName ())
                {
                    if (destFile.existsAsFile ())
                        destFile.deleteFile ();
                    f.copyFileTo (destFile);
                }

                loadModelFile (destFile);
                populateModelSelector ();
            });
    }

    void populateModelSelector ()
    {
        modelSelector.clear (juce::dontSendNotification);
        modelOptions.clear ();

        struct TempDef { juce::String name; juce::String filename; juce::String url; };
        std::vector<TempDef> defaults = {
            { "Demucs (HTDemucs)", "htdemucs.onnx", "https://raw.githubusercontent.com/caseypaite/AuClear/main/models/htdemucs.onnx" }
        };

        auto sharedDir = getSharedModelsDir ();

        for (const auto& d : defaults)
        {
            auto localFile = sharedDir.getChildFile (d.filename);
            bool exists = localFile.existsAsFile ();
            
            ModelOption opt;
            opt.filename = d.filename;
            opt.url = d.url;
            opt.localFile = localFile;

            if (exists)
            {
                opt.name = d.name;
                opt.isRemote = false;
            }
            else
            {
                opt.name = d.name + " (Download)";
                opt.isRemote = true;
            }
            modelOptions.push_back (opt);
        }

        juce::Array<juce::File> customFiles;
        sharedDir.findChildFiles (customFiles, juce::File::findFiles, false, "*.onnx");
        
        for (const auto& file : customFiles)
        {
            auto fname = file.getFileName ();
            if (fname != "identity.onnx" && fname != "gate.onnx" && fname != "htdemucs.onnx")
            {
                ModelOption opt;
                opt.name = file.getFileNameWithoutExtension () + " (Custom)";
                opt.filename = fname;
                opt.url = "";
                opt.isRemote = false;
                opt.localFile = file;
                modelOptions.push_back (opt);
            }
        }

        modelSelector.addItem ("Select Model...", 1);
        int itemIndex = 2;
        for (size_t i = 0; i < modelOptions.size (); ++i)
        {
            modelSelector.addItem (modelOptions[i].name, itemIndex++);
        }

        auto& sp = proc.getRealtimeStemProcessor ();
        auto currentFile = sp.getModelFile ();
        if (currentFile.existsAsFile ())
        {
            int selectedId = 1;
            for (size_t i = 0; i < modelOptions.size (); ++i)
            {
                if (modelOptions[i].localFile.getFullPathName () == currentFile.getFullPathName ())
                {
                    selectedId = (int)(i + 2);
                    break;
                }
            }
            modelSelector.setSelectedId (selectedId, juce::dontSendNotification);
        }
        else
        {
            modelSelector.setSelectedId (1, juce::dontSendNotification);
        }
    }

    void startDownload (const ModelOption& opt)
    {
        modelSelector.setEnabled (false);
        browseBtn.setEnabled (false);
        enableBtn.setEnabled (false);
        
        statusLabel.setText ("Connecting...", juce::dontSendNotification);

        juce::Component::SafePointer<StemRemixPanel> safeThis (this);

        downloadThread = std::make_unique<DownloadThread> (
            opt.filename,
            opt.url,
            opt.localFile,
            [safeThis, opt] (float progress) {
                if (safeThis != nullptr)
                {
                    if (progress >= 0.f)
                    {
                        int pct = juce::roundToInt (progress * 100.f);
                        safeThis->statusLabel.setText ("Downloading " + opt.filename + ": " + juce::String (pct) + "%", juce::dontSendNotification);
                    }
                    else
                    {
                        double mb = -progress / (1024.0 * 1024.0);
                        safeThis->statusLabel.setText ("Downloading " + opt.filename + ": " + juce::String (mb, 2) + " MB", juce::dontSendNotification);
                    }
                }
            },
            [safeThis, opt] (bool success, juce::String error) {
                if (safeThis != nullptr)
                {
                    safeThis->modelSelector.setEnabled (true);
                    safeThis->browseBtn.setEnabled (true);

                    if (success)
                    {
                        safeThis->statusLabel.setText ("Done!", juce::dontSendNotification);
                        safeThis->loadModelFile (opt.localFile);
                        safeThis->populateModelSelector ();
                    }
                    else
                    {
                        safeThis->statusLabel.setText ("Failed: " + error, juce::dontSendNotification);
                        safeThis->populateModelSelector ();
                    }
                }
            }
        );

        downloadThread->startThread ();
    }

    // ── Timer ─────────────────────────────────────────────────────────────────
    void timerCallback () override
    {
        if (downloadThread != nullptr && ! downloadThread->isThreadRunning ())
        {
            downloadThread.reset ();
        }

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

    juce::TextButton browseBtn, enableBtn;
    juce::ComboBox   modelSelector;
    juce::Label      statusLabel, latencyLabel;

    juce::Slider dryMixSlider;
    juce::Label  dryMixLabel;

    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<DownloadThread> downloadThread;

    std::vector<ModelOption> modelOptions;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StemRemixPanel)
};
