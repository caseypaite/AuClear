#pragma once
#include <JuceHeader.h>
#include <atomic>

// Per-stem mix parameters.
// Atomic members are written on the message thread (UI controls) and read on
// the audio thread (StemPlayer::fillNextAudioBlock). stemFile is message-thread
// only (display in StemChannelStrip::paint).
struct StemState
{
    std::atomic<float> gain{1.0f};   // 0–2 linear
    std::atomic<float> pan{0.0f};    // -1 to +1  (negative = left)
    std::atomic<bool>  muted{false};
    std::atomic<bool>  soloed{false};
    juce::File         stemFile;     // message thread only

    StemState () = default;
    StemState (const StemState&) = delete;
    StemState& operator= (const StemState&) = delete;
};
