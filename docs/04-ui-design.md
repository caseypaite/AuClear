# 04 — UI Design

## 1. Design language

Professional, dark, vector, resizable — peer group: FabFilter, iZotope RX,
Sonible, Accentize. Clarity and live visual feedback over skeuomorphism.

- **Palette (dark, default):**
  - Background: `#16181D` (charcoal) / panels `#1E2128`
  - Surface lines/dividers: `#2A2E37`
  - Text primary `#E8EAED`, secondary `#9AA0AB`
  - **Brand accent:** AuClear teal/cyan `#28E0C8` (highlights, active states)
  - Per-category accents: Repair = teal, EQ = amber, Dynamics = violet,
    Creative = coral, Remix = blue. Each module card carries its accent.
  - Meters: green→amber→red; spectrogram: viridis/inferno color map.
- **Light theme** offered for video editors who work on bright timelines.
- **Typography:** Inter (UI), JetBrains Mono (numeric readouts). All values
  editable by double-click + scroll-to-adjust.
- **Rendering:** JUCE vector graphics + custom `LookAndFeel`; fully resizable
  with min/max and DPI scaling. Optional GPU rendering (OpenGL) for the
  spectrogram/analyzer panels.
- **Accessibility:** keyboard focus + JUCE accessibility handlers, colorblind-safe
  meter alternative (pattern + value), large-text mode, tooltips everywhere.

---

## 2. Top-level layout

```
┌───────────────────────────────────────────────────────────────────────────────┐
│ AuClear ▸ │ ◀ Preset: "Podcast Cleanup" ▶ │ A│B  ↺ undo  │ ⏻ bypass │ ⚙ │ CPU 12% │ ← header
├──────────────┬────────────────────────────────────────────┬───────────────────┤
│              │                                            │                   │
│  RACK        │                MAIN STAGE                   │   INSPECTOR /     │
│  (chain)     │   (expanded module panel OR stem view OR    │   ANALYZER        │
│              │    big spectrogram)                         │   (context panel) │
│  ┌────────┐  │                                            │                   │
│  │AIDenoise│ │   ┌──────────────────────────────────────┐  │  spectrum +       │
│  ├────────┤  │   │     selected module's full UI +       │  │  goniometer       │
│  │Dereverb│  │   │     its primary visualization         │  │  + correlation    │
│  ├────────┤  │   └──────────────────────────────────────┘  │                   │
│  │  EQ    │ ◄┼── selected                                  │                   │
│  ├────────┤  │                                            │                   │
│  │ Comp   │  │                                            │                   │
│  ├────────┤  │                                            │                   │
│  │Limiter │  │                                            │                   │
│  ├────────┤  │                                            │                   │
│  │ + Add  │  │                                            │                   │
│  └────────┘  │                                            │                   │
├──────────────┴────────────────────────────────────────────┴───────────────────┤
│  IN ▮▮▮▮▯  [LUFS-M -18  S -16  I -14  LRA 6  TP -1.0]  ▮▮▮▮▮ OUT   corr +0.8  │ ← meter bridge
└───────────────────────────────────────────────────────────────────────────────┘
```

Three columns + header + master meter bridge:
- **Header:** brand menu, preset browser (◀ ▶ + dropdown), **A/B compare**, undo,
  global bypass, settings (AI engine, oversampling, theme), live CPU + latency.
- **Rack column (left):** the stackable chain — the heart of the product.
- **Main stage (center):** whatever's selected — a module's full panel, the
  Stem Remix view, or a full-screen spectrogram.
- **Inspector (right):** always-on analyzer (spectrum + goniometer + correlation),
  collapsible.
- **Meter bridge (bottom):** input/output meters, full **LUFS/true-peak loudness
  suite**, stereo correlation.

---

## 3. The Rack (signal chain)

```
┌─────────────────────────────┐   each card:
│ ⠿  ◉ AI Denoise        ⌄ ✕ │   ⠿ drag handle  ◉ enable  ⌄ collapse  ✕ remove
│ ░░▒▒▓▓ mini-spectrogram     │   • mini-viz so the chain is readable collapsed
│ Amount ●────  Mix ●──       │   • 1–2 key knobs inline
└─────────────────────────────┘
┌─────────────────────────────┐
│ ⠿  ◉ Parametric EQ     ⌄ ✕ │
│  ╱╲___ mini EQ curve         │
└─────────────────────────────┘
            ⋮
        ┌─────────┐
        │ + Add ▾ │  → categorized module palette (Repair/EQ/Dynamics/Creative/Remix)
        └─────────┘
```

- **Drag to reorder** (lock-free apply on the audio thread — see architecture).
- **Click a card** → opens its full panel in the main stage.
- **Per-card:** enable/bypass, wet/dry, collapse, remove, "solo module",
  "listen to removed" (repair modules).
- **+ Add** opens a searchable, categorized palette. Drag a module into any chain
  position.
- **Signal flows top→bottom**; an optional parallel/bus mode (later phase) lets a
  module run in parallel with a mix knob.

---

## 4. Module panel (main stage) — example: Compressor

```
┌──────────────────────────────────────────────────────────────────┐
│ Compressor                              [opto│VCA│FET│clean]  ⓘ   │
│                                                                  │
│   transfer curve            GR meter        GR over time          │
│   ┌───────────────┐         ┌──┐            ┌──────────────────┐   │
│   │        ╱      │         │▓▓│            │  ▁▂▅▇▅▂▁▂▄▆▄▂     │   │
│   │      ╱●       │  ◄live  │▓▓│ -4 dB      │                  │   │
│   │   __╱  op pt  │         │▓▓│            └──────────────────┘   │
│   └───────────────┘         └──┘                                  │
│                                                                  │
│   Thresh ◉   Ratio ◉   Knee ◉   Attack ◉   Release ◉   Makeup ◉    │
│   Mix ◉      SC filter ◉   [ext SC ▾]   [auto-release ☑]           │
└──────────────────────────────────────────────────────────────────┘
```

Each module's panel pairs its **controls** with its **primary visualization**
(catalogued in [05-visualizations.md](05-visualizations.md)).

---

## 5. Stem Remix view (main stage)

```
┌──────────────────────────────────────────────────────────────────┐
│ Stem Remix          Source: [armed capture ▾]   Quality: [Best ▾] │
│                                                                  │
│   waveform / region                                              │
│   ┌──────────────────────────────────────────────────────────┐   │
│   │ ▁▂▅▇▆▅▇█▆▅▃▂▅▇█▇▅▃▂▁  ◀── capture region ──▶              │   │
│   └──────────────────────────────────────────────────────────┘   │
│                          [ ▶ Separate ]   ▓▓▓▓▓▓▓░░░ 72%  ⏱ 0:14 │
│                                                                  │
│  ┌── VOCALS ──┬── DRUMS ──┬── BASS ──┬── OTHER ──┐                │
│  │  S  M      │  S  M     │  S  M    │  S  M     │  S=solo M=mute │
│  │  ▮         │  ▮        │  ▮       │  ▮        │                │
│  │  ▮  +0.0   │  ▮ -3.0   │  ▮ +1.0  │  ▮  0.0   │  ← gain faders │
│  │  ◉ pan     │  ◉        │  ◉       │  ◉        │                │
│  │ [→FX]      │ [→FX]     │ [→FX]    │ [→FX]     │  per-stem send │
│  └────────────┴───────────┴──────────┴───────────┘                │
│   Dry blend ●───────                                              │
└──────────────────────────────────────────────────────────────────┘
```

Color-coded stem lanes, 4 channel strips, progress + ETA during separation,
per-stem solo/mute/gain/pan and an FX send into a sub-rack.

---

## 6. Full-screen spectrogram / repair surface

A toggle expands a large **before/after spectrogram** across the main stage —
essential for cleanup work and for the Spectral Repair marquee editor. Split or
A/B wipe between dry and processed.

---

## 7. Interaction principles
- **Everything is draggable/scrubbable:** EQ nodes, faders, knobs (scroll to
  fine-tune, double-click to type a value, ⌥/Alt for fine).
- **Always-on feedback:** analyzers update live; bypass is click-to-compare.
- **A/B + undo** at the header; per-module presets via the preset dot.
- **Non-destructive:** offline renders (stems/repairs) are cached artifacts,
  re-editable, never overwrite source.
- **Resizable + remembers** last size, view, and inspector state per session.
