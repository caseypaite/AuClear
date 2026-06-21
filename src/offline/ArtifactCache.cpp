#include "ArtifactCache.h"

ArtifactCache::ArtifactCache (juce::File dir) : cacheDir (std::move (dir)), indexFile (cacheDir.getChildFile ("index.xml"))
{
    cacheDir.createDirectory ();
    loadIndex ();
}

ArtifactCache::~ArtifactCache ()
{
    saveIndex ();
}

// ─────────────────────────────────────────────────────────────────────────────

bool ArtifactCache::has (const juce::String& key) const
{
    juce::ScopedLock sl (lock);
    auto entry = index.getChildWithProperty ("key", key);
    if (!entry.isValid ())
        return false;
    for (const auto& f : getOutputsForEntry (entry))
        if (!f.existsAsFile ())
            return false;
    return entry.getNumProperties () > 1; // has at least "key" + "outputs"
}

juce::Array<juce::File> ArtifactCache::get (const juce::String& key) const
{
    juce::ScopedLock sl (lock);
    auto entry = index.getChildWithProperty ("key", key);
    if (!entry.isValid ())
        return {};

    juce::Array<juce::File> result;
    for (const auto& f : getOutputsForEntry (entry))
        if (f.existsAsFile ())
            result.add (f);
    return result;
}

void ArtifactCache::put (const juce::String& key, const juce::Array<juce::File>& outputs)
{
    juce::StringArray paths;
    for (const auto& f : outputs)
        paths.add (f.getFullPathName ());

    juce::ScopedLock sl (lock);
    // Replace existing entry if present
    auto existing = index.getChildWithProperty ("key", key);
    if (existing.isValid ())
        index.removeChild (existing, nullptr);

    juce::ValueTree entry ("Entry");
    entry.setProperty ("key", key, nullptr);
    entry.setProperty ("outputs", paths.joinIntoString ("|"), nullptr);
    entry.setProperty ("ts", juce::Time::getCurrentTime ().toMilliseconds (), nullptr);
    index.appendChild (entry, nullptr);
    saveIndex ();
}

void ArtifactCache::invalidate (const juce::String& key)
{
    juce::ScopedLock sl (lock);
    auto entry = index.getChildWithProperty ("key", key);
    if (entry.isValid ())
    {
        index.removeChild (entry, nullptr);
        saveIndex ();
    }
}

void ArtifactCache::prune ()
{
    juce::ScopedLock sl (lock);
    bool changed = false;
    for (int i = index.getNumChildren () - 1; i >= 0; --i)
    {
        auto entry = index.getChild (i);
        bool allExist = true;
        for (const auto& f : getOutputsForEntry (entry))
        {
            if (!f.existsAsFile ())
            {
                allExist = false;
                break;
            }
        }
        if (!allExist)
        {
            index.removeChild (entry, nullptr);
            changed = true;
        }
    }
    if (changed)
        saveIndex ();
}

juce::String ArtifactCache::makeKey (const juce::File& inputFile,
                                     const juce::String& jobType,
                                     const juce::ValueTree& params)
{
    juce::String raw;
    raw << inputFile.getFullPathName () << "|"
        << inputFile.getLastModificationTime ().toMilliseconds () << "|" << jobType << "|"
        << params.toXmlString ();
    return juce::String::toHexString (raw.hashCode64 ());
}

int ArtifactCache::size () const
{
    juce::ScopedLock sl (lock);
    return index.getNumChildren ();
}

// ─────────────────────────────────────────────────────────────────────────────

void ArtifactCache::loadIndex ()
{
    if (!indexFile.existsAsFile ())
        return;
    if (auto xml = juce::XmlDocument::parse (indexFile))
    {
        auto vt = juce::ValueTree::fromXml (*xml);
        if (vt.hasType ("Cache"))
            index = std::move (vt);
    }
}

void ArtifactCache::saveIndex ()
{
    if (auto xml = index.createXml ())
        xml->writeTo (indexFile, juce::XmlElement::TextFormat{});
}

juce::Array<juce::File> ArtifactCache::getOutputsForEntry (const juce::ValueTree& entry) const
{
    juce::Array<juce::File> files;
    for (const auto& path : juce::StringArray::fromTokens (entry.getProperty ("outputs").toString (), "|", ""))
        if (path.isNotEmpty ())
            files.add (juce::File (path));
    return files;
}
