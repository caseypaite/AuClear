# Building AuClear (Phase 0)

Phase 0 is an empty passthrough plugin that proves the cross-format build.

## Prerequisites
- **CMake ≥ 3.22** and a **C++20** compiler
  (Xcode/Clang on macOS, MSVC 2022 on Windows, GCC/Clang on Linux)
- Git (JUCE is fetched automatically via `FetchContent`)
- **Linux only**, JUCE system deps:
  ```bash
  sudo apt-get install -y libasound2-dev libjack-jackd2-dev ladspa-sdk \
    libfreetype-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
    libxinerama-dev libxrandr-dev libxrender-dev libglu1-mesa-dev mesa-common-dev
  ```

## Configure & build
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```
First configure clones JUCE (tag `8.0.4`, override with `-DAUCLEAR_JUCE_TAG=...`,
or set `-DJUCE_SOURCE_DIR=/path/to/JUCE` to use a local copy offline).

## Targets produced
| Target | Platform | Output |
|---|---|---|
| VST3 | all | `AuClear.vst3` |
| AU | macOS | `AuClear.component` |
| LV2 | all | `AuClear.lv2` |
| Standalone | all | `AuClear` app (quickest way to eyeball the UI) |

Outputs land under `build/AuClear_artefacts/<Config>/<Format>/`. With
`COPY_PLUGIN_AFTER_BUILD` they're also copied to your user plugin folders.

## Validate
```bash
pluginval --strictness-level 10 --validate \
  build/AuClear_artefacts/Release/VST3/AuClear.vst3
```
CI (`.github/workflows/build.yml`) runs this on Linux/macOS/Windows automatically.

## Exit criteria (Phase 0)
- [ ] Builds on all three OSes
- [ ] VST3/AU/LV2 load in a host (Reaper, Logic, Ardour, DaVinci Resolve)
- [ ] Passes `pluginval` at strictness 10
- [ ] State save/restore round-trips (empty state)

Next: **Phase 1 — rack engine + UI shell** (see [07-roadmap.md](07-roadmap.md)).
