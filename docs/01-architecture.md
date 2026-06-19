# 01 — Architecture

## 1. System layers

```
┌──────────────────────────────────────────────────────────────────────┐
│  HOST (DAW / NLE)  —  loads VST3 / AU / LV2                            │
└──────────────────────────────────────────────────────────────────────┘
            │ audio buffers + params + transport
            ▼
┌──────────────────────────────────────────────────────────────────────┐
│  PLUGIN WRAPPER  (JUCE AudioProcessor)                                 │
│   • format-agnostic; juce_add_plugin emits VST3/AU/LV2/Standalone      │
│   • parameter host bridge  (APVTS ↔ host automation)                   │
│   • reports latency via setLatencySamples() for PDC                    │
└──────────────────────────────────────────────────────────────────────┘
            │
            ▼
┌──────────────────────────────────────────────────────────────────────┐
│  RACK ENGINE  (ProcessorRack)                                          │
│   ordered chain of RackModule instances; lock-free reorder/insert      │
│   ┌────────┐ ┌────────┐ ┌─────────┐ ┌─────┐ ┌──────────┐ ┌─────────┐   │
│   │AIDenoise│→│Dereverb│→│  EQ    │→│Comp │→│ Limiter  │→│StemMixer│   │
│   └────────┘ └────────┘ └─────────┘ └─────┘ └──────────┘ └─────────┘   │
└──────────────────────────────────────────────────────────────────────┘
       │real-time path        │offline path           │viz taps
       ▼                      ▼                        ▼
┌──────────────┐   ┌──────────────────────┐   ┌──────────────────────┐
│  AI ENGINE   │   │ OFFLINE JOB MANAGER   │   │  METERING BUS        │
│ ONNX Runtime │   │ background threads:    │   │ lock-free FIFOs →    │
│ RT sessions  │   │ Demucs stems, heavy    │   │ GUI analyzers/meters │
│ (frame-based)│   │ dereverb, de-clip      │   │                      │
└──────────────┘   └──────────────────────┘   └──────────────────────┘
```

There are **three execution contexts**, and every design decision flows from
keeping them strictly separated:

1. **Audio thread** — hard real-time. No allocation, no locks, no file I/O.
   Runs RT-safe DSP modules and *lightweight, frame-based* AI inference.
2. **Background worker pool** — heavy/offline AI (stem separation, large
   restoration models) operating on captured buffers, never on the audio thread.
3. **Message / GUI thread** — UI rendering, parameter edits, fed by meters
   through lock-free ring buffers.

---

## 2. Threading & data flow

### Audio → GUI (metering)
Single-producer/single-consumer ring buffers (`juce::AbstractFifo`). The audio
thread pushes decimated samples / FFT frames / meter values; the GUI drains them
at ~30–60 Hz on a timer. Never share a `std::vector` or lock between them.

### GUI → Audio (parameters)
All automatable parameters live in a **`juce::AudioProcessorValueTreeState`
(APVTS)**. Edits become atomic value updates the audio thread reads each block.
Smoothing (`juce::SmoothedValue`) prevents zipper noise.

### GUI → Audio (structural changes: reorder / add / remove module)
The audio thread must never see a half-mutated chain. Use a **lock-free command
queue**: the message thread enqueues `InsertModule`, `RemoveModule`,
`MoveModule(from,to)` commands; the audio thread drains the queue at the top of
`processBlock` and applies them. Heap allocation of a new module happens on the
message thread; only the *pointer hand-off* crosses the boundary. Old modules are
retired to a "garbage" queue and freed back on the message thread.

```
message thread                    audio thread
  build module ──► [SPSC cmd FIFO] ──► drain at block start, splice chain
  (alloc here)                          ▲
  free retired ◄── [SPSC retire FIFO] ──┘ (no free in audio thread)
```

### Offline jobs
`OfflineJobManager` owns a `juce::ThreadPool`. A job captures a region of audio
(or receives a host offline-render buffer), runs heavy inference, and writes
results to a result store. Progress + completion marshalled to the GUI via
`juce::AsyncUpdater` / message callbacks.

---

## 3. The Rack engine

### `RackModule` interface
```cpp
class RackModule {
public:
    virtual ~RackModule() = default;

    // lifecycle
    virtual void prepare(const ProcessSpec&) = 0;   // sample rate, block, channels
    virtual void reset() = 0;

    // audio
    virtual void process(AudioBuffer<float>&, MidiBuffer&) = 0;
    virtual int  latencySamples() const { return 0; }   // for PDC summing

    // identity / state
    virtual ModuleType type() const = 0;
    virtual void getState(ValueTree&) const = 0;
    virtual void setState(const ValueTree&) = 0;

    // shared controls every module exposes
    std::atomic<bool>  bypassed { false };
    std::atomic<float> wetDry   { 1.0f };   // global per-module mix
};
```

- **Chain** = ordered `std::vector<std::unique_ptr<RackModule>>`, mutated only
  via the command queue described above.
- **Per-module bypass & wet/dry** handled by the engine (latency-compensated
  bypass: when a latent module is bypassed, the engine still delays the dry path
  so reordering/bypassing doesn't shift timing).
- **Total latency** = Σ `latencySamples()` of active modules, pushed to
  `setLatencySamples()` whenever the chain or any module's latency changes.

### Module catalog (factory)
`AIDenoise, AIDereverb, HumRemover, DeClip, DeClick, SpectralRepair,
ParametricEQ, DynamicEQ, Gate, Compressor, MultibandComp, DeEsser, Limiter,
Saturator, Reverb, Delay, StereoWidth, StemSeparator, StemMixer, Utility`.
Full specs in [02-rack-modules.md](02-rack-modules.md).

---

## 4. AI engine integration

A thin `AIEngine` wraps **ONNX Runtime**:

- **Sessions** are created lazily and **pre-warmed** (one dummy inference) to
  avoid first-call latency spikes inside the audio callback.
- **Execution providers** selectable in settings: CPU/XNNPACK (default & always
  available), CoreML (macOS), DirectML (Windows), CUDA (optional).
- **Real-time models** (DeepFilterNet, RNNoise) run *inside* the audio thread,
  block-by-block, with a fixed compute budget and reported algorithmic latency.
  Pre/post (STFT, windowing, ERB features, normalization) done in C++ or folded
  into the exported graph.
- **Offline models** (Demucs, heavy restoration) run *only* in the worker pool.

See [03-ai-and-stems.md](03-ai-and-stems.md) for pipelines and the stem workflow.

---

## 5. Latency & plugin delay compensation (PDC)

Sources of latency, all summed and reported so the host time-aligns tracks:
- Real-time AI lookahead / STFT frame (e.g. DeepFilterNet ≈ tens of ms).
- Linear-phase EQ (FFT convolution).
- Limiter / de-esser lookahead.
- Oversampling group delay.

The engine recomputes total latency on any change and calls
`setLatencySamples()`. Modules with lookahead expose it via `latencySamples()`.
Offline stem separation adds **no** real-time latency (it's render-then-play).

---

## 6. State, presets & recall

- **APVTS** holds all continuous/automatable parameters.
- A top-level **`ValueTree`** stores: rack order, per-module type + state,
  bypass/wet-dry, global I/O, UI view state, and **references to offline
  artifacts** (separated stems, repair renders) cached on disk by hash.
- `getStateInformation` / `setStateInformation` serialize the tree (binary/XML).
- **Presets**: factory bank (e.g. *Podcast Cleanup, Vocal Rescue, Dialogue for
  Video, Music Restore, Field Recording, Stem Remix Starter*) + user presets as
  files, with an A/B compare buffer and undo history.
- **Offline artifact cache:** separated stems are large; cache them in the
  project/app-data folder keyed by source-audio hash so reopening a session
  restores stems without re-running Demucs.

---

## 7. Module/processing-block sizing

The host block size is arbitrary; AI models want fixed frames. The engine
inserts **internal re-blocking** (a FIFO + fixed-size accumulator) in front of
frame-based AI modules so they always see their native frame size regardless of
host buffer size — at the cost of one frame of latency, which is reported.
