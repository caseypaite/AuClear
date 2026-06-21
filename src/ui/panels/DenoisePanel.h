#pragma once
#include "PanelHelpers.h"
#include "../../modules/DenoiseModule.h"

/**
 * Control panel for the DenoiseModule.
 *
 * Layout:
 *   [Status bar — model path / error]
 *   [Load Model...] [Listen to Removed]
 *   [Strength knob]
 */
#include "DownloadThread.h"
#include <vector>

class DenoisePanel : public juce::Component, public juce::Timer
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

    explicit DenoisePanel (DenoiseModule& m) : mod (m)
    {
        bindKnob (strength, m.strength, 0.0, 1.0, 0.01, "");

        addAndMakeVisible (strength.slider);
        addAndMakeVisible (strength.label);
        addAndMakeVisible (strength.value);

        addAndMakeVisible (modelSelector);
        
        browseBtn.setButtonText ("Browse...");
        browseBtn.onClick = [this]
        {
            auto chooser = std::make_shared<juce::FileChooser> (
                "Select ONNX Model",
                getSharedModelsDir (),
                "*.onnx");

            chooser->launchAsync (juce::FileBrowserComponent::openMode |
                                      juce::FileBrowserComponent::canSelectFiles,
                                  [this, chooser] (const juce::FileChooser& fc)
                                  {
                                      auto results = fc.getResults ();
                                      if (results.isEmpty ())
                                          return;
                                      
                                      auto chosenFile = results.getFirst ();
                                      auto destFile = getSharedModelsDir ().getChildFile (chosenFile.getFileName ());
                                      
                                      if (chosenFile.getFullPathName () != destFile.getFullPathName ())
                                      {
                                          if (destFile.existsAsFile ())
                                              destFile.deleteFile ();
                                          chosenFile.copyFileTo (destFile);
                                      }
                                      
                                      mod.setModelFile (destFile);
                                      populateModelSelector ();
                                      updateStatus ();
                                  });
        };
        addAndMakeVisible (browseBtn);

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
                    mod.setModelFile (opt.localFile);
                    updateStatus ();
                }
            }
            else
            {
                mod.setModelFile (juce::File ());
                updateStatus ();
            }
        };

        listenBtn.setButtonText ("Listen to Removed");
        listenBtn.setClickingTogglesState (true);
        listenBtn.setToggleState (m.listenToRemoved.load (), juce::dontSendNotification);
        listenBtn.onStateChange = [this]
        { mod.listenToRemoved.store (listenBtn.getToggleState ()); };
        addAndMakeVisible (listenBtn);

        statusLabel.setFont (juce::FontOptions (12.f));
        statusLabel.setColour (juce::Label::textColourId, textLo ());
        statusLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (statusLabel);

        cpuLabel.setFont (juce::FontOptions (11.f));
        cpuLabel.setJustificationType (juce::Justification::centredRight);
        cpuLabel.setColour (juce::Label::textColourId, textLo ());
        addAndMakeVisible (cpuLabel);

        // Populate and select current
        populateModelSelector ();

        // Refresh status when model loads
        mod.modelStatusChanged = [this] 
        { 
            populateModelSelector ();
            updateStatus (); 
        };

        updateStatus ();
        startTimerHz (4); // refresh value readout
    }

    ~DenoisePanel () override
    {
        stopTimer ();
        if (downloadThread != nullptr)
            downloadThread->stopThread (4000);
        mod.modelStatusChanged = nullptr;
    }

    void resized () override
    {
        auto b = getLocalBounds ().reduced (16, 12);

        // Status bar
        auto statusRow = b.removeFromTop (20);
        cpuLabel.setBounds (statusRow.removeFromRight (80));
        statusLabel.setBounds (statusRow);
        b.removeFromTop (6);

        // Buttons row
        auto btnRow = b.removeFromTop (28);
        modelSelector.setBounds (btnRow.removeFromLeft (180).reduced (0, 2));
        btnRow.removeFromLeft (8);
        browseBtn.setBounds (btnRow.removeFromLeft (80).reduced (0, 2));
        btnRow.removeFromLeft (8);
        listenBtn.setBounds (btnRow.removeFromLeft (150).reduced (0, 2));
        b.removeFromTop (16);

        // Strength knob
        const int kw = 80;
        strength.layout (b.removeFromLeft (kw).reduced (4), "Strength");
    }

    void timerCallback () override
    {
        if (downloadThread != nullptr && ! downloadThread->isThreadRunning ())
        {
            downloadThread.reset ();
        }

        strength.updateValue (mod.strength.load ());

        const float load = mod.getCpuLoad ();
        if (load > 0.f)
        {
            const int pct = juce::roundToInt (load * 100.f);
            cpuLabel.setText (juce::String (pct) + "% CPU", juce::dontSendNotification);
            const juce::Colour col = load > 1.f    ? juce::Colour (0xffff4444)
                                     : load > 0.7f ? juce::Colour (0xffffaa00)
                                                   : textLo ();
            cpuLabel.setColour (juce::Label::textColourId, col);
        }
        else
        {
            cpuLabel.setText ("", juce::dontSendNotification);
        }
    }

  private:
    juce::File getSharedModelsDir () const
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                       .getChildFile (".auclear")
                       .getChildFile ("models");
        if (! dir.exists ())
            dir.createDirectory ();
        return dir;
    }

    void populateModelSelector ()
    {
        modelSelector.clear (juce::dontSendNotification);
        modelOptions.clear ();

        struct TempDef { juce::String name; juce::String filename; juce::String url; };
        std::vector<TempDef> defaults = {
            { "Identity (Passthrough)", "identity.onnx", "https://raw.githubusercontent.com/caseypaite/AuClear/main/models/identity.onnx" },
            { "Noise Gate", "gate.onnx", "https://raw.githubusercontent.com/caseypaite/AuClear/main/models/gate.onnx" }
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
            if (fname != "identity.onnx" && fname != "gate.onnx")
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

        auto currentFile = mod.getModelFile ();
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
        listenBtn.setEnabled (false);
        
        statusLabel.setText ("Connecting to download " + opt.filename + "...", juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, textLo ());

        juce::Component::SafePointer<DenoisePanel> safeThis (this);

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
                    safeThis->listenBtn.setEnabled (true);

                    if (success)
                    {
                        safeThis->statusLabel.setText ("Download finished! Loading model...", juce::dontSendNotification);
                        safeThis->mod.setModelFile (opt.localFile);
                        safeThis->populateModelSelector ();
                        safeThis->updateStatus ();
                    }
                    else
                    {
                        safeThis->statusLabel.setText ("Download failed: " + error, juce::dontSendNotification);
                        safeThis->statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xffff4444));
                        safeThis->populateModelSelector ();
                    }
                }
            }
        );

        downloadThread->startThread ();
    }

    void updateStatus ()
    {
        const juce::String s = mod.getStatusString ();
        statusLabel.setText (s, juce::dontSendNotification);

        const bool ready = mod.isModelLoaded ();
        statusLabel.setColour (juce::Label::textColourId, ready ? accent () : textLo ());
        repaint ();
    }

    DenoiseModule& mod;
    KnobGroup strength;
    juce::TextButton browseBtn, listenBtn;
    juce::ComboBox modelSelector;
    juce::Label statusLabel, cpuLabel;

    std::unique_ptr<DownloadThread> downloadThread;

    std::vector<ModelOption> modelOptions;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DenoisePanel)
};
