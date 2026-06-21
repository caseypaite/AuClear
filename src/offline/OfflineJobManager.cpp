#include "OfflineJobManager.h"
#include "DemucsSession.h"
#include "DemucsRunner.h"

// ─── JobTask ──────────────────────────────────────────────────────────────────

class OfflineJobManager::JobTask : public juce::ThreadPoolJob
{
  public:
    JobTask (OfflineJobManager& mgr, std::shared_ptr<OfflineJob> j)
        : juce::ThreadPoolJob ("OfflineJob:" + j->id.toString ())
        , manager (mgr)
        , job (std::move (j))
    {
    }

    JobStatus runJob () override
    {
        manager.runJob (*job);
        return JobStatus::jobHasFinished;
    }

  private:
    OfflineJobManager& manager;
    std::shared_ptr<OfflineJob> job;
};

// ─── OfflineJobManager ────────────────────────────────────────────────────────

juce::File OfflineJobManager::defaultCacheDir ()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("AuClear/OfflineCache");
}

int OfflineJobManager::defaultPoolThreads ()
{
    return juce::jlimit (1, 4, juce::SystemStats::getNumCpus () / 2);
}

OfflineJobManager::OfflineJobManager ()
    : pool (defaultPoolThreads ())
    , artifactCache (defaultCacheDir ())
{
}

OfflineJobManager::~OfflineJobManager ()
{
    cancelAll ();
    pool.removeAllJobs (true, 10000);
}

// ─── Public API ───────────────────────────────────────────────────────────────

juce::String OfflineJobManager::submit (std::shared_ptr<OfflineJob> job)
{
    jassert (juce::MessageManager::getInstance ()->isThisTheMessageThread ());

    const juce::String id = job->id.toString ();

    // Check artifact cache before queuing
    const juce::String cacheKey = ArtifactCache::makeKey (
        job->inputFile, juce::String (static_cast<int> (job->type)), job->params);

    if (artifactCache.has (cacheKey))
    {
        auto cached = artifactCache.get (cacheKey);
        if (!cached.isEmpty ())
        {
            job->state.store (JobState::Completed, std::memory_order_release);
            job->progress.store (1.0f, std::memory_order_relaxed);
            job->setStatusMessage ("Loaded from cache.");
            {
                juce::ScopedLock sl (jobsLock);
                jobs.push_back (job);
            }
            if (job->onDone)
            {
                auto fn = job->onDone;
                juce::MessageManager::callAsync ([fn] { fn (true, "Loaded from cache."); });
            }
            return id;
        }
    }

    {
        juce::ScopedLock sl (jobsLock);
        jobs.push_back (job);
    }
    pool.addJob (new JobTask (*this, std::move (job)), true /* deleteJobWhenDone */);
    return id;
}

void OfflineJobManager::cancel (const juce::String& jobId)
{
    juce::ScopedLock sl (jobsLock);
    for (auto& j : jobs)
        if (j->id.toString () == jobId)
            j->cancelRequested.store (true, std::memory_order_relaxed);
}

void OfflineJobManager::cancelAll ()
{
    juce::ScopedLock sl (jobsLock);
    for (auto& j : jobs)
        j->cancelRequested.store (true, std::memory_order_relaxed);
}

std::vector<std::shared_ptr<OfflineJob>> OfflineJobManager::getJobs () const
{
    juce::ScopedLock sl (jobsLock);
    return jobs;
}

void OfflineJobManager::clearFinished ()
{
    juce::ScopedLock sl (jobsLock);
    jobs.erase (
        std::remove_if (jobs.begin (), jobs.end (),
                        [] (const std::shared_ptr<OfflineJob>& j)
                        {
                            const auto s = j->state.load (std::memory_order_acquire);
                            return s == JobState::Completed || s == JobState::Failed
                                   || s == JobState::Cancelled;
                        }),
        jobs.end ());
}

// ─── Job dispatch ─────────────────────────────────────────────────────────────

void OfflineJobManager::runJob (OfflineJob& job)
{
    job.state.store (JobState::Running, std::memory_order_release);

    switch (job.type)
    {
    case JobType::DspProcess:
        runDspProcess (job);
        break;

    case JobType::DemucsStems:
        runDemucsStems (job);
        break;

    case JobType::DeClip:
    case JobType::DeCrackle:
        job.state.store (JobState::Failed, std::memory_order_release);
        job.setStatusMessage ("Not yet implemented.");
        if (job.onDone)
        {
            auto fn = job.onDone;
            juce::MessageManager::callAsync ([fn] { fn (false, "Not yet implemented."); });
        }
        break;
    }
}

// ─── DspProcess runner ────────────────────────────────────────────────────────

namespace
{
static const juce::StringArray kVideoExtensions{"mp4", "mkv", "mov", "avi", "m4v", "webm",
                                                "flv", "wmv", "ts",  "mxf", "mts", "m2ts"};

bool isVideoFile (const juce::File& f) noexcept
{
    return kVideoExtensions.contains (
        f.getFileExtension ().trimCharactersAtStart (".").toLowerCase ());
}

juce::String findFfmpeg ()
{
    juce::ChildProcess probe;
    juce::StringArray args{"ffmpeg", "-version"};
    if (probe.start (args))
    {
        probe.waitForProcessToFinish (3000);
        if (probe.getExitCode () == 0)
            return "ffmpeg";
    }
#if !JUCE_WINDOWS
    for (const char* candidate : {"/usr/bin/ffmpeg", "/usr/local/bin/ffmpeg",
                                  "/opt/homebrew/bin/ffmpeg", "/opt/local/bin/ffmpeg"})
        if (juce::File (candidate).existsAsFile ())
            return candidate;
#endif
    return {};
}

constexpr int kChunk = 512;
constexpr juce::uint32 kProgressMs = 80;
} // namespace

void OfflineJobManager::runDspProcess (OfflineJob& job)
{
    jassert (job.rack != nullptr);

    // ── Helpers ───────────────────────────────────────────────────────────────
    auto postProgress = [&] (float p, const juce::String& msg)
    {
        job.progress.store (p, std::memory_order_relaxed);
        job.setStatusMessage (msg);
        if (job.onProgress)
        {
            auto fn = job.onProgress;
            juce::MessageManager::callAsync ([fn, p, msg] { fn (p, msg); });
        }
    };

    auto cancelled = [&]
    { return job.cancelRequested.load (std::memory_order_relaxed); };

    auto postDone = [&] (bool ok, const juce::String& msg)
    {
        job.state.store (ok ? JobState::Completed : JobState::Failed, std::memory_order_release);
        job.setStatusMessage (msg);
        if (job.onDone)
        {
            auto fn = job.onDone;
            juce::MessageManager::callAsync ([fn, ok, msg] { fn (ok, msg); });
        }
    };

    // ── Video: extract audio ──────────────────────────────────────────────────
    const bool video = isVideoFile (job.inputFile);
    juce::TemporaryFile tmpExtracted (".wav");
    juce::TemporaryFile tmpProcessed (".wav");

    const juce::File audioIn = video ? tmpExtracted.getFile () : job.inputFile;
    const juce::File audioOut = video ? tmpProcessed.getFile () : job.outputFile;

    if (video)
    {
        const juce::String ffmpeg = findFfmpeg ();
        if (ffmpeg.isEmpty ())
        {
            postDone (false, "FFmpeg not found — install FFmpeg and ensure it is on your PATH.");
            return;
        }

        postProgress (0.f, "Extracting audio from video...");

        juce::StringArray args;
        args.add (ffmpeg);  args.add ("-y");
        args.add ("-i");    args.add (job.inputFile.getFullPathName ());
        args.add ("-vn");
        args.add ("-acodec"); args.add ("pcm_f32le");
        args.add ("-ar");   args.add ("48000");
        args.add ("-ac");   args.add ("2");
        args.add ("-f");    args.add ("wav");
        args.add (tmpExtracted.getFile ().getFullPathName ());

        juce::ChildProcess proc;
        if (!proc.start (args, juce::ChildProcess::wantStdErr))
        {
            postDone (false, "Could not launch FFmpeg.");
            return;
        }
        while (proc.isRunning ())
        {
            if (cancelled ()) { proc.kill (); job.state.store (JobState::Cancelled, std::memory_order_release); return; }
            juce::Thread::sleep (100);
        }
        if (proc.getExitCode () != 0)
        {
            postDone (false, "FFmpeg failed to extract audio from video.");
            return;
        }
    }

    // ── Process audio through the rack ────────────────────────────────────────
    juce::AudioFormatManager fmt;
    fmt.registerBasicFormats ();
    fmt.registerFormat (new juce::MP3AudioFormat (), true);

    auto reader = std::unique_ptr<juce::AudioFormatReader> (fmt.createReaderFor (audioIn));
    if (!reader)
    {
        postDone (false, "Could not open audio file: " + audioIn.getFileName ());
        return;
    }

    const double sr = reader->sampleRate;
    const int nch = juce::jlimit (1, 2, (int)reader->numChannels);
    const juce::int64 totalSamples = reader->lengthInSamples;

    audioOut.deleteFile ();
    auto outStream = std::make_unique<juce::FileOutputStream> (audioOut);
    if (!outStream->openedOk ())
    {
        postDone (false, "Could not create output file: " + audioOut.getFileName ());
        return;
    }

    juce::WavAudioFormat wavFmt;
    juce::StringPairArray noMeta;
    JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wdeprecated-declarations")
    JUCE_BEGIN_IGNORE_WARNINGS_MSVC (4996)
    auto writer = std::unique_ptr<juce::AudioFormatWriter> (
        wavFmt.createWriterFor (outStream.release (), sr, (unsigned)nch, 24, noMeta, 0));
    JUCE_END_IGNORE_WARNINGS_MSVC
    JUCE_END_IGNORE_WARNINGS_GCC_LIKE

    if (!writer)
    {
        postDone (false, "Could not create WAV writer.");
        return;
    }

    juce::AudioBuffer<float> buffer (nch, kChunk);
    juce::MidiBuffer midi;

    const int lat = job.rackLatency;
    const juce::int64 totalToProcess = totalSamples + (juce::int64)lat;
    juce::int64 processed = 0, written = 0;
    int skipped = 0;
    juce::uint32 lastProgressMs = 0;

    while (written < totalSamples && !cancelled ())
    {
        const int chunk = (int)juce::jmin ((juce::int64)kChunk, totalToProcess - processed);
        if (chunk <= 0)
            break;

        buffer.setSize (nch, chunk, false, true, true);

        const int fromFile = (int)juce::jmin ((juce::int64)chunk, totalSamples - processed);
        if (fromFile > 0)
            reader->read (&buffer, 0, fromFile, processed, true, nch > 1);
        if (fromFile < chunk)
            for (int ch = 0; ch < nch; ++ch)
                buffer.clear (ch, fromFile, chunk - fromFile);

        processed += chunk;
        job.rack->processBlock (buffer, midi);

        // Compensate for rack latency by discarding initial output samples
        int outStart = 0, outCount = chunk;
        if (skipped < lat)
        {
            const int skip = std::min (chunk, lat - skipped);
            skipped += skip;
            outStart = skip;
            outCount -= skip;
        }

        const int toWrite = (int)juce::jmin ((juce::int64)outCount, totalSamples - written);
        if (toWrite > 0)
        {
            writer->writeFromAudioSampleBuffer (buffer, outStart, toWrite);
            written += toWrite;
        }

        const juce::uint32 now = juce::Time::getMillisecondCounter ();
        if (now - lastProgressMs >= kProgressMs)
        {
            lastProgressMs = now;
            const float p = (float)written / (float)juce::jmax ((juce::int64)1, totalSamples);
            postProgress (p, "Processing " + juce::String (juce::roundToInt (p * 100)) + "%");
        }
    }

    writer.reset ();

    if (cancelled ())
    {
        job.state.store (JobState::Cancelled, std::memory_order_release);
        audioOut.deleteFile ();
        return;
    }

    if (written < totalSamples)
    {
        postDone (false, "Processing incomplete.");
        return;
    }

    // ── Video: remux processed audio back into container ──────────────────────
    if (video)
    {
        postProgress (0.95f, "Remuxing video...");

        const juce::String ffmpeg = findFfmpeg ();
        juce::StringArray args;
        args.add (ffmpeg);  args.add ("-y");
        args.add ("-i");    args.add (job.inputFile.getFullPathName ());
        args.add ("-i");    args.add (tmpProcessed.getFile ().getFullPathName ());
        args.add ("-c:v");  args.add ("copy");
        args.add ("-c:a");  args.add ("aac");
        args.add ("-b:a");  args.add ("320k");
        args.add ("-map");  args.add ("0:v:0");
        args.add ("-map");  args.add ("1:a:0");
        args.add (job.outputFile.getFullPathName ());

        juce::ChildProcess proc;
        if (!proc.start (args, juce::ChildProcess::wantStdErr))
        {
            postDone (false, "Could not launch FFmpeg for remux.");
            return;
        }
        while (proc.isRunning ())
        {
            if (cancelled ()) { proc.kill (); job.state.store (JobState::Cancelled, std::memory_order_release); return; }
            juce::Thread::sleep (100);
        }
        if (proc.getExitCode () != 0)
        {
            postDone (false, "FFmpeg remux failed.");
            return;
        }
    }

    // ── Store result in artifact cache ────────────────────────────────────────
    const juce::String cacheKey = ArtifactCache::makeKey (
        job.inputFile, juce::String (static_cast<int> (job.type)), job.params);
    juce::Array<juce::File> outputs;
    outputs.add (job.outputFile);
    artifactCache.put (cacheKey, outputs);

    postDone (true, "Done.");
}

// ─── DemucsStems runner ───────────────────────────────────────────────────────

void OfflineJobManager::runDemucsStems (OfflineJob& job)
{
    auto postProgress = [&] (float p, const juce::String& msg)
    {
        job.progress.store (p, std::memory_order_relaxed);
        job.setStatusMessage (msg);
        if (job.onProgress)
        {
            auto fn = job.onProgress;
            juce::MessageManager::callAsync ([fn, p, msg] { fn (p, msg); });
        }
    };

    auto postDone = [&] (bool ok, const juce::String& msg)
    {
        job.state.store (ok ? JobState::Completed : JobState::Failed, std::memory_order_release);
        job.setStatusMessage (msg);
        if (job.onDone)
        {
            auto fn = job.onDone;
            juce::MessageManager::callAsync ([fn, ok, msg] { fn (ok, msg); });
        }
    };

    // Retrieve model path from job params
    const juce::String modelPath = job.params.getProperty ("modelPath", "").toString ();
    if (modelPath.isEmpty ())
    {
        postDone (false, "No model path specified in job params (set 'modelPath').");
        return;
    }

    const juce::File modelFile (modelPath);
    if (! modelFile.existsAsFile ())
    {
        postDone (false, "Demucs model not found: " + modelPath);
        return;
    }

    postProgress (0.0f, "Loading Demucs model...");
    DemucsSession session;
    if (! session.loadModel (modelPath.toStdString ()))
    {
        postDone (false, "Failed to load model: " + juce::String (session.getLastError ()));
        return;
    }

    if (job.cancelRequested.load (std::memory_order_relaxed))
    {
        job.state.store (JobState::Cancelled, std::memory_order_release);
        return;
    }

    job.outputDir.createDirectory ();

    DemucsRunner runner (session);
    DemucsRunner::Callbacks cb;
    cb.shouldCancel = [&] { return job.cancelRequested.load (std::memory_order_relaxed); };
    cb.onProgress   = [&] (float p, juce::String msg) { postProgress (p, msg); };

    const auto outputs = runner.run (job.inputFile, job.outputDir, cb);

    if (job.cancelRequested.load (std::memory_order_relaxed))
    {
        job.state.store (JobState::Cancelled, std::memory_order_release);
        return;
    }

    if (outputs.isEmpty ())
    {
        postDone (false, "Stem separation failed: " + runner.getLastError ());
        return;
    }

    // Cache the results
    const juce::String cacheKey = ArtifactCache::makeKey (
        job.inputFile, juce::String (static_cast<int> (job.type)), job.params);
    artifactCache.put (cacheKey, outputs);

    postDone (true, "Separated into " + juce::String (outputs.size ()) + " stems.");
}
