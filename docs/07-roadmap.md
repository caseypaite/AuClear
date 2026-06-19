# 07 — Roadmap

A phased plan that de-risks the hard parts (real-time AI budget, offline stem UX,
LV2 GUI, model licensing) early and keeps a shippable artifact at every stage.

> Estimates assume a small team. Treat them as relative sizing, not commitments.

---

## Phase 0 — Foundations  *(build skeleton)*
**Goal:** an empty plugin that loads and validates in all three formats.
- CMake + JUCE `juce_add_plugin`; VST3 / AU / LV2 / Standalone targets.
- CI matrix (mac/win/linux): build + **pluginval** on an empty processor.
- Repo layout, code style, `THIRD_PARTY_LICENSES` skeleton.
- **Exit:** empty AuClear loads in Reaper/Logic/Resolve and passes pluginval.

## Phase 1 — Rack engine + shell  *(the spine)*
**Goal:** working modular chain with one trivial module and full state recall.
- `ProcessorRack`, `RackModule`, lock-free command queue (reorder/add/remove).
- APVTS, latency summing/PDC, A/B + undo, preset save/load (`ValueTree` schema).
- UI shell: header, rack column, main stage, inspector, meter bridge.
- `Utility/Gain` module + I/O meters end-to-end.
- **Exit:** add/reorder/remove a module live with no glitches; state survives
  save/reload; meters move.

## Phase 2 — Core DSP modules  *(classic rack)*
**Goal:** a useful channel strip even before any AI.
- Parametric EQ (+ draggable curve over analyzer), Gate, Compressor, Limiter
  (+ true-peak/LUFS), Reverb, Delay, Saturator, Stereo/Utility.
- Visualizations: spectrum analyzer, EQ curve, transfer curves, GR meters,
  goniometer/correlation, LUFS suite (libebur128).
- **Exit:** ship-quality classic channel strip; loudness metering validated
  against reference.

## Phase 3 — Real-time AI  *(the differentiator, part 1)*
**Goal:** neural denoise + dereverb running glitch-free in the audio thread.
- `AIEngine` over ONNX Runtime; internal re-blocker; pre-warm; EP selection.
- DeepFilterNet RT denoise + dereverb; RNNoise fallback; CPU-budget guard.
- "Listen to removed" + before/after spectrogram; Hum Remover.
- **Exit:** denoise holds real-time at target buffer sizes across hosts; latency
  reported correctly; A/B shows clear cleanup.

## Phase 4 — Offline AI & Stems  *(the differentiator, part 2)*
**Goal:** 4-stem remix workflow.
- `OfflineJobManager` (ThreadPool), capture, artifact cache (hash-keyed).
- Demucs runner (chunk/overlap-add), GPU EPs where available, progress/ETA/cancel.
- Stem Remix UI: waveform, 4 channel strips, solo/mute/gain/pan, dry blend,
  per-stem FX send.
- De-Clip / De-Crackle offline jobs share the pipeline.
- **Exit:** separate a track → mute vocals → instant instrumental; stems cached
  and restored on reload.

## Phase 5 — Advanced repair & viz  *(depth)*
- Spectral Repair interactive spectrogram editor (marquee selections).
- Dynamic EQ, Multiband Compressor, De-Esser.
- Full spectrogram surface (before/after wipe), decay/RT60, harmonic spectrum.
- **Exit:** repair feature set competitive with entry-tier RX.

## Phase 6 — Polish & UX  *(make it pro)*
- Final `LookAndFeel`, resizable/DPI, light theme, accessibility pass.
- Full factory preset library; preset browser; tooltips/help; onboarding.
- Performance pass (OpenGL panels, repaint throttling, model size options).
- **Exit:** UX on par with the FabFilter/iZotope peer group.

## Phase 7 — Release  *(ship it)*
- Codesign/notarize (mac), Authenticode (win), installers, LV2 bundles.
- Full DAW/NLE compatibility matrix sign-off (incl. Resolve/Premiere/Audition).
- **License gate cleared** (esp. model weights — see
  [06](06-tech-stack-build.md#licensing)).
- Crash/telemetry opt-in, auto-update, docs/site.
- **Exit:** AuClear 1.0.

---

## Cross-cutting risks & mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| **Model weight licensing** | Can't ship as designed | Clear early (Phase 0–1); fallback to own-trained/first-run download |
| RT AI CPU budget / dropouts | Core feature unusable on weak machines | Model size tiers, budget guard, RNNoise fallback, re-blocker |
| Stem separation UX in plugin sandbox | Confusing/blocking | Explicit capture→render→play model + progress/cancel + cache |
| LV2 GUI support across hosts | Broken UI on Linux | Validate in Ardour/Resolve early (Phase 0); headless-safe fallback |
| PDC misalignment | Phasey/late audio in hosts | Sum + report latency rigorously; test bounce vs realtime per host |
| Large model download/footprint | Install friction | First-run download w/ progress; per-feature model fetch |

---

## What to do next (Phase 0 kickoff)
When you're ready to leave the design phase, the first concrete tasks are:
1. Scaffold CMake + JUCE plugin with all four targets.
2. Stand up the CI matrix with pluginval on an empty processor.
3. Drop in ONNX Runtime and prove a trivial `.onnx` inference on each platform.

> You chose **"design docs only"** for now — say the word and I'll scaffold
> Phase 0 (CMake/JUCE skeleton compiling to VST3/AU/LV2) as the next step.
