# AuClear

[![CI](https://github.com/caseypaite/AuClear/actions/workflows/build.yml/badge.svg)](https://github.com/caseypaite/AuClear/actions/workflows/build.yml)

**AuClear** is a cross-platform audio repair, restoration, and creative-processing
plugin for DAWs and video-editing software. It combines embedded AI models
(noise/reverb/artifact removal and 4-stem music separation) with a stackable
**audio rack** of classic DSP processors (EQ, dynamics, limiter, reverb, delay),
wrapped in a professional, visualization-rich UI.

> One C++/JUCE codebase → **VST3**, **AU**, **LV2** (plus a Standalone app).
> Targets Pro Tools-class DAWs, Reaper, Ableton, Logic, FL Studio, Cubase,
> Ardour, DaVinci Resolve, Premiere Pro, and Final Cut.

---

## Vision

Audio cleanup today is split between dedicated repair suites (iZotope RX,
Acon DeNoise, Accentize) and creative channel-strip plugins (FabFilter, Waves).
AuClear merges both into a single **modular rack** where an AI denoiser, a
surgical EQ, a compressor, and a stem-remixer all live in one reorderable signal
chain — with first-class, real-time visual feedback at every stage.

### Headline features

| Category | Feature |
|---|---|
| **AI Repair (real-time)** | Neural noise reduction, dereverberation, hum/buzz removal |
| **AI Repair (offline)** | De-clip, de-click/de-crackle, spectral repair, heavy dereverb |
| **AI Remix (offline)** | 4-stem separation (vocals / drums / bass / other) → per-stem mixer |
| **Classic DSP rack** | Parametric + dynamic EQ, gate, compressor, multiband comp, de-esser, brickwall limiter, saturator, reverb, delay, stereo width, utility/gain |
| **Metering & viz** | Spectrum analyzer, before/after spectrogram, EQ curve, GR meters, LUFS/true-peak loudness suite, goniometer + correlation |
| **Workflow** | Drag-to-reorder rack, per-module bypass + wet/dry, A/B compare, factory + user presets, full state recall |

---

## Tech stack

| Component | Choice |
|---|---|
| **Framework** | JUCE 8.0.4 (`juce_add_plugin`, CMake) — VST3 / AU / LV2 / Standalone |
| **Language** | C++20 |
| **AI runtime** | ONNX Runtime (CPU/XNNPACK default; CoreML / DirectML / CUDA optional) |
| **Denoise model** | DeepFilterNet — real-time, exported to ONNX |
| **Stem model** | Demucs (htdemucs) — offline 4-stem, ONNX |
| **Fallback denoiser** | RNNoise |
| **Loudness** | libebur128 (ITU-R BS.1770-4 / EBU R128) |
| **Validation** | pluginval (strictness 10), CI matrix on macOS / Windows / Linux |

> **Licensing:** JUCE (GPL vs commercial), the VST3 SDK, ONNX Runtime (MIT), and
> model weights each carry separate terms. Cleared before any release.
> See [docs/06-tech-stack-build.md](docs/06-tech-stack-build.md#licensing).

---

## Status

**Phase 1 complete.** The rack engine spine, first DSP module, and full UI shell
are wired up and building cleanly on all three platforms.

| Phase | Description | Status |
|---|---|---|
| **0** | CMake + JUCE scaffold; VST3/AU/LV2/Standalone; CI + pluginval | ✅ Done |
| **1** | Rack engine, lock-free command queue, GainModule, UI shell, I/O meters | ✅ Done |
| **2** | Classic DSP rack — EQ, gate, compressor, limiter, reverb, delay, visualizations | 🔜 Next |
| **3** | Real-time AI — DeepFilterNet denoise + dereverb, hum removal | 📋 Planned |
| **4** | Offline AI + stems — Demucs, per-stem remix UI | 📋 Planned |
| **5** | Advanced repair + viz — spectral repair, dynamic EQ, multiband comp | 📋 Planned |
| **6** | Polish & UX — final LookAndFeel, presets, accessibility, performance | 📋 Planned |
| **7** | Release — codesign, installers, compatibility matrix, 1.0 | 📋 Planned |

### Phase 1 deliverables

- **`ProcessorRack`** — ordered module chain with a lock-free SPSC command queue
  (insert / remove / reorder handled on the audio thread, memory freed on the
  message thread); total PDC latency cached atomically.
- **`CommandQueue`** / **`MeterBus`** — lock-free ring buffers for structural
  commands and audio → GUI metering.
- **`RackModule`** interface + **`GainModule`** (first concrete module, −60 to
  +12 dB with 10 ms smoothing).
- **UI shell** — 5-panel layout: Header · Rack column with drag-to-reorder ·
  Main stage (module panel) · Inspector placeholder · Meter bridge.
- **`AuClearLookAndFeel`** — teal/dark brand palette, custom rotary knob.
- **APVTS** (global bypass), full `getStateInformation` / `setStateInformation`
  with rack `ValueTree` serialization.
- Zero warnings from project source files against JUCE recommended warning flags.

---

## Building

**Prerequisites:** CMake ≥ 3.22, a C++20 compiler, Git.
Linux also needs [system dependencies](docs/BUILDING.md#linux-deps).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

JUCE 8.0.4 is fetched automatically. Override with
`-DAUCLEAR_JUCE_TAG=...` or point at a local copy:
`-DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE`.

Full instructions → [docs/BUILDING.md](docs/BUILDING.md)

---

## Documentation

| Doc | Contents |
|---|---|
| [01 — Architecture](docs/01-architecture.md) | System layers, threading model, rack engine, latency/PDC, state & presets |
| [02 — Rack Modules](docs/02-rack-modules.md) | Spec for every processor: params, viz, RT-safety, latency |
| [03 — AI & Stems](docs/03-ai-and-stems.md) | ONNX inference layer, real-time denoise/dereverb, offline stem-separation workflow |
| [04 — UI Design](docs/04-ui-design.md) | Design language, layout, wireframes, interaction, LookAndFeel |
| [05 — Visualizations](docs/05-visualizations.md) | Every graph/meter, the DSP behind it, update/threading model |
| [06 — Tech Stack & Build](docs/06-tech-stack-build.md) | Dependencies, CMake/CI, packaging, signing, licensing |
| [07 — Roadmap](docs/07-roadmap.md) | Phased delivery plan, milestones, risks |

---

## CI / CD

Every push and pull request runs:

1. **Format check** — `clang-format --dry-run --Werror` on all `src/` files.
2. **Build matrix** — Release build on Ubuntu 22.04, macOS 14, and Windows 2022.
3. **pluginval** — VST3 validated at strictness level 10 on every platform.
4. **Artifacts** — Plugin bundles uploaded (14-day retention) per platform + SHA.

Pushing a `v*` tag additionally creates a **draft GitHub Release** with zipped
platform artifacts attached.
