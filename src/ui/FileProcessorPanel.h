#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../io/FileProcessor.h"

/**
 * File-processor UI, shown only in standalone mode.
 *
 * Supports drag-and-drop or click-to-browse for audio and video files.
 * Video files are remuxed via FFmpeg (stream-copy video, replace audio).
 */
class FileProcessorPanel : public juce::Component, public juce::FileDragAndDropTarget
{
  public:
    explicit FileProcessorPanel (AuClearAudioProcessor& p) : proc (p)
    {
        setOpaque (true);

        addAndMakeVisible (titleLabel);
        titleLabel.setText ("Process File", juce::dontSendNotification);
        titleLabel.setFont (juce::FontOptions (18.f, juce::Font::bold));
        titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe8eaed));

        addAndMakeVisible (ffmpegLabel);
        ffmpegLabel.setFont (juce::FontOptions (11.f));
        ffmpegLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0ab));
        ffmpegLabel.setJustificationType (juce::Justification::centredRight);
        refreshFfmpegLabel ();

        // Drop zone
        addAndMakeVisible (dropZoneLabel);
        dropZoneLabel.setText ("Drop audio or video file here, or click Browse…",
                               juce::dontSendNotification);
        dropZoneLabel.setFont (juce::FontOptions (13.f));
        dropZoneLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0ab));
        dropZoneLabel.setJustificationType (juce::Justification::centred);

        addAndMakeVisible (inputPathLabel);
        inputPathLabel.setFont (juce::FontOptions (12.f));
        inputPathLabel.setColour (juce::Label::textColourId, juce::Colour (0xff28e0c8));
        inputPathLabel.setJustificationType (juce::Justification::centred);
        inputPathLabel.setVisible (false);

        addAndMakeVisible (browseButton);
        browseButton.setButtonText ("Browse…");
        applyStyle (browseButton, false);
        browseButton.onClick = [this] { browseForInput (); };

        addAndMakeVisible (clearButton);
        clearButton.setButtonText ("Clear");
        applyStyle (clearButton, false);
        clearButton.setVisible (false);
        clearButton.onClick = [this] { clearInput (); };

        addAndMakeVisible (outputHeading);
        outputHeading.setText ("Output:", juce::dontSendNotification);
        outputHeading.setFont (juce::FontOptions (12.f));
        outputHeading.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0ab));

        addAndMakeVisible (outputPathLabel);
        outputPathLabel.setFont (juce::FontOptions (12.f));
        outputPathLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe8eaed));
        outputPathLabel.setJustificationType (juce::Justification::centredLeft);

        addAndMakeVisible (changeOutputButton);
        changeOutputButton.setButtonText ("Change…");
        applyStyle (changeOutputButton, false);
        changeOutputButton.setVisible (false);
        changeOutputButton.onClick = [this] { browseForOutput (); };

        addAndMakeVisible (processButton);
        processButton.setButtonText ("Process");
        applyStyle (processButton, true);
        processButton.setEnabled (false);
        processButton.onClick = [this] { onProcessClicked (); };

        addAndMakeVisible (progressBar);
        progressBar.setColour (juce::ProgressBar::foregroundColourId, juce::Colour (0xff28e0c8));
        progressBar.setVisible (false);

        addAndMakeVisible (statusLabel);
        statusLabel.setFont (juce::FontOptions (12.f));
        statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0ab));
        statusLabel.setJustificationType (juce::Justification::centred);
    }

    ~FileProcessorPanel () override { fileProcessor.cancel (); }

    // ─────────────────────────────────────────────────────────────────────────
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1a1d24));

        if (!inputFile.existsAsFile ())
        {
            // Drop-zone border
            const auto r = dropZoneLabel.getBounds ().expanded (12, 8).toFloat ();
            const auto col = isDragging ? juce::Colour (0xff28e0c8) : juce::Colour (0xff2a2e37);
            g.setColour (col);
            g.drawRoundedRectangle (r, 6.f, isDragging ? 2.f : 1.f);
        }
    }

    void resized () override
    {
        auto b = getLocalBounds ().reduced (20);

        // Title + ffmpeg status
        auto titleRow = b.removeFromTop (28);
        titleLabel.setBounds (titleRow.removeFromLeft (200));
        ffmpegLabel.setBounds (titleRow);
        b.removeFromTop (14);

        // Drop zone with Browse/Clear on the right
        auto dropRow = b.removeFromTop (80);
        auto btnCol = dropRow.removeFromRight (90);
        browseButton.setBounds (btnCol.removeFromTop (30).withSizeKeepingCentre (80, 28));
        btnCol.removeFromTop (6);
        clearButton.setBounds (btnCol.removeFromTop (30).withSizeKeepingCentre (80, 28));

        // Both the zone label and input path sit in the same region
        auto dropInner = dropRow.reduced (8, 0);
        dropZoneLabel.setBounds (dropInner);
        inputPathLabel.setBounds (dropInner);

        b.removeFromTop (14);

        // Output row
        auto outRow = b.removeFromTop (22);
        outputHeading.setBounds (outRow.removeFromLeft (56));
        changeOutputButton.setBounds (outRow.removeFromRight (80).withSizeKeepingCentre (80, 24));
        outputPathLabel.setBounds (outRow.reduced (6, 0));

        b.removeFromTop (18);

        // Process button
        processButton.setBounds (b.removeFromTop (36).withSizeKeepingCentre (140, 36));
        b.removeFromTop (12);

        // Progress bar + status
        progressBar.setBounds (b.removeFromTop (14));
        b.removeFromTop (6);
        statusLabel.setBounds (b.removeFromTop (20));
    }

    // ─────────────────────────────────────────────────────────────────────────
    // FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        return files.size () == 1;
    }

    void fileDragEnter (const juce::StringArray&, int, int) override
    {
        isDragging = true;
        repaint ();
    }

    void fileDragExit (const juce::StringArray&) override
    {
        isDragging = false;
        repaint ();
    }

    void filesDropped (const juce::StringArray& files, int, int) override
    {
        isDragging = false;
        repaint ();
        if (files.size () == 1)
            setInputFile (juce::File (files[0]));
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Callbacks from FileProcessor (already marshalled to message thread)
    void onProgress (float p, const juce::String& msg)
    {
        progressValue = (double)p;
        statusLabel.setText (msg, juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0ab));
    }

    void onDone (bool ok, const juce::String& msg)
    {
        isProcessing = false;
        progressBar.setVisible (false);
        progressValue = 0.0;
        processButton.setButtonText ("Process");
        processButton.setEnabled (inputFile.existsAsFile ());
        statusLabel.setText (msg, juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId,
                               ok ? juce::Colour (0xff28e0c8) : juce::Colour (0xffee5555));
        if (ok && outputFile.existsAsFile ())
            outputFile.revealToUser ();
    }

  private:
    AuClearAudioProcessor& proc;
    FileProcessor fileProcessor;

    juce::File inputFile, outputFile;
    bool isDragging{false};
    bool isProcessing{false};
    double progressValue{0.0};

    juce::Label titleLabel, ffmpegLabel;
    juce::Label dropZoneLabel, inputPathLabel;
    juce::TextButton browseButton{"Browse..."};
    juce::TextButton clearButton{"Clear"};
    juce::Label outputHeading, outputPathLabel;
    juce::TextButton changeOutputButton{"Change..."};
    juce::TextButton processButton{"Process"};
    juce::ProgressBar progressBar{progressValue};
    juce::Label statusLabel;
    std::unique_ptr<juce::FileChooser> activeChooser;

    // ─────────────────────────────────────────────────────────────────────────
    void applyStyle (juce::TextButton& btn, bool accent)
    {
        btn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2e37));
        btn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff28e0c8));
        btn.setColour (juce::TextButton::textColourOffId,
                       accent ? juce::Colour (0xff28e0c8) : juce::Colour (0xffe8eaed));
        btn.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff1a1d24));
    }

    void refreshFfmpegLabel ()
    {
        const bool found = !FileProcessor::findFfmpeg ().isEmpty ();
        ffmpegLabel.setText (found ? "ffmpeg: found" : "ffmpeg: not found (video disabled)",
                             juce::dontSendNotification);
    }

    void setInputFile (const juce::File& f)
    {
        if (!f.existsAsFile ())
            return;
        inputFile = f;
        outputFile = FileProcessor::suggestOutput (f);

        dropZoneLabel.setVisible (false);
        inputPathLabel.setVisible (true);
        inputPathLabel.setText (f.getFileName (), juce::dontSendNotification);
        clearButton.setVisible (true);
        changeOutputButton.setVisible (true);
        outputPathLabel.setText (outputFile.getFileName (), juce::dontSendNotification);
        processButton.setEnabled (true);
        statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0ab));
        statusLabel.setText ({}, juce::dontSendNotification);
        repaint ();
    }

    void clearInput ()
    {
        inputFile = juce::File ();
        outputFile = juce::File ();
        dropZoneLabel.setVisible (true);
        inputPathLabel.setVisible (false);
        clearButton.setVisible (false);
        changeOutputButton.setVisible (false);
        outputPathLabel.setText ({}, juce::dontSendNotification);
        processButton.setEnabled (false);
        statusLabel.setText ({}, juce::dontSendNotification);
        repaint ();
    }

    void browseForInput ()
    {
        activeChooser = std::make_unique<juce::FileChooser> (
            "Open audio or video file",
            juce::File::getSpecialLocation (juce::File::userMusicDirectory));

        activeChooser->launchAsync (juce::FileBrowserComponent::openMode |
                                        juce::FileBrowserComponent::canSelectFiles,
                                    [this] (const juce::FileChooser& fc)
                                    {
                                        const auto result = fc.getResult ();
                                        if (result.existsAsFile ())
                                            setInputFile (result);
                                    });
    }

    void browseForOutput ()
    {
        const juce::File startDir =
            outputFile.existsAsFile ()
                ? outputFile
                : juce::File::getSpecialLocation (juce::File::userMusicDirectory);

        activeChooser = std::make_unique<juce::FileChooser> ("Save processed file as", startDir);

        activeChooser->launchAsync (
            juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto result = fc.getResult ();
                if (result != juce::File ())
                {
                    outputFile = result;
                    outputPathLabel.setText (outputFile.getFileName (), juce::dontSendNotification);
                }
            });
    }

    void onProcessClicked ()
    {
        if (isProcessing)
        {
            fileProcessor.cancel ();
            isProcessing = false;
            processButton.setButtonText ("Process");
            progressBar.setVisible (false);
            statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0ab));
            statusLabel.setText ("Cancelled.", juce::dontSendNotification);
            processButton.setEnabled (inputFile.existsAsFile ());
            return;
        }

        if (!inputFile.existsAsFile ())
            return;

        // Snapshot the rack state (message thread) before preparing the local rack
        juce::ValueTree rackState ("Rack");
        proc.getRack ().getState (rackState);

        const bool isVideo = FileProcessor::isVideoFile (inputFile);
        double sr = 48000.0;
        int nch = 2;

        if (!isVideo)
        {
            juce::AudioFormatManager fmt;
            fmt.registerBasicFormats ();
            if (auto r = std::unique_ptr<juce::AudioFormatReader> (fmt.createReaderFor (inputFile)))
            {
                sr = r->sampleRate;
                nch = juce::jlimit (1, 2, (int)r->numChannels);
            }
        }

        // Set up the isolated rack on the message thread before starting the thread
        fileProcessor.prepareLocalRack (rackState, sr, nch);

        isProcessing = true;
        processButton.setButtonText ("Cancel");
        progressValue = 0.0;
        progressBar.setVisible (true);
        statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0ab));
        statusLabel.setText ("Starting…", juce::dontSendNotification);

        juce::Component::SafePointer<FileProcessorPanel> self (this);

        fileProcessor.startJob (
            inputFile, outputFile,
            [self] (float p, juce::String msg)
            {
                juce::MessageManager::callAsync (
                    [self, p, m = std::move (msg)]
                    {
                        if (auto* panel = self.getComponent ())
                            panel->onProgress (p, m);
                    });
            },
            [self] (bool ok, juce::String msg)
            {
                juce::MessageManager::callAsync (
                    [self, ok, m = std::move (msg)]
                    {
                        if (auto* panel = self.getComponent ())
                            panel->onDone (ok, m);
                    });
            });
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FileProcessorPanel)
};
