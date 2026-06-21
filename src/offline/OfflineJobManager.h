#pragma once
#include "OfflineJob.h"
#include "ArtifactCache.h"
#include <JuceHeader.h>
#include <memory>
#include <vector>

/**
 * Queue and run offline AI/DSP processing jobs on a thread pool.
 *
 * Usage (message thread):
 *   1. Create an OfflineJob, fill in inputFile/outputFile/type/params.
 *   2. For DspProcess: also build + prepare `job->rack` on the message thread.
 *   3. Call manager.submit(std::move(job)).
 *   4. The manager checks the artifact cache — if hit, onDone fires immediately
 *      (async, message thread); otherwise a pool thread picks up the work.
 *
 * Thread pool size defaults to half the logical CPU count (min 1, max 4).
 * The cache persists to disk and survives restarts.
 */
class OfflineJobManager
{
  public:
    OfflineJobManager ();
    ~OfflineJobManager ();

    /** Submit a job. Returns the job's UUID string. Message-thread only. */
    juce::String submit (std::shared_ptr<OfflineJob> job);

    /** Request cancellation of a job by UUID. */
    void cancel (const juce::String& jobId);

    /** Cancel all pending/running jobs. */
    void cancelAll ();

    /** All known jobs in submission order. */
    std::vector<std::shared_ptr<OfflineJob>> getJobs () const;

    /** Remove completed / failed / cancelled jobs from the list. */
    void clearFinished ();

    ArtifactCache& cache () { return artifactCache; }
    const ArtifactCache& cache () const { return artifactCache; }

  private:
    class JobTask;

    void runJob (OfflineJob& job);
    void runDspProcess (OfflineJob& job);
    void runDemucsStems (OfflineJob& job);

    static juce::File defaultCacheDir ();
    static int defaultPoolThreads ();

    juce::ThreadPool pool;
    ArtifactCache artifactCache;

    mutable juce::CriticalSection jobsLock;
    std::vector<std::shared_ptr<OfflineJob>> jobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OfflineJobManager)
};
