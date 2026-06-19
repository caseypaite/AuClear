# 02 — Rack Modules

Every module implements the `RackModule` interface (see
[01-architecture.md](01-architecture.md#3-the-rack-engine)) and ships with
bypass, wet/dry, a compact "card" view, and an expanded panel. Below: purpose,
key parameters, visualization, real-time safety, and latency.

Legend — **RT**: safe to run in the audio thread · **OFF**: offline/background
job · **Lat**: typical added latency.

---

## A. AI Repair modules

### A1. AI Denoise — `AIDenoise` · RT · Lat: ~20–40 ms
Neural broadband noise reduction (DeepFilterNet; RNNoise as low-CPU fallback).
- **Params:** Reduction amount (dB / %), Model size (Nano/Small/Full → CPU
  trade-off), Noise floor sensitivity, Attack/Release of the gain mask, Dry/Wet,
  Output trim.
- **Viz:** before/after spectrogram split, live "noise reduced" meter, residual
  (removed-noise) monitor toggle.
- **Notes:** internal re-blocking to native frame size; latency reported.

### A2. AI Dereverb — `AIDereverb` · RT (light) / OFF (heavy) · Lat: ~20–40 ms
Removes room reverb / reflections.
- **Params:** Reverb reduction amount, Early/Late balance, Preserve direct
  level, Sensitivity, Dry/Wet.
- **Viz:** decay-tail meter (estimated RT60 reduction), before/after spectrogram.
- **Two engines:** a real-time light mode and an offline high-quality pass for
  difficult material.

### A3. Hum & Buzz Remover — `HumRemover` · RT · Lat: ~0
50/60 Hz mains hum + harmonics, plus broadband whine.
- **Params:** Base frequency (50/60/auto-detect), # harmonics, Notch depth/Q,
  Adaptive tracking on/off.
- **Viz:** spectrum with detected hum partials highlighted as movable markers.

### A4. De-Clip — `DeClip` · OFF · Lat: n/a (render)
Reconstructs clipped waveform peaks.
- **Params:** Clip threshold (auto/manual), Reconstruction strength, Makeup.
- **Viz:** waveform with clipped regions shaded; before/after overlay.

### A5. De-Click / De-Crackle — `DeClick` · RT (clicks) / OFF (crackle) · Lat: small
Removes impulsive noise: vinyl clicks, mouth clicks, digital pops.
- **Params:** Sensitivity, Click width, Crackle reduction, Mode (clicks/crackle/both).
- **Viz:** event histogram (clicks removed/sec), waveform markers.

### A6. Spectral Repair — `SpectralRepair` · OFF · Lat: n/a (render)
Interpolate/attenuate selected time-frequency regions (RX-style). Editor works on
a captured region's spectrogram with marquee selection.
- **Params:** Mode (Attenuate / Replace / Interpolate), Strength, Frequency
  smoothing.
- **Viz:** **interactive spectrogram editor** (the centerpiece repair surface).

---

## B. EQ & tonal

### B1. Parametric EQ — `ParametricEQ` · RT · Lat: 0 (min-phase) / ~few ms (linear-phase)
Up to ~12 bands; bell/shelf/HP/LP/notch; optional linear-phase mode.
- **Params per band:** enable, type, freq, gain, Q; global: phase mode,
  analyzer pre/post, auto-gain.
- **Viz:** **draggable EQ curve** over a live spectrum analyzer; per-band nodes,
  solo-band listen, real-time response.

### B2. Dynamic EQ — `DynamicEQ` · RT · Lat: small (lookahead)
Bands whose gain reacts to level (de-mud, de-harsh, resonance taming).
- **Params per band:** freq, Q, threshold, range, attack/release, mode
  (compress/expand), external sidechain.
- **Viz:** EQ curve with animated band movement + per-band GR readout.

---

## C. Dynamics

### C1. Gate / Expander — `Gate` · RT · Lat: 0–small
- **Params:** threshold, range, ratio (expander), attack/hold/release,
  hysteresis, sidechain HP/LP, lookahead.
- **Viz:** transfer curve, GR meter, threshold vs input level history.

### C2. Compressor — `Compressor` · RT · Lat: 0–small
Feed-forward, program-dependent; selectable character (clean/opto/VCA/FET).
- **Params:** threshold, ratio, knee, attack, release, makeup, mix (parallel),
  sidechain filter + external SC, auto-release.
- **Viz:** transfer curve with live operating-point dot, GR meter, GR-over-time.

### C3. Multiband Compressor — `MultibandComp` · RT · Lat: small (crossovers)
3–5 bands with per-band dynamics.
- **Params:** crossover freqs, per-band threshold/ratio/attack/release/makeup,
  per-band solo/bypass.
- **Viz:** band-split spectrum, per-band GR bars.

### C4. De-Esser — `DeEsser` · RT · Lat: small (lookahead)
Sibilance control (split-band or dynamic-EQ style).
- **Params:** frequency/range, threshold, mode (wideband/split), listen.
- **Viz:** sibilance band highlight on spectrum + GR meter.

### C5. Brickwall Limiter — `Limiter` · RT · Lat: ~1–5 ms (lookahead)
True-peak limiting for final output.
- **Params:** ceiling (dBTP), threshold/gain, release, lookahead, true-peak
  on/off, dither.
- **Viz:** GR meter, **true-peak + LUFS readout**, gain-reduction histogram.

---

## D. Creative / color

### D1. Saturator — `Saturator` · RT · Lat: 0 (×oversampled)
Tape/tube/transistor harmonic coloration with oversampling.
- **Params:** drive, type, bias/asymmetry, tone, mix, oversampling factor.
- **Viz:** input/output transfer curve + harmonic spectrum.

### D2. Reverb — `Reverb` · RT · Lat: 0
Algorithmic (FDN/plate/hall/room) for adding space back after cleanup.
- **Params:** size, decay, pre-delay, damping HF/LF, width, early/late mix,
  mod, dry/wet.
- **Viz:** decay-envelope curve + frequency-dependent decay display.

### D3. Delay — `Delay` · RT · Lat: 0
Stereo / ping-pong / tempo-synced.
- **Params:** time (ms or note value), feedback, ping-pong, filter in
  feedback path, ducking (sidechain), width, dry/wet.
- **Viz:** tap/echo timeline, feedback decay display.

### D4. Stereo Width / Utility — `StereoWidth` / `Utility` · RT · Lat: 0
- **Params:** width (M/S), pan, mono-below-frequency, channel swap, polarity,
  gain, mono-sum check.
- **Viz:** goniometer + correlation meter.

---

## E. AI Remix modules

### E1. Stem Separator — `StemSeparator` · OFF · Lat: n/a (render)
Runs Demucs on a captured region to produce 4 stems
(**vocals / drums / bass / other**). This is a *job launcher*, not a live
processor — see [03-ai-and-stems.md](03-ai-and-stems.md).
- **Params:** Source select (armed capture / host loop region), Quality (model
  variant + overlap), Shift/segment settings.
- **Viz:** waveform with capture region, progress bar, per-stem result lanes.

### E2. Stem Mixer — `StemMixer` · RT (playback) · Lat: 0
Plays back the four separated stems summed in place of the dry signal.
- **Params per stem:** gain, mute, solo, pan; optional per-stem FX send into a
  sub-rack; master stem blend vs. dry.
- **Viz:** 4 channel strips with meters; waveform lanes color-coded per stem.

---

## F. Utility — `Utility` · RT · Lat: 0
Input/output gain, phase, channel ops, A/B trim, mono check. Always available as
a chain anchor.

---

## Per-module common UI affordances
- Drag handle (reorder), enable/bypass toggle, collapse/expand, **wet/dry knob**,
  preset dot, output-trim, and a **mini-viz** shown on the collapsed card so the
  whole rack is readable at a glance.
- "Solo module" (audition just this module's contribution) and "Listen to
  removed" (for repair modules — hear what's being taken out).
