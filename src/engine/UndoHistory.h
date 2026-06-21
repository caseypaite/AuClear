#pragma once
#include <JuceHeader.h>
#include "RackModule.h"
#include <deque>
#include <functional>
#include <memory>

class ProcessorRack;

/**
 * Snapshot-based undo for rack structural changes (add/remove/move module).
 * Parameter edits are NOT tracked — only topology changes.
 * Holds up to kMaxDepth states; oldest snapshots are discarded when full.
 */
class UndoHistory
{
  public:
    static constexpr int kMaxDepth = 20;

    using FactoryFn = std::function<std::unique_ptr<RackModule>(ModuleType)>;

    // Take a snapshot of the current rack state.  Call BEFORE the structural change.
    void snapshot (const ProcessorRack& rack);

    // Undo the last structural change.  Returns false if the stack is empty.
    bool undo (ProcessorRack& rack, FactoryFn factory);

    // Clear the entire history (e.g., after loading a preset).
    void clear () noexcept { states.clear (); }

    bool canUndo () const noexcept { return !states.empty (); }

  private:
    std::deque<juce::ValueTree> states;
};
