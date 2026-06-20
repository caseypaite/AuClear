#include "FileProcessor.h"
#include "../engine/ModuleFactory.h"

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static const juce::StringArray kVideoExtensions{"mp4", "mkv", "mov", "avi", "m4v", "webm",
                                                "flv", "wmv", "ts",  "mxf", "mts", "m2ts"};

bool FileProcessor::isVideoFile (const juce::File& f) noexcept
{
    return kVideoExtensions.contains (
        f.getFileExtension ().trimCharactersAtStart (".").toLowerCase ());
}

juce::File FileProcessor::suggestOutput (const juce::File& input)
{
    const bool video = isVideoFile (input);
    const juce::String stem = input.getFileNameWithoutExtension () + "_processed";
    const juce::String ext = video ? input.getFileExtension () : ".wav";
    return input.getParentDirectory ().getChildFile (stem + ext);
}

juce::String FileProcessor::findFfmpeg ()
{
    // Quick check via PATH
    juce::ChildProcess probe;
    juce::StringArray testArgs;
    testArgs.add ("ffmpeg");
    testArgs.add ("-version");
    if (probe.start (testArgs))
    {
        probe.waitForProcessToFinish (3000);
        if (probe.getExitCode () == 0)
            return "ffmpeg";
    }
#if !JUCE_WINDOWS
    for (auto* candidate : {"/usr/bin/ffmpeg", "/usr/local/bin/ffmpeg", "/opt/homebrew/bin/ffmpeg",
                            "/opt/local/bin/ffmpeg"})
        if (juce::File (candidate).existsAsFile ())
            return candidate;
#endif
    return {};
}

// ─────────────────────────────────────────────────────────────────────────────
// FileProcessor
// ─────────────────────────────────────────────────────────────────────────────

FileProcessor::FileProcessor () : juce::Thread ("AuClear FileProcessor") {}

FileProcessor::~FileProcessor ()
{
    cancel ();
    stopThread (5000);
}

void FileProcessor::prepareLocalRack (const juce::ValueTree& rackState, double sr, int nch)
{
    jassert (juce::MessageManager::getInstance ()->isThisTheMessageThread ());

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sr;
    spec.maximumBlockSize = (juce::uint32)kChunkSize;
    spec.numChannels = (juce::uint32)nch;

    localRack.prepare (spec);
    localRack.setState (rackState, makeModule);

    preparedLatency = 0;
    for (int i = 0; i < localRack.numModules (); ++i)
        if (auto* m = localRack.getModule (i))
            if (!m->bypassed.load ())
                preparedLatency += m->latencySamples ();
}

void FileProcessor::startJob (juce::File input, juce::File output, ProgressFn onProgress,
                              DoneFn onDone)
{
    jassert (!isThreadRunning ());
    jobInput = std::move (input);
    jobOutput = std::move (output);
    progressFn = std::move (onProgress);
    doneFn = std::move (onDone);
    cancelFlag.store (false, std::memory_order_relaxed);
    lastProgressMs = 0;
    startThread ();
}

void FileProcessor::cancel ()
{
    cancelFlag.store (true, std::memory_order_relaxed);
    signalThreadShouldExit ();
}

// ─────────────────────────────────────────────────────────────────────────────
// Background thread entry
// ─────────────────────────────────────────────────────────────────────────────

void FileProcessor::run ()
{
    const bool video = isVideoFile (jobInput);
    const bool ok =
        video ? processVideoFile (jobInput, jobOutput) : processAudioFile (jobInput, jobOutput);

    if (!ok && !cancelFlag.load () && !threadShouldExit ())
        postDone (false, "Processing failed — check that the file is valid.");
    else if (ok)
        postDone (true, "Done.");
}

// ─────────────────────────────────────────────────────────────────────────────
// Audio file processing
// ─────────────────────────────────────────────────────────────────────────────

bool FileProcessor::processAudioFile (juce::File input, juce::File output)
{
    juce::AudioFormatManager fmt;
    fmt.registerBasicFormats ();

    auto reader = std::unique_ptr<juce::AudioFormatReader> (fmt.createReaderFor (input));
    if (!reader)
    {
        postDone (false, "Could not open audio file: " + input.getFileName ());
        return false;
    }

    const double sr = reader->sampleRate;
    const int nch = juce::jlimit (1, 2, (int)reader->numChannels);
    const juce::int64 totalSamples = reader->lengthInSamples;

    // Create output WAV writer
    output.deleteFile ();
    auto outStream = std::make_unique<juce::FileOutputStream> (output);
    if (!outStream->openedOk ())
    {
        postDone (false, "Could not create output file: " + output.getFileName ());
        return false;
    }

    juce::WavAudioFormat wavFmt;
    juce::StringPairArray noMetadata;
    JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wdeprecated-declarations")
    JUCE_BEGIN_IGNORE_WARNINGS_MSVC (4996)
    auto writer = std::unique_ptr<juce::AudioFormatWriter> (
        wavFmt.createWriterFor (outStream.release (), sr, (unsigned)nch, 24, noMetadata, 0));
    JUCE_END_IGNORE_WARNINGS_MSVC
    JUCE_END_IGNORE_WARNINGS_GCC_LIKE
    if (!writer)
    {
        postDone (false, "Could not create WAV writer.");
        return false;
    }

    juce::AudioBuffer<float> buffer (nch, kChunkSize);
    juce::MidiBuffer midi;

    const juce::int64 totalToProcess = totalSamples + (juce::int64)preparedLatency;
    juce::int64 processed = 0;
    juce::int64 written = 0;
    int skipped = 0;

    while (written < totalSamples && !threadShouldExit () && !cancelFlag.load ())
    {
        const int chunkSize = (int)juce::jmin ((juce::int64)kChunkSize, totalToProcess - processed);
        if (chunkSize <= 0)
            break;

        buffer.setSize (nch, chunkSize, false, true, true);

        // Read from file while samples remain; pad with silence to flush latency
        const int fromFile = (int)juce::jmin ((juce::int64)chunkSize, totalSamples - processed);
        if (fromFile > 0)
            reader->read (&buffer, 0, fromFile, processed, true, nch > 1);
        if (fromFile < chunkSize)
            for (int ch = 0; ch < nch; ++ch)
                buffer.clear (ch, fromFile, chunkSize - fromFile);

        processed += chunkSize;

        localRack.processBlock (buffer, midi);

        // Skip initial output samples equal to the pipeline latency
        int outStart = 0;
        int outCount = chunkSize;
        if (skipped < preparedLatency)
        {
            const int skip = std::min (chunkSize, preparedLatency - skipped);
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
        if (now - lastProgressMs >= kProgressIntervalMs)
        {
            lastProgressMs = now;
            const float p = (float)written / (float)juce::jmax ((juce::int64)1, totalSamples);
            postProgress (p, "Processing " + juce::String (juce::roundToInt (p * 100)) + "%");
        }
    }

    // Flush writer before we check result
    writer.reset ();
    return written >= totalSamples;
}

// ─────────────────────────────────────────────────────────────────────────────
// Video file processing (requires FFmpeg)
// ─────────────────────────────────────────────────────────────────────────────

bool FileProcessor::processVideoFile (juce::File input, juce::File output)
{
    const juce::String ffmpeg = findFfmpeg ();
    if (ffmpeg.isEmpty ())
    {
        postDone (false,
                  "FFmpeg not found. Install FFmpeg and ensure it is on your PATH to process video "
                  "files.");
        return false;
    }

    // Use TemporaryFile for the extracted audio and the processed audio
    juce::TemporaryFile extractedAudio (".wav");
    juce::TemporaryFile processedAudio (".wav");

    // Step 1: extract audio from video to a flat WAV
    postProgress (0.0f, "Extracting audio from video...");
    {
        juce::StringArray args;
        args.add (ffmpeg);
        args.add ("-y");
        args.add ("-i");
        args.add (input.getFullPathName ());
        args.add ("-vn");
        args.add ("-acodec");
        args.add ("pcm_f32le");
        args.add ("-ar");
        args.add ("48000");
        args.add ("-ac");
        args.add ("2");
        args.add ("-f");
        args.add ("wav");
        args.add (extractedAudio.getFile ().getFullPathName ());
        if (!runFfmpeg (args))
        {
            postDone (false, "FFmpeg failed to extract audio from the video.");
            return false;
        }
    }

    if (threadShouldExit () || cancelFlag.load ())
        return false;

    // Step 2: process extracted audio through the rack
    if (!processAudioFile (extractedAudio.getFile (), processedAudio.getFile ()))
        return false;

    if (threadShouldExit () || cancelFlag.load ())
        return false;

    // Step 3: remux processed audio back into the video container (stream-copy video)
    postProgress (0.95f, "Remuxing video...");
    {
        juce::StringArray args;
        args.add (ffmpeg);
        args.add ("-y");
        args.add ("-i");
        args.add (input.getFullPathName ());
        args.add ("-i");
        args.add (processedAudio.getFile ().getFullPathName ());
        args.add ("-c:v");
        args.add ("copy");
        args.add ("-c:a");
        args.add ("aac");
        args.add ("-b:a");
        args.add ("320k");
        args.add ("-map");
        args.add ("0:v:0");
        args.add ("-map");
        args.add ("1:a:0");
        args.add (output.getFullPathName ());
        if (!runFfmpeg (args))
        {
            postDone (false, "FFmpeg failed to remux the processed audio into the video.");
            return false;
        }
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

bool FileProcessor::runFfmpeg (const juce::StringArray& args)
{
    juce::ChildProcess proc;
    if (!proc.start (args, juce::ChildProcess::wantStdErr))
        return false;

    while (proc.isRunning ())
    {
        if (threadShouldExit () || cancelFlag.load ())
        {
            proc.kill ();
            return false;
        }
        juce::Thread::sleep (100);
    }
    return proc.getExitCode () == 0;
}

void FileProcessor::postProgress (float p, const juce::String& msg)
{
    if (progressFn)
    {
        auto fn = progressFn;
        juce::MessageManager::callAsync ([fn, p, msg] { fn (p, msg); });
    }
}

void FileProcessor::postDone (bool ok, const juce::String& msg)
{
    if (doneFn)
    {
        auto fn = doneFn;
        juce::MessageManager::callAsync ([fn, ok, msg] { fn (ok, msg); });
    }
}
