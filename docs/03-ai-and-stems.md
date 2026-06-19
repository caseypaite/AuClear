# 03 — AI & Stems

This is where AuClear's "embedded AI" lives. The key architectural rule:

> **Real-time models run in the audio thread on small fixed frames.
> Heavy models run offline in background workers on captured buffers.**

Mixing these up is the #1 way to ship a plugin that crackles and drops out.

---

## 1. Inference layer (`AIEngine`)

Thin C++ wrapper over **ONNX Runtime**:

- **Model packaging:** `.onnx` files shipped as binary resources (small RT
  models) or installed to app-data (large models like Demucs, ~hundreds of MB).
  Loaded lazily; **pre-warmed** with a dummy inference so the first real call
  doesn't allocate/JIT inside the audio callback.
- **Execution providers (selectable in Settings):**
  - CPU + **XNNPACK** — default, always available, deterministic budget.
  - **CoreML** (macOS), **DirectML** (Windows), **CUDA** (optional) — only for
    *offline* jobs by default; RT models stay on CPU for predictable timing.
- **Threading:** RT sessions are single-threaded (intra-op = 1) to keep the
  audio-thread cost bounded. Offline sessions can use all cores.
- **I/O:** pre/post-processing (STFT, windowing, ERB/feature extraction,
  normalization, ISTFT/overlap-add) implemented in C++ around the model, or
  folded into the exported graph where possible.

```cpp
struct AIEngine {
    SessionHandle load(ModelId, ExecProvider);   // lazy + cached
    void          prewarm(SessionHandle);
    void          run(SessionHandle, const Tensor& in, Tensor& out); // RT-safe for RT models
};
```

---

## 2. Real-time denoise / dereverb (DeepFilterNet)

Pipeline, executed block-by-block in the audio thread behind an internal
re-blocker that feeds the model its native frame/hop:

```
in ─► STFT (window+FFT) ─► ERB band features ─► [neural net] ─┐
                                                              │ predicts:
                          ┌───────────────────────────────────┘  • band gains
                          ▼                                       • deep-filter coeffs
        apply gains + deep filter ─► ISTFT (overlap-add) ─► out
```

- **Latency** = analysis window + lookahead frames; reported via PDC.
- **CPU control:** "Quality vs CPU" selects model size (Nano/Small/Full). A
  hard real-time budget guard can fall back to RNNoise or reduce update rate if
  a frame risks overrun.
- **Dereverb** shares the same STFT front-end; a light model runs RT, a heavier
  variant is offered as an offline pass for hard cases.
- **"Listen to removed"** monitors `input − output` so the user can verify the
  model isn't eating wanted signal.

### RNNoise fallback
Tiny GRU model (BSD-licensed) for very low-CPU hosts or when DeepFilterNet's
budget is exceeded. Lower quality, near-zero cost.

---

## 3. Offline 4-stem separation (Demucs)

Demucs (htdemucs hybrid transformer) is **not real-time** — it needs seconds of
context and is happiest on a GPU. So the "remix from a recorded track" feature is
a **capture → render → playback** workflow, like iZotope RX Music Rebalance or
Hit'n'Mix RipX, *not* a live insert effect.

### Workflow

```
 1. CAPTURE
    User arms the Stem Separator and plays the track through the plugin
    (or selects the host loop region). Plugin records the input to a buffer.
       └─ alternatively: drag-drop an audio file into the Stem panel.

 2. SEPARATE  (background ThreadPool job)
    ┌────────────────────────────────────────────────────────┐
    │ resample to model rate (44.1k)                          │
    │ chunk into overlapping segments (e.g. 7.8 s, 25% overlap)│
    │ run Demucs per segment on worker/GPU                    │
    │ overlap-add → 4 full-length stem buffers                │
    │ report progress % to GUI (AsyncUpdater)                 │
    └────────────────────────────────────────────────────────┘

 3. CACHE
    Write stems to app-data/project cache, keyed by hash of source audio +
    model + settings, so reopening the session restores them instantly.

 4. PLAYBACK / REMIX  (real-time, StemMixer module)
    Stems play back in sync, summed in place of the dry signal:
    ┌──────────┬──────────┬──────────┬──────────┐
    │ VOCALS   │ DRUMS    │ BASS     │ OTHER     │  ← gain / mute / solo / pan
    └──────────┴──────────┴──────────┴──────────┘  ← optional per-stem FX send
            └──────────── sum ───────────┘ + dry blend → out
```

### Why offline (be explicit with the user)
- Demucs latency/compute is far beyond an audio block; forcing it RT would drop
  audio. The render-then-play model gives studio-quality separation with a
  progress bar instead of glitches.
- Once rendered, **playback and remixing are fully real-time** — faders, mutes,
  solos, and per-stem effects respond instantly.

### Use cases unlocked
- Mute/lower vocals → instant instrumental / karaoke.
- Solo drums for sampling; isolate bass for transcription.
- Re-balance a stereo mix you don't have stems for; clean a single stem with the
  AI repair modules (route a stem into its own sub-rack).

### Performance & options
- **Quality presets:** Fast (smaller overlap / lighter variant) ↔ Best (more
  shifts, higher overlap).
- GPU (CoreML/DirectML/CUDA) used when available for a large speed-up; CPU
  fallback always works, just slower.
- Time-remaining estimate shown; job cancellable.

---

## 4. Other offline AI jobs
De-Clip, De-Crackle, heavy Dereverb, and Spectral Repair use the same
`OfflineJobManager`: capture region → background inference → cached render →
the module plays the repaired buffer in place. Spectral Repair additionally has
an **interactive spectrogram editor** for marquee selections.

---

## 5. Model summary & licensing flags

| Model | Role | Mode | Code license | ⚠️ Weight redistribution |
|---|---|---|---|---|
| DeepFilterNet | denoise + dereverb | RT | MIT/Apache | verify weight terms |
| RNNoise | denoise fallback | RT | BSD | OK |
| Demucs (htdemucs) | 4-stem separation | OFF | MIT | **training-data/weight terms must be cleared** |

**Action item before shipping:** confirm you may *redistribute the trained
weights* in a commercial product (code license ≠ weight license; some weights are
trained on datasets with research-only terms). Options: bundle only
clearly-licensed weights, train/fine-tune your own (see the "Hybrid" path), or
download weights on first run with the user accepting terms. Tracked in
[06-tech-stack-build.md](06-tech-stack-build.md#licensing).
