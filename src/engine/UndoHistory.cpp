#include "UndoHistory.h"
#include "ProcessorRack.h"

void UndoHistory::snapshot (const ProcessorRack& rack)
{
    juce::ValueTree tree ("Rack");
    rack.getState (tree);

    states.push_back (tree);
    while ((int)states.size () > kMaxDepth)
        states.pop_front ();
}

bool UndoHistory::undo (ProcessorRack& rack, FactoryFn factory)
{
    if (states.empty ())
        return false;

    const auto tree = states.back ();
    states.pop_back ();
    rack.setState (tree, factory);
    return true;
}
