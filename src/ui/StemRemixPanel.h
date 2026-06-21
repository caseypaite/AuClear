#pragma once
#include <JuceHeader.h>
#include "../offline/OfflineJobManager.h"
#include "panels/PanelHelpers.h"
#include <array>
#include <atomic>
#include <memory>

// ─── Per-stem mix state ───────────────────────────────────────────────────────
// Atomic members are safe to read from the audio thread; everything else is
// message-thread only.

struct StemState
{
    std::atomic<float> gain{1.0f};   // 0–2 linear
    std::atomic<float> pan{0.0f};    // -1 to +1
    std::atomic<bool>  muted{false};
    std::atomic<bool>  soloed{false};
    juce::File         stemFile;     // message thread only

    StemState () = default;
    StemState (const StemState&) = delete;
    StemState& operator= (const StemState&) = delete;
};

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
            s.setColour (juce::Slider::thumbColourId,         col);
            s.setColour (juce::Slider::trackColourId,         juce::Colour (0xff2a2e37));
            s.setColour (juce::Slider::backgroundColourId,    juce::Colour (0xff1a1d24));
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
        panLbl.setText ("Pan",   juce::dontSendNotification);
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

        // Mini waveform / status area
        const auto wave = getLocalBounds ().withTop (kHdrH).removeFromTop (kWaveH).reduced (2, 2);
        g.setColour (juce::Colour (0xff13151a));
        g.fillRect (wave);
        g.setFont (juce::FontOptions (9.f));

        if (st.stemFile.existsAsFile ())
        {
            // Loaded: draw a simple filled bar placeholder until full thumbnail is wired
            const int barH = juce::jmax (2, wave.getHeight () / 2);
            g.setColour (col.withAlpha (0.45f));
            g.fillRect (wave.withSizeKeepingCentre (wave.getWidth (), barH));
            g.setColour (col.withAlpha (0.75f));
            g.setFont (9.f);
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

        // Solo / Mute
        auto smRow = b.removeFromTop (20);
        soloBtn.setBounds (smRow.removeFromLeft (24).withSizeKeepingCentre (22, 17));
        smRow.removeFromLeft (3);
        muteBtn.setBounds (smRow.removeFromLeft (24).withSizeKeepingCentre (22, 17));
        b.removeFromTop (4);

        // Gain
        auto gr = b.removeFromTop (17);
        gainLbl.setBounds (gr.removeFromLeft (28));
        gainSlider.setBounds (gr);
        b.removeFromTop (3);

        // Pan
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
    // Read-only access for audio-thread routing (Phase 4b)
    const std::array<StemState, 4>& getStemStates () const noexcept { return stemStates; }
    float dryMix () const noexcept { return dryMixVal.load (std::memory_order_relaxed); }

    StemRemixPanel () : srcThumbnail (512, thumbnailFmt, thumbnailCache)
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

        // ── Channel strips ────────────────────────────────────────────────────
        static constexpr struct { const char* name; juce::uint32 col; } kDefs[4] = {
            {"drums",  0xff4a9eff},
            {"bass",   0xffff7043},
            {"other",  0xffab47bc},
            {"vocals", 0xffec407a},
        };
        for (int i = 0; i < 4; ++i)
        {
            strips[i] = std::make_unique<StemChannelStrip> (
                kDefs[i].name, juce::Colour (kDefs[i].col), stemStates[i]);
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

        dryMixSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        dryMixSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        dryMixSlider.setRange (0.0, 1.0, 0.01);
        dryMixSlider.setValue (0.0, juce::dontSendNotification);
        dryMixSlider.setColour (juce::Slider::thumbColourId,  juce::Colour (0xff28e0c8));
        dryMixSlider.setColour (juce::Slider::trackColourId,  juce::Colour (0xff2a2e37));
        dryMixSlider.onValueChange = [this]
        { dryMixVal.store ((float)dryMixSlider.getValue (), std::memory_order_relaxed); };
        addAndMakeVisible (dryMixSlider);

        dryMixLabel.setText ("Dry Mix", juce::dontSendNotification);
        dryMixLabel.setFont (juce::FontOptions (9.f));
        dryMixLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0ab));
        dryMixLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (dryMixLabel);

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

        // Top divider
        g.setColour (juce::Colour (0xff2a2e37));
        g.fillRect (0, 0, getWidth (), 1);

        // Header bar
        g.setColour (juce::Colour (0xff1e2128));
        g.fillRect (0, 1, getWidth (), kHeaderH - 1);
        g.setFont (juce::Font (juce::FontOptions (11.f).withStyle ("Bold")));
        g.setColour (juce::Colour (0xff28e0c8));
        g.drawText ("STEM REMIX", 8, 0, 96, kHeaderH, juce::Justification::centredLeft);

        // Source waveform area
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

        // Divider between strips and sidebar
        g.setColour (juce::Colour (0xff2a2e37));
        const int sideX = getWidth () - sidebarWidth ();
        g.fillRect (sideX - 1, kHeaderH, 1, getHeight () - kHeaderH);
    }

    void resized () override
    {
        auto b = getLocalBounds ();
        b.removeFromTop (1);            // divider
        b.removeFromTop (kHeaderH);     // header bar painted manually

        // Header control row
        {
            auto hdr = getLocalBounds ().withTop (1).removeFromTop (kHeaderH);
            hdr.removeFromLeft (100);   // "STEM REMIX" title
            loadModelBtn.setBounds  (hdr.removeFromRight (100).withSizeKeepingCentre (96, 22));
            hdr.removeFromRight (4);
            openSourceBtn.setBounds (hdr.removeFromRight (100).withSizeKeepingCentre (96, 22));
            hdr.removeFromRight (6);
            modelLabel.setBounds    (hdr.removeFromRight (130).withSizeKeepingCentre (128, 14));
            hdr.removeFromRight (4);
            srcFileLabel.setBounds  (hdr.reduced (4, 3));
        }

        b.removeFromTop (2);

        // Waveform row (painted above, set bounds for mouse events)
        b.removeFromTop (kWaveH);
        b.removeFromTop (3);

        // Remaining: strips (left) + sidebar (right)
        const int sw = sidebarWidth ();
        auto sidebar = b.removeFromRight (sw);
        b.removeFromLeft (1);

        // Sidebar layout
        sidebar.reduce (6, 4);
        separateBtn.setBounds (sidebar.removeFromTop (26).withSizeKeepingCentre (sidebar.getWidth (), 22));
        sidebar.removeFromTop (3);
        cancelBtn.setBounds   (sidebar.removeFromTop (22).withSizeKeepingCentre (sidebar.getWidth (), 18));
        sidebar.removeFromTop (3);
        progressBar.setBounds (sidebar.removeFromTop (12));
        statusLabel.setBounds (sidebar.removeFromTop (20));
        sidebar.removeFromTop (10);
        dryMixLabel.setBounds (sidebar.removeFromTop (14));
        sidebar.removeFromTop (2);
        dryMixSlider.setBounds (sidebar.removeFromTop (16));

        // Four equal channel strips
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
    // ── Constants ─────────────────────────────────────────────────────────────
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
            if (auto* p = self.getComponent ())
                p->onSeparationDone (ok, msg, outputDir);
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
        statusLabel.setText (ok ? "Done!" : msg, juce::dontSendNotification);
        updateButtons ();
        if (ok)
            loadStemFiles (outputDir);
    }

    void loadStemFiles (const juce::File& outputDir)
    {
        static const char* kNames[4] = {"drums", "bass", "other", "vocals"};
        const juce::String base = sourceFile.getFileNameWithoutExtension ();
        for (int i = 0; i < 4; ++i)
        {
            const auto f = outputDir.getChildFile (base + "_" + kNames[i] + ".wav");
            stemStates[i].stemFile = f.existsAsFile () ? f : juce::File{};
            strips[i]->repaint ();
        }
    }

    // ── Timer: poll job progress ──────────────────────────────────────────────
    void timerCallback () override
    {
        if (! isSeparating)
            return;

        for (auto& job : jobManager.getJobs ())
        {
            if (job->id.toString () != activeJobId)
                continue;

            progressVal = (double)job->progress.load (std::memory_order_relaxed);
            const auto st = job->state.load (std::memory_order_acquire);
            const auto msg = job->getStatusMessage ();

            if (! msg.isEmpty ())
                statusLabel.setText (msg, juce::dontSendNotification);

            if (st == JobState::Cancelled)
            {
                isSeparating = false;
                statusLabel.setText ("Cancelled.", juce::dontSendNotification);
                updateButtons ();
            }
            break;
        }
    }

    // ── Members ───────────────────────────────────────────────────────────────
    // Declare stemStates before strips so strips' constructors can take references.
    std::array<StemState, 4>                         stemStates;
    std::array<std::unique_ptr<StemChannelStrip>, 4> strips;

    OfflineJobManager jobManager;

    juce::AudioFormatManager  thumbnailFmt;
    juce::AudioThumbnailCache thumbnailCache{4};
    juce::AudioThumbnail      srcThumbnail;

    juce::File sourceFile, modelFile;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::TextButton openSourceBtn, loadModelBtn;
    juce::Label      srcFileLabel, modelLabel;

    juce::TextButton  separateBtn, cancelBtn;
    juce::Label       statusLabel;
    double            progressVal{0.0};
    juce::ProgressBar progressBar{progressVal};
    juce::Slider      dryMixSlider;
    juce::Label       dryMixLabel;

    std::atomic<float> dryMixVal{0.0f};

    juce::String activeJobId;
    bool         isSeparating{false};
    bool         isDraggingOver{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StemRemixPanel)
};
