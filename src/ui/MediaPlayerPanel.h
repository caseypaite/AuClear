#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../io/FileProcessor.h"

/**
 * Media-player strip for the standalone application (bottom of the editor).
 *
 * Drag-and-drop or browse to load any audio file, or a video file (audio
 * extracted via FFmpeg).  Audio is routed live through the DSP rack so every
 * parameter tweak is heard in real time.  An Export button renders the
 * processed result to a new file offline.
 */
class MediaPlayerPanel : public juce::Component,
                         public juce::FileDragAndDropTarget,
                         public juce::ChangeListener,
                         private juce::Timer
{
  public:
    explicit MediaPlayerPanel (AuClearAudioProcessor& p)
        : proc (p), thumbnail (512, p.getFormatManager (), thumbnailCache)
    {
        setOpaque (true);
        thumbnail.addChangeListener (this);

        // Browse button
        addAndMakeVisible (browseButton);
        applyStyle (browseButton);
        browseButton.setButtonText ("Open...");
        browseButton.onClick = [this] { browse (); };

        // File name label
        addAndMakeVisible (fileLabel);
        fileLabel.setFont (juce::FontOptions (12.f));
        fileLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe8eaed));
        fileLabel.setText ("Drop a file or click Open...", juce::dontSendNotification);

        // Time label (right-aligned)
        addAndMakeVisible (timeLabel);
        timeLabel.setFont (juce::FontOptions (12.f, juce::Font::plain));
        timeLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0ab));
        timeLabel.setJustificationType (juce::Justification::centredRight);

        // Transport: Play/Pause, Stop, Loop
        addAndMakeVisible (playPauseButton);
        applyStyle (playPauseButton);
        playPauseButton.setButtonText ("|>");
        playPauseButton.setEnabled (false);
        playPauseButton.onClick = [this] { onPlayPause (); };

        addAndMakeVisible (stopButton);
        applyStyle (stopButton);
        stopButton.setButtonText ("[]");
        stopButton.setEnabled (false);
        stopButton.onClick = [this]
        {
            proc.getTransportSource ().stop ();
            proc.getTransportSource ().setPosition (0.0);
            playPauseButton.setButtonText ("|>");
            repaint ();
        };

        addAndMakeVisible (loopButton);
        loopButton.setButtonText ("Loop");
        loopButton.setColour (juce::ToggleButton::textColourId, juce::Colour (0xff9aa0ab));
        loopButton.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff28e0c8));
        loopButton.setColour (juce::ToggleButton::tickDisabledColourId,
                              juce::Colour (0xff5a6070));

        // Export button
        addAndMakeVisible (exportButton);
        applyStyle (exportButton);
        exportButton.setButtonText ("Export...");
        exportButton.setEnabled (false);
        exportButton.onClick = [this] { onExport (); };

        // Status / progress
        addAndMakeVisible (statusLabel);
        statusLabel.setFont (juce::FontOptions (11.f));
        statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0ab));
        statusLabel.setJustificationType (juce::Justification::centred);

        addAndMakeVisible (exportProgressBar);
        exportProgressBar.setColour (juce::ProgressBar::foregroundColourId,
                                     juce::Colour (0xff28e0c8));
        exportProgressBar.setVisible (false);

        startTimerHz (30);
    }

    ~MediaPlayerPanel () override
    {
        thumbnail.removeChangeListener (this);
        stopTimer ();
        stopVideoExtract ();
        proc.unloadMediaFile ();
    }

    // ─── Component ───────────────────────────────────────────────────────────
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff13151a));

        // Divider at top
        g.setColour (juce::Colour (0xff2a2e37));
        g.fillRect (0, 0, getWidth (), 1);

        const auto wb = waveformBounds ();

        if (thumbnail.getTotalLength () > 0.0)
        {
            // Waveform background
            g.setColour (juce::Colour (0xff1e2128));
            g.fillRect (wb);

            // Channels
            g.setColour (juce::Colour (0xff28e0c8).withAlpha (0.65f));
            thumbnail.drawChannels (g, wb, 0.0, thumbnail.getTotalLength (), 1.0f);

            // Played region overlay
            const double pos = proc.getTransportSource ().getCurrentPosition ();
            const double dur = thumbnail.getTotalLength ();
            if (dur > 0.0 && pos > 0.0)
            {
                const int playedW = (int)((pos / dur) * wb.getWidth ());
                g.setColour (juce::Colour (0xff28e0c8).withAlpha (0.08f));
                g.fillRect (wb.withWidth (playedW));
            }

            // Playhead
            if (dur > 0.0)
            {
                const int phX = wb.getX () + (int)((pos / dur) * wb.getWidth ());
                g.setColour (juce::Colours::white.withAlpha (0.9f));
                g.drawVerticalLine (phX, (float)wb.getY (), (float)wb.getBottom ());
            }

            // Drag highlight
            if (isDraggingOver)
            {
                g.setColour (juce::Colour (0xff28e0c8).withAlpha (0.25f));
                g.fillRect (wb);
                g.setColour (juce::Colour (0xff28e0c8));
                g.drawRect (wb, 2);
            }
        }
        else
        {
            // Empty / status state
            g.setColour (juce::Colour (0xff1e2128));
            g.fillRect (wb);
            g.setColour (isDraggingOver ? juce::Colour (0xff28e0c8) : juce::Colour (0xff2a2e37));
            g.drawRect (wb, isDraggingOver ? 2 : 1);

            if (! statusText.isEmpty ())
            {
                g.setColour (juce::Colour (0xff9aa0ab));
                g.setFont (12.f);
                g.drawText (statusText, wb.reduced (8, 0), juce::Justification::centred, true);
            }
            else
            {
                g.setColour (juce::Colour (0xff5a6070));
                g.setFont (12.f);
                g.drawText ("Drop audio / video file here", wb.reduced (8, 0),
                            juce::Justification::centred);
            }
        }
    }

    void resized () override
    {
        auto b = getLocalBounds ().reduced (8, 6);

        // Top row: Browse | file name | time | transport | Loop | Export
        auto topRow = b.removeFromTop (26);
        browseButton.setBounds (topRow.removeFromLeft (64).withSizeKeepingCentre (60, 24));
        topRow.removeFromLeft (6);
        exportButton.setBounds (topRow.removeFromRight (64).withSizeKeepingCentre (60, 24));
        topRow.removeFromRight (6);
        loopButton.setBounds (topRow.removeFromRight (50).withSizeKeepingCentre (50, 20));
        topRow.removeFromRight (4);
        stopButton.setBounds (topRow.removeFromRight (28).withSizeKeepingCentre (28, 24));
        topRow.removeFromRight (2);
        playPauseButton.setBounds (topRow.removeFromRight (34).withSizeKeepingCentre (32, 24));
        topRow.removeFromRight (8);
        timeLabel.setBounds (topRow.removeFromRight (100));
        topRow.removeFromRight (4);
        fileLabel.setBounds (topRow);

        b.removeFromTop (4);

        // Waveform region
        const auto wb = b.removeFromTop (50);
        // (stored below)

        b.removeFromTop (4);

        // Status + export progress share the bottom row
        auto botRow = b.removeFromTop (16);
        exportProgressBar.setBounds (botRow);
        statusLabel.setBounds (botRow);
    }

    void mouseDown (const juce::MouseEvent& e) override { seekIfInWaveform (e.x, e.y); }
    void mouseDrag (const juce::MouseEvent& e) override { seekIfInWaveform (e.x, e.y); }

    // ─── FileDragAndDropTarget ────────────────────────────────────────────────
    bool isInterestedInFileDrag (const juce::StringArray& f) override { return f.size () == 1; }

    void fileDragEnter (const juce::StringArray&, int, int) override
    {
        isDraggingOver = true;
        repaint ();
    }
    void fileDragExit (const juce::StringArray&) override
    {
        isDraggingOver = false;
        repaint ();
    }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        isDraggingOver = false;
        if (files.size () == 1)
            loadFile (juce::File (files[0]));
    }

    // ─── ChangeListener (AudioThumbnail ready) ───────────────────────────────
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint (); }

  private:
    // ─── Video extraction thread ──────────────────────────────────────────────
    struct VideoExtractThread : public juce::Thread
    {
        juce::File videoFile, outputWav, ffmpegExe;
        std::function<void (bool)> onDone;

        VideoExtractThread () : juce::Thread ("AuClear-VideoExtract") {}

        void run () override
        {
            juce::StringArray args;
            const juce::String exe =
                ffmpegExe.existsAsFile () ? ffmpegExe.getFullPathName () : "ffmpeg";
            args.add (exe);
            args.addArray ({"-y", "-i", videoFile.getFullPathName (), "-vn", "-acodec",
                            "pcm_f32le", "-ar", "48000", "-ac", "2", "-f", "wav",
                            outputWav.getFullPathName ()});

            juce::ChildProcess child;
            const bool started = child.start (args, juce::ChildProcess::wantStdErr);
            if (started)
            {
                while (child.isRunning () && ! threadShouldExit ())
                    juce::Thread::sleep (100);

                if (threadShouldExit ())
                {
                    child.kill ();
                    return;
                }
            }

            const bool ok = started && child.getExitCode () == 0;
            auto cb = onDone;
            juce::MessageManager::callAsync ([cb, ok] { if (cb) cb (ok); });
        }
    };

    // ─────────────────────────────────────────────────────────────────────────
    AuClearAudioProcessor& proc;

    juce::AudioThumbnailCache thumbnailCache{5};
    juce::AudioThumbnail thumbnail;

    juce::File loadedFile;
    std::unique_ptr<juce::TemporaryFile> videoTempWav;
    std::unique_ptr<VideoExtractThread> extractThread;

    juce::TextButton browseButton{"Open..."};
    juce::Label fileLabel;
    juce::Label timeLabel;
    juce::TextButton playPauseButton{"|>"};
    juce::TextButton stopButton{"[]"};
    juce::ToggleButton loopButton{"Loop"};
    juce::TextButton exportButton{"Export..."};
    juce::Label statusLabel;
    double exportProgress{0.0};
    juce::ProgressBar exportProgressBar{exportProgress};

    std::unique_ptr<juce::FileChooser> fileChooser;
    FileProcessor exportProcessor;

    bool isDraggingOver{false};
    juce::String statusText;

    // ─── Helpers ─────────────────────────────────────────────────────────────
    void applyStyle (juce::TextButton& btn)
    {
        btn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2e37));
        btn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffe8eaed));
        btn.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff1a1d24));
    }

    juce::Rectangle<int> waveformBounds () const
    {
        // Mirror resized() waveform calculation
        auto b = getLocalBounds ().reduced (8, 6);
        b.removeFromTop (26 + 4);
        return b.removeFromTop (50);
    }

    void seekIfInWaveform (int mouseX, int mouseY)
    {
        if (thumbnail.getTotalLength () <= 0.0)
            return;
        const auto wb = waveformBounds ();
        if (! wb.contains (mouseX, mouseY))
            return;
        const double t =
            juce::jlimit (0.0, thumbnail.getTotalLength (),
                          (double)(mouseX - wb.getX ()) / wb.getWidth () * thumbnail.getTotalLength ());
        proc.getTransportSource ().setPosition (t);
        repaint ();
    }

    static juce::String formatTime (double s)
    {
        s = std::max (0.0, s);
        const int mins = (int)(s / 60.0);
        const int secs = (int)(std::fmod (s, 60.0));
        return juce::String::formatted ("%d:%02d", mins, secs);
    }

    void timerCallback () override
    {
        auto& ts = proc.getTransportSource ();

        if (proc.isMediaFileLoaded ())
        {
            const double pos = ts.getCurrentPosition ();
            const double dur = ts.getLengthInSeconds ();
            timeLabel.setText (formatTime (pos) + " / " + formatTime (dur),
                               juce::dontSendNotification);

            if (ts.hasStreamFinished ())
            {
                if (loopButton.getToggleState ())
                {
                    ts.setPosition (0.0);
                    ts.start ();
                }
                else
                {
                    ts.setPosition (0.0);
                    playPauseButton.setButtonText ("|>");
                }
            }
        }

        repaint ();
    }

    void onPlayPause ()
    {
        auto& ts = proc.getTransportSource ();
        if (ts.isPlaying ())
        {
            ts.stop ();
            playPauseButton.setButtonText ("|>");
        }
        else
        {
            ts.start ();
            playPauseButton.setButtonText ("||");
        }
    }

    void browse ()
    {
        fileChooser =
            std::make_unique<juce::FileChooser> ("Open audio or video file",
                                                 juce::File::getSpecialLocation (
                                                     juce::File::userMusicDirectory));
        fileChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                if (fc.getResult ().existsAsFile ())
                    loadFile (fc.getResult ());
            });
    }

    void loadFile (const juce::File& file)
    {
        stopVideoExtract ();
        proc.getTransportSource ().stop ();
        proc.unloadMediaFile ();
        thumbnail.clear ();
        statusText = "Loading...";
        timeLabel.setText ({}, juce::dontSendNotification);
        playPauseButton.setButtonText ("|>");
        playPauseButton.setEnabled (false);
        stopButton.setEnabled (false);
        exportButton.setEnabled (false);
        repaint ();

        if (proc.loadMediaFile (file))
        {
            onFileReady (file, file);
        }
        else if (FileProcessor::isVideoFile (file))
        {
            const juce::String ffmpeg = FileProcessor::findFfmpeg ();
            if (ffmpeg.isEmpty ())
            {
                statusText = "FFmpeg not found — install it to open video files.";
                repaint ();
                return;
            }
            statusText = "Extracting audio from video...";
            repaint ();
            startVideoExtract (file);
        }
        else
        {
            statusText = "Could not open file.";
            repaint ();
        }
    }

    void onFileReady (const juce::File& originalFile, const juce::File& audioFile)
    {
        loadedFile = originalFile;
        statusText.clear ();
        fileLabel.setText (originalFile.getFileName (), juce::dontSendNotification);
        thumbnail.setSource (new juce::FileInputSource (audioFile));
        playPauseButton.setEnabled (true);
        stopButton.setEnabled (true);
        exportButton.setEnabled (true);

        proc.getTransportSource ().setPosition (0.0);
        proc.getTransportSource ().start ();
        playPauseButton.setButtonText ("||");
        repaint ();
    }

    // ─── Video extraction ─────────────────────────────────────────────────────
    void startVideoExtract (const juce::File& videoFile)
    {
        videoTempWav = std::make_unique<juce::TemporaryFile> (".wav");

        extractThread = std::make_unique<VideoExtractThread> ();
        extractThread->videoFile = videoFile;
        extractThread->outputWav = videoTempWav->getFile ();

        juce::Component::SafePointer<MediaPlayerPanel> self (this);
        extractThread->onDone = [self, videoFile] (bool ok)
        {
            if (auto* panel = self.getComponent ())
                panel->onVideoExtractDone (videoFile, ok);
        };
        extractThread->startThread ();
    }

    void onVideoExtractDone (const juce::File& videoFile, bool ok)
    {
        if (! ok || ! videoTempWav)
        {
            statusText = "FFmpeg audio extraction failed.";
            repaint ();
            return;
        }
        if (! proc.loadMediaFile (videoTempWav->getFile ()))
        {
            statusText = "Could not load extracted audio.";
            repaint ();
            return;
        }
        onFileReady (videoFile, videoTempWav->getFile ());
    }

    void stopVideoExtract ()
    {
        if (extractThread)
        {
            extractThread->signalThreadShouldExit ();
            extractThread->stopThread (3000);
            extractThread.reset ();
        }
    }

    // ─── Export ───────────────────────────────────────────────────────────────
    void onExport ()
    {
        if (! proc.isMediaFileLoaded () || loadedFile == juce::File ())
            return;

        const juce::File suggested = FileProcessor::suggestOutput (loadedFile);
        fileChooser = std::make_unique<juce::FileChooser> ("Export processed audio", suggested);
        fileChooser->launchAsync (
            juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto outFile = fc.getResult ();
                if (outFile == juce::File ())
                    return;
                startExport (outFile);
            });
    }

    void startExport (const juce::File& outFile)
    {
        // Determine which audio file to export from
        const juce::File audioSrc =
            (videoTempWav && videoTempWav->getFile ().existsAsFile ())
                ? videoTempWav->getFile ()
                : loadedFile;

        double sr = 48000.0;
        int nch = 2;
        {
            juce::AudioFormatManager fmt;
            fmt.registerBasicFormats ();
            if (auto r = std::unique_ptr<juce::AudioFormatReader> (fmt.createReaderFor (audioSrc)))
            {
                sr = r->sampleRate;
                nch = juce::jlimit (1, 2, (int)r->numChannels);
            }
        }

        juce::ValueTree rackState ("Rack");
        proc.getRack ().getState (rackState);
        exportProcessor.prepareLocalRack (rackState, sr, nch);

        exportProgress = 0.0;
        exportProgressBar.setVisible (true);
        statusLabel.setText ("Exporting...", juce::dontSendNotification);
        exportButton.setEnabled (false);

        juce::Component::SafePointer<MediaPlayerPanel> self (this);

        exportProcessor.startJob (
            audioSrc, outFile,
            [self] (float p, juce::String msg)
            {
                juce::MessageManager::callAsync (
                    [self, p, m = std::move (msg)]
                    {
                        if (auto* panel = self.getComponent ())
                        {
                            panel->exportProgress = (double)p;
                            panel->statusLabel.setText (m, juce::dontSendNotification);
                        }
                    });
            },
            [self, outFile] (bool ok, juce::String msg)
            {
                juce::MessageManager::callAsync (
                    [self, ok, m = std::move (msg), outFile]
                    {
                        if (auto* panel = self.getComponent ())
                        {
                            panel->exportProgressBar.setVisible (false);
                            panel->statusLabel.setText (m, juce::dontSendNotification);
                            panel->exportButton.setEnabled (true);
                            if (ok)
                                outFile.revealToUser ();
                        }
                    });
            });
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MediaPlayerPanel)
};
