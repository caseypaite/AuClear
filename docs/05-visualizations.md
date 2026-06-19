# 05 — Visualizations & Metering

Every graph below is fed from the audio thread through **lock-free SPSC FIFOs**
and drawn on a GUI timer (~30–60 Hz). The audio thread only *pushes* cheap data
(samples, FFT magnitudes, meter scalars); all rendering happens on the message
thread. Heavy panels (spectrogram, analyzer) can opt into OpenGL.

| # | Visualization | Where | DSP behind it | Update path |
|---|---|---|---|---|
| 1 | **Spectrum analyzer** | Inspector, EQ panels | windowed FFT (Hann), log-freq, magnitude smoothing, peak-hold | FFT frames → FIFO → GUI |
| 2 | **Spectrogram (before/after)** | Repair panels, full-screen | STFT magnitude → color map (viridis/inferno), scrolling waterfall | STFT cols → FIFO → texture |
| 3 | **EQ response curve** | EQ panels | analytic biquad/FIR transfer function evaluated over freq grid | param change → recompute |
| 4 | **Compressor transfer curve** | Comp/MB panels | static gain curve from thresh/ratio/knee + live operating-point dot | params + level meter |
| 5 | **Gain-reduction meter** | all dynamics | per-block GR in dB, peak-hold + ballistics | scalar → FIFO |
| 6 | **GR-over-time history** | Comp/Limiter | ring buffer of GR vs time | scalar stream → FIFO |
| 7 | **Loudness suite (LUFS)** | meter bridge, Limiter | ITU-R BS.1770-4 / EBU R128: Momentary, Short, Integrated, LRA | libebur128 → FIFO |
| 8 | **True-peak meter** | meter bridge, Limiter | 4× oversampled inter-sample peak (dBTP) | scalar → FIFO |
| 9 | **Goniometer (vectorscope)** | Inspector, Utility | L/R → mid/side X-Y scatter | decimated samples → FIFO |
| 10 | **Stereo correlation** | meter bridge | normalized L·R correlation coefficient | scalar → FIFO |
| 11 | **Waveform + stem lanes** | Stem Remix | min/max peak bins of captured/stem buffers | offline → static draw |
| 12 | **Noise-reduction / "removed" monitor** | AI Denoise/Dereverb | energy of (input−output), reduction in dB; optional audible solo | scalar + audio tap |
| 13 | **Hum-partial markers** | Hum Remover | detected mains fundamental + harmonics on spectrum | detector → markers |
| 14 | **Click/event histogram** | De-Click | events removed per second | counter → FIFO |
| 15 | **Decay / RT60 display** | Reverb, Dereverb | energy-decay curve / estimated reverberation time | analysis → GUI |
| 16 | **Harmonic spectrum** | Saturator | FFT of output showing added harmonics vs input | FFT → FIFO |
| 17 | **I/O level meters** | meter bridge, Utility | RMS + peak with ballistics, peak-hold | scalar → FIFO |

---

## Implementation notes

### FFT / spectrum
- `juce::dsp::FFT` or pffft; Hann window; 2048–8192 size selectable (freq vs time
  resolution). Magnitude in dB, log frequency axis, configurable smoothing and
  peak-hold decay. **Pre/post overlay** so users see what a module changed.

### Spectrogram
- Scrolling STFT written to a texture/`Image`; color map applied per bin.
- **Before/after** modes: side-by-side, A/B wipe, or difference. This is the
  single most useful view for verifying AI cleanup (you literally see noise
  disappear). Drives the Spectral Repair marquee editor too.

### Loudness (BS.1770-4 / EBU R128)
- K-weighting filter → mean-square → gating → Momentary (400 ms), Short (3 s),
  **Integrated** (gated, whole-program), **LRA** (loudness range).
- True peak via ≥4× oversampling for inter-sample peaks (dBTP).
- Targets/presets per delivery spec: **-14 LUFS** (streaming/music),
  **-16 LUFS** (podcast), **-23 LUFS / EBU R128** (broadcast/video),
  **-19/-24** regional TV. Shown as a target line + "distance to target."
- Use **libebur128** (MIT) or a vetted custom implementation.

### Dynamics curves
- Transfer curve computed analytically from current params; a **live operating
  point** dot shows the current input→output mapping using the level meter — this
  makes compression legible at a glance.

### Goniometer & correlation
- Lissajous X-Y (mid/side rotated 45°) from decimated stereo samples; correlation
  coefficient as a -1…+1 bar (warns on mono-compatibility issues — important for
  video/broadcast deliverables).

### Performance discipline
- Audio thread never allocates or draws. It pushes into pre-allocated FIFOs and
  drops data if the GUI falls behind (visual smoothness ≠ audio correctness).
- GUI repaints are throttled and clipped to dirty regions; spectrogram/analyzer
  can use OpenGL on a dedicated context.
- All meter ballistics (attack/release/peak-hold) standardized in one
  `MeterSource` helper so every meter behaves consistently.
