#pragma once
#include <JuceHeader.h>

/**
 * Hash-keyed persistent cache for offline processing artifacts.
 *
 * Cache keys are MD5 digests of (input path + modification time + job type + params XML).
 * The index is persisted as a ValueTree XML in cacheDir/index.xml so results survive restarts.
 *
 * All methods are thread-safe (guarded by an internal CriticalSection).
 */
class ArtifactCache
{
  public:
    explicit ArtifactCache (juce::File cacheDir);
    ~ArtifactCache ();

    /** True iff a cached result exists and all output files are present. */
    bool has (const juce::String& key) const;

    /** Returns cached output files for key, or empty if not cached / outputs missing. */
    juce::Array<juce::File> get (const juce::String& key) const;

    /** Store output files under key. Files must already exist. */
    void put (const juce::String& key, const juce::Array<juce::File>& outputs);

    /** Remove a single entry. */
    void invalidate (const juce::String& key);

    /** Remove entries whose output files no longer exist on disk. */
    void prune ();

    /** Build a deterministic MD5 cache key from job inputs. */
    static juce::String makeKey (const juce::File& inputFile,
                                 const juce::String& jobType,
                                 const juce::ValueTree& params);

    juce::File getCacheDir () const { return cacheDir; }
    int size () const;

  private:
    void loadIndex ();
    void saveIndex ();

    juce::Array<juce::File> getOutputsForEntry (const juce::ValueTree& entry) const;

    juce::File cacheDir;
    juce::File indexFile;
    juce::ValueTree index{"Cache"};
    mutable juce::CriticalSection lock;
};
