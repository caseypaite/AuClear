# AuClear

**AuClear** is a cross-platform audio repair, restoration, and creative-processing
plugin for DAWs and video-editing software. It combines embedded AI models
(noise/reverb/artifact removal and 4-stem music separation) with a stackable,
"audio rack" of classic DSP processors (EQ, dynamics, limiter, reverb, delay),
wrapped in a professional, visualization-rich UI.

> One C++/JUCE codebase → **VST3**, **AU**, **LV2** (plus a Standalone app for
> testing). Runs in Pro Tools-class DAWs, Reaper, Ableton, Logic, FL Studio,
> Cubase, Ardour, DaVinci Resolve, Premiere Pro (via VST3/AU), Final Cut (via AU
> wrappers), etc.

---

## Vision

Audio cleanup today is split between two worlds: dedicated repair suites
(iZotope RX, Acon DeNoise, Accentize) and creative channel-strip plugins
(FabFilter, Waves). AuClear merges both into a single **modular rack** where an
AI denoiser, a surgical EQ, a compressor, and a stem-remixer all live in one
reorderable signal chain — with first-class, real-time visual feedback at every
stage.

### Headline features

| Category | Feature |
|---|---|
| **AI Repair (real-time)** | Neural noise reduction, dereverberation, hum/buzz removal |
| **AI Repair (offline)** | De-clip, de-click/de-crackle, spectral repair, heavy dereverb |
| **AI Remix (offline)** | 4-stem separation (vocals / drums / bass / other) → per-stem mixer |
| **Classic DSP rack** | Parametric + dynamic EQ, gate/expander, compressor, multiband comp, de-esser, brickwall limiter, saturator, algorithmic reverb, delay, stereo width, utility/gain |
| **Metering & viz** | Spectrum analyzer, before/after spectrogram, EQ curve, GR meters, LUFS/true-peak loudness suite, goniometer + correlation, waveform/stem lanes |
| **Workflow** | Drag-to-reorder rack, per-module bypass + wet/dry, A/B compare, factory + user presets, full state recall |

---

## Tech stack (chosen)

- **Framework:** JUCE 7.x (`juce_add_plugin`, CMake) — VST3 / AU / LV2 / Standalone
- **Language:** C++20
- **AI runtime:** ONNX Runtime (CPU/XNNPACK default; CoreML / DirectML / CUDA optional)
- **Bundled models (pretrained, open-source, exported to ONNX):**
  - **DeepFilterNet** — real-time denoise + dereverb
  - **Demucs (htdemucs)** — offline 4-stem separation
  - **RNNoise** — lightweight fallback denoiser
- **Loudness:** libebur128 (ITU-R BS.1770-4 / EBU R128)
- **Validation:** pluginval + Catch2/GoogleTest, CI matrix on macOS/Windows/Linux

> ⚠️ **Licensing must be cleared before shipping.** JUCE (GPL vs commercial), the
> Steinberg VST3 SDK, ONNX Runtime (MIT), and especially **model weights**
> (code licenses ≠ weight redistribution rights) each have terms. See
> [docs/06-tech-stack-build.md](docs/06-tech-stack-build.md#licensing).

---

## Documentation map

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

## Status

🏗️ **Phase 0 scaffolded.** The repo now contains a cross-format JUCE plugin
skeleton (empty passthrough) that builds to VST3/AU/LV2/Standalone, plus a
CI matrix that validates the VST3 with `pluginval`. Build instructions:
[docs/BUILDING.md](docs/BUILDING.md). Next up: Phase 1 (rack engine + UI shell)
— see the [roadmap](docs/07-roadmap.md).
