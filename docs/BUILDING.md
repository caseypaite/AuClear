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

Next: **Phase 3 — AI stem separation** — see [07-roadmap.md](07-roadmap.md).
