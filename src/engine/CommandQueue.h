#pragma once
#include <JuceHeader.h>
#include <array>
#include <cstdint>

class RackModule;

struct RackCommand
{
    enum class Type : uint8_t
    {
        Insert,
        Remove,
        Move,
        None
    };

    Type type{Type::None};
    int indexA{-1};              // insert-at / move-from
    int indexB{-1};              // move-to
    RackModule* module{nullptr}; // Insert: audio thread takes it; Remove: pointer to find
};

/**
 * Lock-free SPSC queue: message thread produces, audio thread consumes.
 * Capacity is intentionally generous — structural rack edits are rare.
 */
class CommandQueue
{
  public:
    static constexpr int kCapacity = 64;

    bool push (RackCommand cmd) noexcept
    {
        int s1, b1, s2, b2;
        fifo.prepareToWrite (1, s1, b1, s2, b2);
        if (b1 == 0)
            return false;
        buffer[static_cast<size_t> (s1)] = cmd;
        fifo.finishedWrite (1);
        return true;
    }

    bool pop (RackCommand& cmd) noexcept
    {
        int s1, b1, s2, b2;
        fifo.prepareToRead (1, s1, b1, s2, b2);
        if (b1 == 0)
            return false;
        cmd = buffer[static_cast<size_t> (s1)];
        fifo.finishedRead (1);
        return true;
    }

  private:
    juce::AbstractFifo fifo{kCapacity};
    std::array<RackCommand, kCapacity> buffer{};
};
