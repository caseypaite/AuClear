#include "ProcessorRack.h"

ProcessorRack::ProcessorRack ()
{
    // Reserve so vector::insert/erase inside the audio thread never allocate.
    audioChain.reserve (64);
}

// ---------------------------------------------------------------------------
// Message thread: structural edits
// ---------------------------------------------------------------------------

void ProcessorRack::insertModule (int index, std::unique_ptr<RackModule> module)
{
    jassert (juce::MessageManager::getInstance ()->isThisTheMessageThread ());

    auto* raw = module.get ();
    ownedModules.push_back (std::move (module));

    index = juce::jlimit (0, (int)guiOrder.size (), index);
    guiOrder.insert (guiOrder.begin () + index, raw);

    // Prepare here on the message thread so the audio thread's drainCommandQueue()
    // only has to splice a pointer — no heap alloc on the RT thread.
    if (currentSpec.sampleRate > 0)
        raw->prepare (currentSpec);

    commandQueue.push ({RackCommand::Type::Insert, index, -1, raw});
}

void ProcessorRack::removeModule (int index)
{
    jassert (juce::MessageManager::getInstance ()->isThisTheMessageThread ());
    if (index < 0 || index >= static_cast<int> (guiOrder.size ()))
        return;

    auto* raw = guiOrder[static_cast<size_t> (index)];
    guiOrder.erase (guiOrder.begin () + index);
    // Keep raw in ownedModules until audio thread retires it.

    commandQueue.push ({RackCommand::Type::Remove, -1, -1, raw});
}

void ProcessorRack::moveModule (int from, int to)
{
    jassert (juce::MessageManager::getInstance ()->isThisTheMessageThread ());
    const int n = static_cast<int> (guiOrder.size ());
    if (from < 0 || from >= n || to < 0 || to >= n || from == to)
        return;

    auto* ptr = guiOrder[static_cast<size_t> (from)];
    guiOrder.erase (guiOrder.begin () + from);
    guiOrder.insert (guiOrder.begin () + to, ptr);

    commandQueue.push ({RackCommand::Type::Move, from, to, nullptr});
}

int ProcessorRack::numModules () const
{
    return (int)guiOrder.size ();
}

RackModule* ProcessorRack::getModule (int index) const
{
    if (index < 0 || index >= static_cast<int> (guiOrder.size ()))
        return nullptr;
    return guiOrder[static_cast<size_t> (index)];
}

void ProcessorRack::getState (juce::ValueTree& tree) const
{
    tree.removeAllChildren (nullptr);
    for (auto* m : guiOrder)
    {
        juce::ValueTree ms ("Module");
        ms.setProperty ("type", (int)m->type (), nullptr);
        ms.setProperty ("uid", m->uid, nullptr);
        ms.setProperty ("bypassed", (bool)m->bypassed.load (), nullptr);
        ms.setProperty ("wetDry", (float)m->wetDry.load (), nullptr);
        m->getState (ms);
        tree.appendChild (ms, nullptr);
    }
}

void ProcessorRack::setState (const juce::ValueTree& tree,
                              std::function<std::unique_ptr<RackModule> (ModuleType)> factory)
{
    while (!guiOrder.empty ())
        removeModule (0);

    for (auto child : tree)
    {
        auto typeInt = static_cast<int> (child.getProperty ("type", -1));
        if (typeInt < 0)
            continue;

        auto mod = factory (static_cast<ModuleType> (typeInt));
        if (!mod)
            continue;

        mod->bypassed.store ((bool)child.getProperty ("bypassed", false));
        mod->wetDry.store ((float)child.getProperty ("wetDry", 1.0f));
        mod->setState (child);

        insertModule (static_cast<int> (guiOrder.size ()), std::move (mod));
    }
}

void ProcessorRack::retireOldModules ()
{
    jassert (juce::MessageManager::getInstance ()->isThisTheMessageThread ());

    const int available = retireFifo.getNumReady ();
    if (available == 0)
        return;

    int s1, b1, s2, b2;
    retireFifo.prepareToRead (available, s1, b1, s2, b2);

    auto retire = [&] (RackModule* ptr)
    {
        auto it = std::find_if (ownedModules.begin (), ownedModules.end (),
                                [ptr] (const auto& up) { return up.get () == ptr; });
        if (it != ownedModules.end ())
            ownedModules.erase (it); // unique_ptr destructor deletes the module
    };

    for (int i = 0; i < b1; ++i)
        retire (retireBuffer[static_cast<size_t> (s1 + i)]);
    for (int i = 0; i < b2; ++i)
        retire (retireBuffer[static_cast<size_t> (s2 + i)]);
    retireFifo.finishedRead (available);
}

// ---------------------------------------------------------------------------
// Audio thread
// ---------------------------------------------------------------------------

void ProcessorRack::prepare (const juce::dsp::ProcessSpec& spec)
{
    currentSpec = spec;
    dryBuffer.setSize ((int)spec.numChannels, (int)spec.maximumBlockSize, false, false, true);
    for (auto* m : audioChain)
        m->prepare (spec);
    specFifo.prepare (spec.sampleRate);
    lufs.prepare (spec.sampleRate, (int)spec.numChannels);
}

void ProcessorRack::reset ()
{
    for (auto* m : audioChain)
        m->reset ();
}

static float linearToDb (float linear)
{
    return linear > 0.0f ? juce::Decibels::gainToDecibels (linear) : -100.0f;
}

void ProcessorRack::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    drainCommandQueue ();

    // Input metering
    {
        const int nch = buffer.getNumChannels ();
        MeterValues v;
        v.peakL = linearToDb (buffer.getMagnitude (0, 0, buffer.getNumSamples ()));
        v.peakR =
            (nch > 1) ? linearToDb (buffer.getMagnitude (1, 0, buffer.getNumSamples ())) : v.peakL;
        v.rmsL = linearToDb (buffer.getRMSLevel (0, 0, buffer.getNumSamples ()));
        v.rmsR =
            (nch > 1) ? linearToDb (buffer.getRMSLevel (1, 0, buffer.getNumSamples ())) : v.rmsL;
        inMeters.push (v);
    }

    const int nSamples = buffer.getNumSamples ();
    const int nCh = buffer.getNumChannels ();

    for (auto* m : audioChain)
    {
        if (m->bypassed.load (std::memory_order_relaxed))
            continue;

        const float wet = m->wetDry.load (std::memory_order_relaxed);

        if (wet >= 0.999f)
        {
            m->process (buffer, midi);
        }
        else
        {
            // Copy dry, process, then blend.
            for (int ch = 0; ch < nCh; ++ch)
                dryBuffer.copyFrom (ch, 0, buffer, ch, 0, nSamples);

            m->process (buffer, midi);

            for (int ch = 0; ch < nCh; ++ch)
            {
                buffer.applyGain (ch, 0, nSamples, wet);
                buffer.addFrom (ch, 0, dryBuffer, ch, 0, nSamples, 1.0f - wet);
            }
        }
    }

    // Output metering
    {
        const int nch = buffer.getNumChannels ();
        MeterValues v;
        v.peakL = linearToDb (buffer.getMagnitude (0, 0, buffer.getNumSamples ()));
        v.peakR =
            (nch > 1) ? linearToDb (buffer.getMagnitude (1, 0, buffer.getNumSamples ())) : v.peakL;
        v.rmsL = linearToDb (buffer.getRMSLevel (0, 0, buffer.getNumSamples ()));
        v.rmsR =
            (nch > 1) ? linearToDb (buffer.getRMSLevel (1, 0, buffer.getNumSamples ())) : v.rmsL;
        outMeters.push (v);
    }

    // Spectrum analysis and loudness measurement on output
    specFifo.pushSamples (buffer);
    lufs.process (buffer);
}

void ProcessorRack::drainCommandQueue ()
{
    RackCommand cmd;
    while (commandQueue.pop (cmd))
    {
        switch (cmd.type)
        {
        case RackCommand::Type::Insert:
        {
            int idx = juce::jlimit (0, (int)audioChain.size (), cmd.indexA);
            // Module was already prepared on the message thread in insertModule().
            audioChain.insert (audioChain.begin () + idx, cmd.module);
            break;
        }

        case RackCommand::Type::Remove:
        {
            auto it = std::find (audioChain.begin (), audioChain.end (), cmd.module);
            if (it != audioChain.end ())
            {
                audioChain.erase (it);
                // Hand back to message thread for deallocation.
                int s1, b1, s2, b2;
                retireFifo.prepareToWrite (1, s1, b1, s2, b2);
                if (b1 > 0)
                {
                    retireBuffer[static_cast<size_t> (s1)] = cmd.module;
                    retireFifo.finishedWrite (1);
                }
            }
            break;
        }

        case RackCommand::Type::Move:
        {
            const int n = (int)audioChain.size ();
            const int from = cmd.indexA;
            const int to = cmd.indexB;
            if (from >= 0 && from < n && to >= 0 && to < n)
            {
                auto* ptr = audioChain[static_cast<size_t> (from)];
                audioChain.erase (audioChain.begin () + from);
                audioChain.insert (audioChain.begin () + to, ptr);
            }
            break;
        }

        case RackCommand::Type::None:
            break;
        }
    }

    // Recompute and cache total latency so the host can query it without a race.
    int lat = 0;
    for (auto* m : audioChain)
        if (!m->bypassed.load (std::memory_order_relaxed))
            lat += m->latencySamples ();
    cachedLatency.store (lat, std::memory_order_relaxed);
}
