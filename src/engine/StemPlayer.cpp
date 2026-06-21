#include "StemPlayer.h"

//==============================================================================
bool StemPlayer::loadStems (const std::array<juce::File, 4>& files,
                             juce::AudioFormatManager& fmt)
{
    // Open all four readers before touching shared state so a bad file doesn't
    // leave us half-initialised.
    std::array<std::unique_ptr<juce::AudioFormatReader>, 4>       newReaders;
    std::array<std::unique_ptr<juce::AudioFormatReaderSource>, 4> newSources;

    for (int i = 0; i < 4; ++i)
    {
        newReaders[i].reset (fmt.createReaderFor (files[i]));
        if (! newReaders[i])
            return false;

        newSources[i] = std::make_unique<juce::AudioFormatReaderSource> (
            newReaders[i].get (), false /* does not own reader */);

        if (preparedSr > 0.0 && preparedBlock > 0)
            newSources[i]->prepareToPlay (preparedBlock, preparedSr);

        newSources[i]->setLooping (false);
    }

    {
        juce::ScopedLock sl (lock);
        for (int i = 0; i < 4; ++i)
        {
            sources[i] = std::move (newSources[i]);
            readers[i] = std::move (newReaders[i]);
        }
        displayPos.store (0, std::memory_order_relaxed);
        active.store (true, std::memory_order_release);
    }
    return true;
}

void StemPlayer::unloadStems ()
{
    {
        juce::ScopedLock sl (lock);
        active.store (false, std::memory_order_release);
        for (auto& s : sources) s.reset ();
        for (auto& r : readers) r.reset ();
        displayPos.store (0, std::memory_order_relaxed);
    }
}

void StemPlayer::prepare (int samplesPerBlock, double sampleRate)
{
    preparedSr    = sampleRate;
    preparedBlock = samplesPerBlock;

    // Over-allocate so audio-thread calls with smaller nSamples never reallocate.
    stemBuf.setSize (2, samplesPerBlock, false, true, false);

    juce::ScopedLock sl (lock);
    for (auto& s : sources)
        if (s) s->prepareToPlay (samplesPerBlock, sampleRate);
}

void StemPlayer::releaseResources ()
{
    juce::ScopedLock sl (lock);
    active.store (false, std::memory_order_release);
    for (auto& s : sources) { if (s) s->releaseResources (); s.reset (); }
    for (auto& r : readers) r.reset ();
    stemBuf.setSize (0, 0);
}

//==============================================================================
void StemPlayer::syncPosition (double transportPositionSeconds) noexcept
{
    if (! active.load (std::memory_order_relaxed))
        return;

    juce::ScopedTryLock tryLock (lock);
    if (! tryLock.isLocked () || ! sources[0])
        return;

    const double stemPosSec =
        (double)sources[0]->getNextReadPosition () / juce::jmax (1.0, preparedSr);

    if (std::abs (stemPosSec - transportPositionSeconds) > kSyncThresholdSec)
    {
        const auto targetSample =
            (juce::int64)(transportPositionSeconds * preparedSr);
        for (auto& s : sources)
            if (s) s->setNextReadPosition (juce::jmax ((juce::int64)0, targetSample));
    }
}

double StemPlayer::getPositionSeconds () const noexcept
{
    if (preparedSr <= 0.0) return 0.0;
    return (double)displayPos.load (std::memory_order_relaxed) / preparedSr;
}

//==============================================================================
void StemPlayer::fillNextAudioBlock (juce::AudioBuffer<float>& buffer,
                                      const juce::AudioBuffer<float>& dry,
                                      int numSamples) noexcept
{
    buffer.clear (0, numSamples);

    juce::ScopedTryLock tryLock (lock);
    if (! tryLock.isLocked () || ! active.load (std::memory_order_relaxed))
        return;

    if (! isPlaying.load (std::memory_order_relaxed))
    {
        // Paused: blend dry at dryMix level (transport also outputs silence when
        // paused, so this will be silent unless the user has a device input path).
        const float dm = dryMix.load (std::memory_order_relaxed);
        if (dm > 0.0f)
        {
            const int nCh = juce::jmin (buffer.getNumChannels (), dry.getNumChannels ());
            for (int ch = 0; ch < nCh; ++ch)
                buffer.addFrom (ch, 0, dry, ch, 0, numSamples, dm);
        }
        return;
    }

    const int nOutCh = buffer.getNumChannels ();

    // Check if any stem is soloed — determines which stems contribute.
    bool anySoloed = false;
    for (const auto& st : stems)
        anySoloed |= st.soloed.load (std::memory_order_relaxed);

    for (int i = 0; i < 4; ++i)
    {
        if (! sources[i]) continue;

        // Always advance the reader to keep all stems in lock-step.
        stemBuf.clear (0, numSamples);
        juce::AudioSourceChannelInfo info (&stemBuf, 0, numSamples);
        sources[i]->getNextAudioBlock (info);

        const bool muted  = stems[i].muted.load (std::memory_order_relaxed);
        const bool soloed = stems[i].soloed.load (std::memory_order_relaxed);
        if (muted || (anySoloed && ! soloed))
            continue;

        const float g = stems[i].gain.load (std::memory_order_relaxed);
        const float p = stems[i].pan.load  (std::memory_order_relaxed);

        // Linear pan law: pan=-1 → full left, pan=+1 → full right.
        const float gL = g * (p <= 0.f ? 1.f : 1.f - p);
        const float gR = g * (p >= 0.f ? 1.f : 1.f + p);

        const int srcCh0 = 0;
        const int srcCh1 = juce::jmin (1, stemBuf.getNumChannels () - 1);

        buffer.addFrom (0, 0, stemBuf, srcCh0, 0, numSamples, gL);
        if (nOutCh > 1)
            buffer.addFrom (1, 0, stemBuf, srcCh1, 0, numSamples, gR);
    }

    // Dry blend: out = out * (1 - dryMix) + dry * dryMix
    const float dm = dryMix.load (std::memory_order_relaxed);
    if (dm > 0.0f)
    {
        const float wetScale = 1.0f - dm;
        const int   nCh      = juce::jmin (buffer.getNumChannels (), dry.getNumChannels ());
        for (int ch = 0; ch < nCh; ++ch)
        {
            buffer.applyGain (ch, 0, numSamples, wetScale);
            buffer.addFrom (ch, 0, dry, ch, 0, numSamples, dm);
        }
    }

    // Update display position from the first reader (all are in lock-step).
    if (sources[0])
        displayPos.store (sources[0]->getNextReadPosition (), std::memory_order_relaxed);
}
