# 06 — Tech Stack, Build & Packaging

## 1. Dependencies

| Concern | Choice | License | Notes |
|---|---|---|---|
| Plugin framework | **JUCE 7.x** | GPLv3 / commercial | VST3/AU/LV2/Standalone from one codebase |
| Build | **CMake** + `juce_add_plugin` | — | reproducible, CI-friendly |
| Language | **C++20** | — | concepts, `std::atomic`, ranges |
| AI runtime | **ONNX Runtime** | MIT | CPU/XNNPACK + optional CoreML/DirectML/CUDA |
| FFT | JUCE `dsp::FFT` or **pffft** | BSD | analyzers, STFT |
| Loudness | **libebur128** | MIT | BS.1770-4 / EBU R128 |
| Resampling | JUCE / **libsamplerate** | BSD/MIT | model-rate conversion for stems |
| Tests | **Catch2** or GoogleTest | BSL/BSD | DSP unit tests |
| Plugin QA | **pluginval** | GPL | host-compat fuzzing/validation |
| VST3 | **Steinberg VST3 SDK** | GPLv3 / Steinberg | required for VST3 target |
| LV2 | JUCE LV2 exporter (+ `lv2` headers) | ISC | verify GUI support per host |

### Bundled AI models
See [03-ai-and-stems.md](03-ai-and-stems.md): **DeepFilterNet** (RT denoise/dereverb),
**RNNoise** (fallback), **Demucs/htdemucs** (offline 4-stem). All exported to
`.onnx`. Large weights install to app-data (or first-run download); small RT
models can be embedded as binary resources.

---

## 2. Repository layout (target)

```
AuClear/
├─ CMakeLists.txt
├─ cmake/                 # find scripts, ORT/JUCE fetch, codesign helpers
├─ src/
│  ├─ PluginProcessor.*   # AudioProcessor: APVTS, latency, state
│  ├─ PluginEditor.*      # top-level UI
│  ├─ rack/               # ProcessorRack, RackModule, command queue
│  ├─ modules/            # one folder per module (EQ, Comp, AIDenoise, …)
│  ├─ ai/                 # AIEngine (ONNX), re-blocker, STFT front-ends
│  ├─ offline/            # OfflineJobManager, Demucs runner, artifact cache
│  ├─ dsp/                # shared DSP (filters, oversampling, meters)
│  ├─ ui/                 # LookAndFeel, widgets, analyzers, spectrogram
│  └─ state/              # presets, ValueTree schema, A/B, undo
├─ models/                # .onnx assets + manifest (hash, license, rate)
├─ resources/             # fonts, icons, color maps
├─ tests/                 # Catch2 DSP + state tests
└─ docs/                  # this design package
```

---

## 3. Build & CI

- **Local:** `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build`.
  Targets: `AuClear_VST3`, `AuClear_AU` (macOS), `AuClear_LV2`,
  `AuClear_Standalone`.
- **CI matrix (GitHub Actions):**
  - **macOS** (arm64 + x86_64 universal) → VST3, AU, LV2
  - **Windows** (x64) → VST3, LV2
  - **Linux** (x64) → VST3, LV2
  - Each job: configure → build → run Catch2 tests → run **pluginval** at strict
    level on the built plugins → upload artifacts.
- **ONNX Runtime** fetched as prebuilt per-platform (or built once and cached) to
  keep CI fast; model assets pulled from a release bucket, not committed raw if
  large.

---

## 4. Packaging & distribution

| Platform | Formats | Installer | Signing |
|---|---|---|---|
| macOS | AU, VST3, LV2 | `.pkg` (productbuild) | **codesign + notarize + staple** (Developer ID) |
| Windows | VST3, LV2 | Inno Setup / WiX | **Authenticode** signing |
| Linux | VST3, LV2 | `.tar.xz` / `.deb` / flatpak | — |

- Standard install paths per format (e.g. macOS `~/Library/Audio/Plug-Ins/{VST3,Components}`).
- Models installed alongside or downloaded on first launch with a license prompt.
- Auto-update channel + version/build stamped into the UI header.

---

## 5. DAW / NLE compatibility matrix (test targets)

Reaper, Ableton Live, Logic Pro (AU), Cubase/Nuendo, Studio One, FL Studio,
Pro Tools (AAX optional, future), Bitwig, Ardour (LV2), **DaVinci Resolve /
Fairlight (VST3/AU)**, **Premiere Pro / Audition (VST3)**, **Final Cut (AU via
wrapper)**. Validate transport, PDC alignment, offline/bounce rendering, and
state recall in each.

---

## 6. Licensing (must clear before any release) {#licensing}

> This is the biggest non-engineering risk. Treat it as a release gate.

- **JUCE** — GPLv3 *or* paid commercial license. A closed-source commercial
  product needs the commercial license.
- **Steinberg VST3 SDK** — GPLv3 or Steinberg's proprietary license; agree to
  terms to ship VST3.
- **ONNX Runtime** — MIT, clean.
- **Model weights** — *the critical one.* Code license ≠ weight redistribution
  rights. Demucs/htdemucs weights may be trained on datasets (e.g. MUSDB18 +
  extra) with research-only terms. **Before bundling, confirm commercial
  redistribution of the weights**, or:
  1. ship only permissively-licensed weights, or
  2. fine-tune/train your own weights on cleared data (the "Hybrid" path), or
  3. download weights on first run with explicit user acceptance.
- **Fonts/icons** — verify embedding rights (Inter = OFL, fine).

Maintain `models/MANIFEST` and a `THIRD_PARTY_LICENSES` file enumerating every
dependency, model, hash, and license.
