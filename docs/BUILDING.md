# Building AuClear

## Prerequisites

- **CMake ≥ 3.22** and a **C++20** compiler
  (Xcode/Clang on macOS, MSVC 2022 on Windows, GCC ≥ 12 / Clang ≥ 14 on Linux)
- **Git** — JUCE 8.0.4 is fetched automatically via `FetchContent`

### Linux deps

```bash
sudo apt-get install -y \
  libasound2-dev libjack-jackd2-dev ladspa-sdk \
  libfreetype-dev libfontconfig1-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
  libxinerama-dev libxrandr-dev libxrender-dev \
  libglu1-mesa-dev mesa-common-dev \
  libgtk-3-dev libwebkit2gtk-4.1-dev
```

## Configure & build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

**JUCE options:**

| Option | Default | Effect |
|---|---|---|
| `-DAUCLEAR_JUCE_TAG=<tag>` | `8.0.4` | Use a different JUCE tag |
| `-DFETCHCONTENT_SOURCE_DIR_JUCE=/path` | — | Use a local JUCE clone (skips network fetch) |

**ONNX Runtime options (Phase 3 AI):**

| Option | Default | Effect |
|---|---|---|
| `-DAUCLEAR_NO_ONNX=ON` | `OFF` | Disable AI modules (classic DSP rack only) |
| `-DAUCLEAR_ORT_ROOT=/path` | — | Use a pre-extracted ORT directory (skips network fetch) |
| `-DAUCLEAR_ORT_VERSION=<ver>` | `1.20.0` | ORT version to fetch or match in ORT_ROOT |

ORT is fetched automatically from the GitHub releases page if `AUCLEAR_ORT_ROOT` is not set.
With an existing local copy (e.g. from another project):

```bash
cmake -B build \
  -DAUCLEAR_ORT_ROOT=/path/to/onnxruntime-linux-x64-1.18.0 \
  -DAUCLEAR_ORT_VERSION=1.18.0
```

### Generating test ONNX models

A helper script is provided to create minimal models for verifying the AI pipeline:

```bash
pip install onnx
python tools/make_test_model.py --type identity   # passthrough (verifies latency)
python tools/make_test_model.py --type gate        # simple noise gate
```

Models are saved to `models/` (excluded from git by `.gitignore`).
Load them in the Denoise module via the *Load Model…* button.

## Output locations

Artefacts land under `build/AuClear_artefacts/Release/<Format>/`:

| Format | Platforms | File |
|---|---|---|
| VST3 | Linux, macOS, Windows | `AuClear.vst3/` |
| AU | macOS only | `AuClear.component/` |
| LV2 | Linux, macOS, Windows | `AuClear.lv2/` |
| Standalone | all | `AuClear` / `AuClear.app` / `AuClear.exe` |

With `COPY_PLUGIN_AFTER_BUILD TRUE` (set in CMakeLists) they are also
copied to your system plugin directories.

## Validate with pluginval

```bash
pluginval --strictness-level 10 --validate \
  build/AuClear_artefacts/Release/VST3/AuClear.vst3
```

CI runs this on all three platforms automatically on every push.

## Format check

```bash
find src -name '*.cpp' -o -name '*.h' | xargs clang-format --dry-run --Werror
# auto-fix:
find src -name '*.cpp' -o -name '*.h' | xargs clang-format -i
```

Style is JUCE-flavoured Allman — see `.clang-format` at the repo root.

## Phase 1 exit criteria

- [x] Builds on Linux, macOS, Windows
- [x] VST3 passes pluginval at strictness 10 on all three platforms
- [x] Lock-free rack engine: add / reorder / remove modules with no glitches
- [x] GainModule active end-to-end: knob moves, audio processed
- [x] I/O meters update in real time (30 Hz, peak hold)
- [x] State saves and restores correctly across plugin reload

## Phase 2 exit criteria

- [x] 8 classic DSP modules: ParametricEQ, Gate, Compressor, Limiter, Reverb, Delay, Saturator, Utility
- [x] All modules accessible via categorized "+ Add" popup menu
- [x] State save/restore for all module types
- [x] EQ curve display — live analytic frequency response, 30 Hz repaint
- [x] GR meters on Gate, Compressor, Limiter
- [x] Spectrum analyzer in Inspector panel (FFT, log-freq, peak-hold)
- [x] LUFS metering (ITU-R BS.1770-4): M/S/I + true-peak in meter bridge
- [x] Limiter reports PDC latency via latencySamples()

## Phase 3 exit criteria

- [x] `OnnxSession` wraps ORT with zero dynamic allocation on the audio thread
- [x] `ReBlocker` adapts variable host buffer sizes to the model's fixed frame size
- [x] `AIEngine` resamples to/from 48 kHz, runs per-channel inference, maintains dry delay for "listen to removed"
- [x] `DenoiseModule` reports latency via `latencySamples()` — PDC updated automatically
- [x] `HumRemoverModule` — adaptive notch cascade up to 8 harmonics (50/60 Hz)
- [x] CPU load fraction exposed on `AIEngine::cpuLoad()`, shown in DenoisePanel at 4 Hz
- [x] Build with ORT enabled: zero warnings, zero errors; ORT `.so` deployed next to VST3/Standalone
- [x] Test model generation script (`tools/make_test_model.py`) — identity + noise-gate

**Model sourcing:** load any `.onnx` model with a single `input` tensor `[1, N]` → `output` `[1, N]`
at 48 kHz.  Frame size `N` is discovered at runtime.  Use `tools/make_test_model.py` for synthetic
test signals; DeepFilterNet-compatible exports (when available) drop directly into `models/`.

Next: **Phase 4 — Offline AI & Stems** — see [07-roadmap.md](07-roadmap.md).
