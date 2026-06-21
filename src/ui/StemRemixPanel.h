#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../offline/OfflineJobManager.h"
#include "../engine/StemState.h"
#include "panels/PanelHelpers.h"
#include <array>
#include <memory>

// ─── Single stem channel strip ────────────────────────────────────────────────

class StemChannelStrip : public juce::Component
{
  public:
    static constexpr int kHdrH  = 22;
    static constexpr int kWaveH = 28;

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

        // Colour header bar
        const auto hdr = getLocalBounds ().removeFromTop (kHdrH);
        g.setColour (col.withAlpha (0.88f));
        g.fillRect (hdr);
        g.setFont (juce::Font (juce::FontOptions (10.f).withStyle ("Bold")));
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.drawText (name.toUpperCase (), hdr.reduced (5, 0), juce::Justification::centredLeft);

        // Stem loaded / waveform area
        const auto wave = getLocalBounds ().withTop (kHdrH).removeFromTop (kWaveH).reduced (2, 2);
        g.setColour (juce::Colour (0xff13151a));
        g.fillRect (wave);
        g.setFont (juce::FontOptions (9.f));

        if (st.stemFile.existsAsFile ())
        {
            const int barH = juce::jmax (2, wave.getHeight () / 2);
            g.setColour (col.withAlpha (0.45f));
            g.fillRect (wave.withSizeKeepingCentre (wave.getWidth (), barH));
            g.setColour (col.withAlpha (0.75f));
            g.drawText (st.stemFile.getFileName (), wave.reduced (2, 0),
                        juce::Justification::centred, true);
        }
        else
        {
            g.setColour (juce::Colour (0xff3a3e47));
            g.drawText ("no stem", wave, juce::Justification::centred);
        }

        // Right-edge divider
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

class StemRemixPanel : public juce::Component,
                       public juce::FileDragAndDropTarget,
                       public juce::ChangeListener,
                       private juce::Timer
{
  public:
    explicit StemRemixPanel (AuClearAudioProcessor& p)
        : proc (p), srcThumbnail (512, thumbnailFmt, thumbnailCache)
    {
        thumbnailFmt.registerBasicFormats ();
        srcThumbnail.addChangeListener (this);
        setOpaque (true);

        // ── Header controls ───────────────────────────────────────────────────
        auto stylePrim = [&] (juce::TextButton& btn, const juce::String& text)
        {
            btn.setButtonText (text);
            btn.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff2a2e37));
            btn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffe8eaed));
            addAndMakeVisible (btn);
        };

        stylePrim (openSourceBtn, "Open Source...");
        openSourceBtn.onClick = [this] { browseSource (); };

        stylePrim (loadModelBtn, "Load Model...");
        loadModelBtn.onClick = [this] { browseModel (); };

        srcFileLabel.setFont (juce::FontOptions (11.f));
        srcFileLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe8eaed));
        srcFileLabel.setText ("Drop audio file or click Open Source...", juce::dontSendNotification);
        addAndMakeVisible (srcFileLabel);

        modelLabel.setFont (juce::FontOptions (10.f));
        modelLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0ab));
        modelLabel.setText ("No model loaded", juce::dontSendNotification);
        addAndMakeVisible (modelLabel);

        // ── Channel strips — bound to stemPlayer.stems[i] in the processor ───
        static constexpr struct { const char* name; juce::uint32 col; } kDefs[4] = {
            {"drums",  0xff4a9eff},
            {"bass",   0xffff7043},
            {"other",  0xffab47bc},
            {"vocals", 0xffec407a},
        };
        auto& sp = proc.getStemPlayer ();
        for (int i = 0; i < 4; ++i)
        {
            strips[i] = std::make_unique<StemChannelStrip> (
                kDefs[i].name, juce::Colour (kDefs[i].col), sp.stems[i]);
            addAndMakeVisible (*strips[i]);
        }

        // ── Sidebar ───────────────────────────────────────────────────────────
        separateBtn.setButtonText ("Separate Stems");
        separateBtn.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff28e0c8).withAlpha (0.15f));
        separateBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff28e0c8));
        separateBtn.setEnabled (false);
        separateBtn.onClick = [this] { startSeparation (); };
        addAndMakeVisible (separateBtn);

        cancelBtn.setButtonText ("Cancel");
        cancelBtn.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff2a2e37));
        cancelBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffff5252));
        cancelBtn.setVisible (false);
        cancelBtn.onClick = [this]
        {
            if (activeJobId.isNotEmpty ())
                jobManager.cancel (activeJobId);
        };
        addAndMakeVisible (cancelBtn);

        statusLabel.setFont (juce::FontOptions (10.f));
        statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0ab));
        statusLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (statusLabel);

        progressBar.setColour (juce::ProgressBar::foregroundColourId, juce::Colour (0xff28e0c8));
        progressBar.setVisible (false);
        addAndMakeVisible (progressBar);

        // Dry-mix slider writes directly into stemPlayer's atomic.
        dryMixSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        dryMixSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        dryMixSlider.setRange (0.0, 1.0, 0.01);
        dryMixSlider.setValue (0.0, juce::dontSendNotification);
        dryMixSlider.setColour (juce::Slider::thumbColourId, juce::Colour (0xff28e0c8));
        dryMixSlider.setColour (juce::Slider::trackColourId, juce::Colour (0xff2a2e37));
        dryMixSlider.onValueChange = [this]
        { proc.getStemPlayer ().dryMix.store ((float)dryMixSlider.getValue (), std::memory_order_relaxed); };
        addAndMakeVisible (dryMixSlider);

        dryMixLabel.setText ("Dry Mix", juce::dontSendNotification);
        dryMixLabel.setFont (juce::FontOptions (9.f));
        dryMixLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0ab));
        dryMixLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (dryMixLabel);

        unloadBtn.setButtonText ("Unload Stems");
        unloadBtn.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff2a2e37));
        unloadBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff9aa0ab));
        unloadBtn.setVisible (false);
        unloadBtn.onClick = [this]
        {
            proc.unloadStems ();
            for (auto& s : proc.getStemPlayer ().stems)
                s.stemFile = juce::File ();
            for (auto& strip : strips) strip->repaint ();
            unloadBtn.setVisible (false);
            statusLabel.setText ({}, juce::dontSendNotification);
        };
        addAndMakeVisible (unloadBtn);

        startTimerHz (10);
    }

    ~StemRemixPanel () override
    {
        stopTimer ();
        srcThumbnail.removeChangeListener (this);
    }

    // ── ChangeListener ────────────────────────────────────────────────────────
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint (); }

    // ── Component ─────────────────────────────────────────────────────────────
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff13151a));

        g.setColour (juce::Colour (0xff2a2e37));
        g.fillRect (0, 0, getWidth (), 1);

        g.setColour (juce::Colour (0xff1e2128));
        g.fillRect (0, 1, getWidth (), kHeaderH - 1);
        g.setFont (juce::Font (juce::FontOptions (11.f).withStyle ("Bold")));
        g.setColour (juce::Colour (0xff28e0c8));
        g.drawText ("STEM REMIX", 8, 0, 96, kHeaderH, juce::Justification::centredLeft);

        // Active indicator
        if (proc.getStemPlayer ().isActive ())
        {
            g.setColour (juce::Colour (0xff28e0c8));
            g.fillEllipse (102.f, (kHeaderH - 7) / 2.f, 7.f, 7.f);
        }

        const auto wf = waveformBounds ();
        g.setColour (juce::Colour (0xff1e2128));
        g.fillRect (wf);

        if (srcThumbnail.getTotalLength () > 0.0)
        {
            g.setColour (juce::Colour (0xff28e0c8).withAlpha (0.55f));
            srcThumbnail.drawChannels (g, wf, 0.0, srcThumbnail.getTotalLength (), 1.0f);
        }
        else
        {
            g.setColour (juce::Colour (0xff3a3e47));
            g.setFont (10.f);
            g.drawText (isDraggingOver ? "Drop audio file here" : "No source loaded",
                        wf.reduced (4), juce::Justification::centred);
        }

        if (isDraggingOver)
        {
            g.setColour (juce::Colour (0xff28e0c8).withAlpha (0.18f));
            g.fillRect (wf);
            g.setColour (juce::Colour (0xff28e0c8));
            g.drawRect (wf, 2);
        }

        g.setColour (juce::Colour (0xff2a2e37));
        g.fillRect (getWidth () - sidebarWidth () - 1, kHeaderH, 1, getHeight () - kHeaderH);
    }

    void resized () override
    {
        auto b = getLocalBounds ();
        b.removeFromTop (1);
        b.removeFromTop (kHeaderH);

        {
            auto hdr = getLocalBounds ().withTop (1).removeFromTop (kHeaderH);
            hdr.removeFromLeft (114);
            loadModelBtn.setBounds  (hdr.removeFromRight (100).withSizeKeepingCentre (96, 22));
            hdr.removeFromRight (4);
            openSourceBtn.setBounds (hdr.removeFromRight (100).withSizeKeepingCentre (96, 22));
            hdr.removeFromRight (6);
            modelLabel.setBounds    (hdr.removeFromRight (130).withSizeKeepingCentre (128, 14));
            hdr.removeFromRight (4);
            srcFileLabel.setBounds  (hdr.reduced (4, 3));
        }

        b.removeFromTop (2);
        b.removeFromTop (kWaveH);
        b.removeFromTop (3);

        const int sw = sidebarWidth ();
        auto sidebar = b.removeFromRight (sw);
        b.removeFromLeft (1);

        sidebar.reduce (6, 4);
        separateBtn.setBounds (sidebar.removeFromTop (26).withSizeKeepingCentre (sidebar.getWidth (), 22));
        sidebar.removeFromTop (3);
        cancelBtn.setBounds   (sidebar.removeFromTop (22).withSizeKeepingCentre (sidebar.getWidth (), 18));
        unloadBtn.setBounds   (cancelBtn.getBounds ());  // same slot, exclusive visibility
        sidebar.removeFromTop (3);
        progressBar.setBounds (sidebar.removeFromTop (12));
        statusLabel.setBounds (sidebar.removeFromTop (20));
        sidebar.removeFromTop (8);
        dryMixLabel.setBounds (sidebar.removeFromTop (14));
        sidebar.removeFromTop (2);
        dryMixSlider.setBounds (sidebar.removeFromTop (16));

        if (b.getWidth () > 0)
        {
            const int sw4 = b.getWidth () / 4;
            for (auto& strip : strips)
                strip->setBounds (b.removeFromLeft (sw4));
        }
    }

    // ── FileDragAndDropTarget ─────────────────────────────────────────────────
    bool isInterestedInFileDrag (const juce::StringArray& f) override { return f.size () == 1; }
    void fileDragEnter (const juce::StringArray&, int, int) override  { isDraggingOver = true;  repaint (); }
    void fileDragExit  (const juce::StringArray&) override            { isDraggingOver = false; repaint (); }
    void filesDropped  (const juce::StringArray& files, int, int) override
    {
        isDraggingOver = false;
        if (files.size () == 1)
            loadSourceFile (juce::File (files[0]));
    }

  private:
    static constexpr int kHeaderH = 28;
    static constexpr int kWaveH   = 34;

    int sidebarWidth () const noexcept { return juce::jmax (145, getWidth () / 5); }

    juce::Rectangle<int> waveformBounds () const
    {
        return getLocalBounds ()
            .withTop  (1 + kHeaderH + 2)
            .removeFromTop (kWaveH)
            .reduced (4, 0);
    }

    // ── Source file ───────────────────────────────────────────────────────────
    void browseSource ()
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Open source audio file",
            juce::File::getSpecialLocation (juce::File::userMusicDirectory));
        fileChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                if (fc.getResult ().existsAsFile ())
                    loadSourceFile (fc.getResult ());
            });
    }

    void loadSourceFile (const juce::File& file)
    {
        sourceFile = file;
        srcFileLabel.setText (file.getFileName (), juce::dontSendNotification);
        srcThumbnail.setSource (new juce::FileInputSource (file));
        updateButtons ();
        repaint ();
    }

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
                if (fc.getResult ().existsAsFile ())
                {
                    modelFile = fc.getResult ();
                    modelLabel.setText (modelFile.getFileNameWithoutExtension (),
                                       juce::dontSendNotification);
                    updateButtons ();
                }
            });
    }

    void updateButtons ()
    {
        const bool ready = sourceFile.existsAsFile () && modelFile.existsAsFile () && !isSeparating;
        separateBtn.setEnabled (ready);
        separateBtn.setVisible (!isSeparating);
        cancelBtn.setVisible (isSeparating);
        unloadBtn.setVisible (!isSeparating && proc.getStemPlayer ().isActive ());
        progressBar.setVisible (isSeparating);
    }

    // ── Separation ────────────────────────────────────────────────────────────
    void startSeparation ()
    {
        if (! sourceFile.existsAsFile () || ! modelFile.existsAsFile ())
            return;

        const juce::File outputDir =
            sourceFile.getParentDirectory ()
                .getChildFile (sourceFile.getFileNameWithoutExtension () + "_stems");

        auto job       = std::make_shared<OfflineJob> ();
        job->type      = JobType::DemucsStems;
        job->inputFile = sourceFile;
        job->outputDir = outputDir;
        job->params.setProperty ("modelPath", modelFile.getFullPathName (), nullptr);

        juce::Component::SafePointer<StemRemixPanel> self (this);
        job->onDone = [self, outputDir] (bool ok, juce::String msg)
        {
            if (auto* panel = self.getComponent ())
                panel->onSeparationDone (ok, msg, outputDir);
        };

        activeJobId  = jobManager.submit (std::move (job));
        isSeparating = true;
        progressVal  = 0.0;
        statusLabel.setText ("Queued...", juce::dontSendNotification);
        updateButtons ();
    }

    void onSeparationDone (bool ok, const juce::String& msg, const juce::File& outputDir)
    {
        isSeparating = false;
        updateButtons ();

        if (! ok)
        {
            statusLabel.setText (msg, juce::dontSendNotification);
            return;
        }

        statusLabel.setText ("Loading stems...", juce::dontSendNotification);
        loadStemFiles (outputDir);
    }

    void loadStemFiles (const juce::File& outputDir)
    {
        static const char* kNames[4] = {"drums", "bass", "other", "vocals"};
        const juce::String base = sourceFile.getFileNameWithoutExtension ();

        std::array<juce::File, 4> stemFiles;
        for (int i = 0; i < 4; ++i)
            stemFiles[i] = outputDir.getChildFile (base + "_" + kNames[i] + ".wav");

        const bool loaded = proc.loadStems (stemFiles);
        statusLabel.setText (loaded ? "Stems active!" : "Load failed",
                             juce::dontSendNotification);

        auto& sp = proc.getStemPlayer ();
        for (int i = 0; i < 4; ++i)
        {
            sp.stems[i].stemFile = (loaded && stemFiles[i].existsAsFile ())
                                       ? stemFiles[i]
                                       : juce::File ();
            strips[i]->repaint ();
        }

        updateButtons ();
        repaint (); // refresh active indicator
    }

    // ── Timer ─────────────────────────────────────────────────────────────────
    void timerCallback () override
    {
        if (! isSeparating)
            return;

        for (auto& job : jobManager.getJobs ())
        {
            if (job->id.toString () != activeJobId)
                continue;

            progressVal = (double)job->progress.load (std::memory_order_relaxed);
            const auto stateVal = job->state.load (std::memory_order_acquire);
            const auto msg = job->getStatusMessage ();

            if (! msg.isEmpty ())
                statusLabel.setText (msg, juce::dontSendNotification);

            if (stateVal == JobState::Cancelled)
            {
                isSeparating = false;
                statusLabel.setText ("Cancelled.", juce::dontSendNotification);
                updateButtons ();
            }
            break;
        }
    }

    // ── Members ───────────────────────────────────────────────────────────────
    AuClearAudioProcessor& proc;

    // strips reference proc.getStemPlayer().stems[i] — declared after proc
    std::array<std::unique_ptr<StemChannelStrip>, 4> strips;

    OfflineJobManager jobManager;

    juce::AudioFormatManager  thumbnailFmt;
    juce::AudioThumbnailCache thumbnailCache{4};
    juce::AudioThumbnail      srcThumbnail;

    juce::File sourceFile, modelFile;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::TextButton openSourceBtn, loadModelBtn;
    juce::Label      srcFileLabel, modelLabel;

    juce::TextButton  separateBtn, cancelBtn, unloadBtn;
    juce::Label       statusLabel;
    double            progressVal{0.0};
    juce::ProgressBar progressBar{progressVal};
    juce::Slider      dryMixSlider;
    juce::Label       dryMixLabel;

    juce::String activeJobId;
    bool         isSeparating{false};
    bool         isDraggingOver{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StemRemixPanel)
};
