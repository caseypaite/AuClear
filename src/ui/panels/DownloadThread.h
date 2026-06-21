#pragma once
#include <JuceHeader.h>
#include <functional>
#include <vector>

class DownloadThread : public juce::Thread
{
public:
    DownloadThread (const juce::String& name, const juce::String& urlStringParam, const juce::File& destFile,
                    std::function<void (float progress)> onProgressUpdate,
                    std::function<void (bool success, juce::String error)> onFinished)
        : juce::Thread ("DownloadThread_" + name),
          urlString (urlStringParam),
          destination (destFile),
          progressCallback (onProgressUpdate),
          finishedCallback (onFinished)
    {
    }

    ~DownloadThread() override
    {
        stopThread (4000);
    }

    void run() override
    {
        // Copy callbacks locally to avoid dependency on 'this' in async posts
        auto progressCb = progressCallback;
        auto finishedCb = finishedCallback;

        // Ensure parent directory exists
        destination.getParentDirectory().createDirectory();

        // Download to a temporary file first
        auto tempFile = destination.getSiblingFile (destination.getFileName() + ".tmp");
        if (tempFile.existsAsFile())
            tempFile.deleteFile();

        juce::URL url (urlString);
        auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                           .withConnectionTimeoutMs (15000)
                           .withResponseHeaders (nullptr);

        std::unique_ptr<juce::InputStream> stream (url.createInputStream (options));

        if (stream == nullptr)
        {
            juce::MessageManager::callAsync ([finishedCb] {
                finishedCb (false, "Could not open URL connection. Check internet/URL.");
            });
            return;
        }

        int64 totalLength = stream->getTotalLength();

        std::unique_ptr<juce::FileOutputStream> outStream (tempFile.createOutputStream());
        if (outStream == nullptr || outStream->failedToOpen())
        {
            juce::MessageManager::callAsync ([finishedCb] {
                finishedCb (false, "Failed to create destination file.");
            });
            return;
        }

        constexpr int bufferSize = 8192;
        std::vector<char> buffer (bufferSize);
        int64 bytesDownloaded = 0;

        while (! threadShouldExit() && ! stream->isExhausted())
        {
            int bytesRead = stream->read (buffer.data(), bufferSize);
            if (bytesRead < 0)
            {
                juce::MessageManager::callAsync ([finishedCb] {
                    finishedCb (false, "Error reading network stream.");
                });
                return;
            }
            if (bytesRead == 0)
            {
                juce::Thread::sleep (2);
                continue;
            }

            outStream->write (buffer.data(), (size_t)bytesRead);
            bytesDownloaded += bytesRead;

            if (totalLength > 0)
            {
                float progress = (float)bytesDownloaded / (float)totalLength;
                juce::MessageManager::callAsync ([progressCb, progress] {
                    progressCb (progress);
                });
            }
            else
            {
                // Guess or unknown
                juce::MessageManager::callAsync ([progressCb, bytesDownloaded] {
                    progressCb (- (float)bytesDownloaded);
                });
            }
        }

        outStream.reset(); // Close file before moving

        if (threadShouldExit())
        {
            tempFile.deleteFile();
            juce::MessageManager::callAsync ([finishedCb] {
                finishedCb (false, "Download cancelled.");
            });
            return;
        }

        if (destination.existsAsFile())
            destination.deleteFile();

        if (tempFile.moveFileTo (destination))
        {
            juce::MessageManager::callAsync ([finishedCb] {
                finishedCb (true, "");
            });
        }
        else
        {
            juce::MessageManager::callAsync ([finishedCb] {
                finishedCb (false, "Failed to move temporary file.");
            });
        }
    }

private:
    juce::String urlString;
    juce::File destination;
    std::function<void (float progress)> progressCallback;
    std::function<void (bool success, juce::String error)> finishedCallback;
};
