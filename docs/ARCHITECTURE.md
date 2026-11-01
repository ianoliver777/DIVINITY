# DIVINITY — Engineering Notes (Phase 1)

TRIPLE SEVEN AUDIO · v0.1.0 · Phase 1 "Core Foundation"

---

## 1. Project audit (starting point)

- **Existing codebase:** none. DIVINITY was started from an empty folder; nothing was imported from earlier TRIPLE SEVEN AUDIO plugins.
- **Dependencies:** JUCE 8.0.4 (fetched automatically by CMake, or used from a local `./JUCE` checkout), CMake ≥ 3.22, a C++17 compiler. Inter font (SIL Open Font License) embedded for the UI. No other third-party code.
- **Implemented in Phase 1:** see §4. **Not implemented yet:** everything listed under Phase 2–4 in §3. Those systems are not shown as working controls in the UI — SOUL and LAB are visibly marked "PHASE 2 / PHASE 3" and disabled.

---

## 2. Architecture

```
Source/
├─ Parameters.h/.cpp        Parameter layout (stable IDs), enums, value formatting, help text
├─ PluginProcessor.h/.cpp   AudioProcessor: owns APVTS, engine, FX, presets; state save/restore
├─ DSP/
│  ├─ DSPCore.h             Envelope, LFO, ZDF SVF, ZDF ladder, comb, noise, PolyBLEP, RNG
│  ├─ Wavetables.h/.cpp     Factory wavetable bank, band-limited mipmaps (shared across instances)
│  ├─ SynthEngine.h/.cpp    Voices, unison, glide/legato, mod matrix, sample-accurate MIDI
│  └─ FXChain.h/.cpp        Saturation (ADAA), chorus, delay, reverb, limiter, master
├─ Presets/
│  ├─ PresetManager.h/.cpp  Versioned XML presets, user folder, import/export, favourites, dirty flag
│  └─ FactoryPresets.cpp    Factory library defined as overrides on top of defaults
├─ PluginEditor.h/.cpp      Window shell: top bar, side nav, workspaces, scaling, undo/redo
└─ UI/
   ├─ DivinityLookAndFeel   Palette, typography, knob/combobox/menu/tooltip rendering
   ├─ Components            Knob, Selector, ChoiceBox, NumberBox, TabRow, displays, meters
   ├─ Panels                Oscillators, filter, envelope, LFO, matrix, macros, FX, global, keyboard
   └─ CreateView            CREATE workspace (preset browser, scope, macros, 8 musical controls)
Tests/TestMain.cpp          Automated test runner (72 checks) + offscreen UI snapshots + WAV demos
```

### Key decisions (and why)

| Decision | Choice | Reason / trade-off |
|---|---|---|
| Parameter system | One `AudioProcessorValueTreeState`, ~164 params, IDs never renamed | Host automation + session recall. New params can be appended; old sessions fill missing params with defaults (tested). |
| Engine ↔ parameters | Engine reads atomics into a plain `EngineSnapshot` every 32-sample control block | No locks/allocation on the audio thread; UI and DSP stay fully decoupled (Rule 8). |
| Control rate | 32-sample blocks, all modulated values ramped linearly inside the block | Click-free modulation at a fraction of per-sample cost. Envelopes and amp run per sample. |
| Oscillator slots | Each slot selects an **engine** (`Wavetable`, `Analog`) | The slot model is engine-agnostic so Granular / Spectral / Sample / Additive engines slot in later (Rule 5). |
| Anti-aliasing | Wavetables: 11 band-limited mip levels per frame chosen by pitch. Analog: PolyBLEP saw/pulse. Saturation: 1st-order ADAA | Clean highs without oversampling latency. Wavefold is not yet oversampled (noted below). |
| Filters | Zero-delay-feedback SVF + 4-pole ZDF ladder with tanh feedback; comb | Stable under extreme resonance/drive and fast modulation (tested at 44.1 & 192 kHz). |
| Modulation | 8-slot matrix, 16 sources × 36 destinations, per-voice + global | LFO→LFO-rate routes use the previous block's value → cycles are allowed and can never blow up. |
| Voice stealing | Steals the quietest releasing voice, else the oldest; stolen voices retrigger from their current level | No clicks on steal, no "dead" notes. |
| Randomness | Every RNG is seeded per voice / per LFO | Two instances with the same state render bit-identical audio (tested) — the basis for reproducible Mutation Lab / Infinity Field seeds later. |
| Macros | TONE / MOVEMENT / TEXTURE / SPACE have built-in musical targets *and* are matrix sources | Labels always mean what they say, even on an init patch (spec §3). |
| Presets | Real-unit values (not normalised) + version number | Survives future range changes. Unknown params ignored, missing params defaulted. |
| UI scaling | Whole UI laid out at 1672×940 and transformed | Pixel-exact match to the design at any size; vector rendering stays sharp on HiDPI. |

---

## 3. Development roadmap

Each milestone ships only when its acceptance criteria pass.

**Phase 1 — Core foundation (this delivery)**
- M1 Plugin shell, parameters, state recall → *pluginval strictness 10 passes; fresh instance restores every parameter.* ✅
- M2 Wavetable + analog oscillators, unison, sub/noise → *pitch within 0.5 cent at 44.1–192 kHz.* ✅
- M3 Filters, envelopes, LFOs, mod matrix → *A/D/R timing within 0.1 ms; filters stable at max res+drive; routing tests.* ✅
- M4 Voice management, glide/legato, sustain, MIDI panic → *voice limit respected; pedal/CC120/legato tests.* ✅
- M5 FX chain + limiter → *no factory preset exceeds the ceiling; 10 s silent processing shows no denormal slowdown.* ✅
- M6 Presets, undo/redo, scalable UI matching the mockup → *user preset round-trip; offscreen UI renders.* ✅

**Phase 2 — Signature identity**
- M7 SOUL ENGINE: XY pad + editable emotion mapping groups, curves, smoothing, per-patch recall.
- M8 Divine Intervention macro with transformation presets.
- M9 Liquid Oscillator: control-point waveform editor → rendered into a user wavetable on a worker thread.
- M10 Infinity Field prototype (seeded evolving layer). M11 oversampled wavefold + quality modes.

**Phase 3 — Advanced systems** (each its own module + tests)
Granular engine → Sample engine → Spectral engine (STFT, latency reported) → DNA Fusion (spectral-envelope transfer first) → Time Fracture → Decay Designer → Parallel Realities (A/B snapshots) → Sonic Terrain → Mutation Lab (seeded) → LAB workspace.

**Phase 4 — Release**
300+ factory presets, drag-reorder FX rack + more modules (EQ, comp, phaser, shimmer…), MIDI learn, MPE, installers, code signing + notarization, host test matrix (Ableton, Logic, FL, REAPER, Studio One), docs, beta.

---

## 4. What Phase 1 actually contains

- **Oscillators A/B/C:** Wavetable engine (6 factory tables × 32 frames: Basic Shapes, Harmonic Sweep, Formant Vox, Pulse Grit, Glass Halo, Hollow Organ) with position scan + frame interpolation, or Analog engine (sine, triangle, saw, square, pulse/PWM). Per osc: unison 1–8, detune, stereo width, phase/random phase, fold, octave/semi/fine, level.
- **Sub** (sine/triangle/square, −1/−2 oct) and **Noise** (white/pink/bright/dark).
- **Filters 1 & 2:** MG Low 24 (ladder), LP12, HP12, BP, Notch, Peak, Comb; cutoff, resonance, drive, mix, key-track; serial/parallel routing; live response display.
- **Envelopes 1–3** (ENV 1 = amp) with curve and draggable display. **LFOs 1–4:** sine/square/triangle/random, rate or tempo-sync (locks to host transport), depth, phase, smooth.
- **Mod matrix:** 8 slots; sources LFO 1–4, ENV 1–3, velocity, mod wheel, aftertouch, pitch bend, note, macros 1–4.
- **Macros:** TONE, MOVEMENT, TEXTURE, SPACE.
- **FX:** saturation, chorus, tempo-synced ping-pong delay, reverb, limiter; master volume.
- **Voices:** 1–32 poly, legato mono, glide, bend range, master tune, sustain pedal, CC120/123, MIDI panic.
- **Presets:** 20 factory presets across all categories; user save/import/export; favourites; search; modified indicator.
- **UI:** CREATE + DESIGN workspaces, side nav, tooltips with plain-English help on every parameter, double-click reset, shift-drag fine adjust, right-click value entry, mod rings on modulated knobs, undo/redo (Cmd/Ctrl+Z), 60–160% scaling.

### Known limitations (honest list)
- Wavefold isn't oversampled yet → some aliasing at high fold on high notes (Phase 2, M11).
- FX order is fixed (sat → chorus → delay → reverb → limiter); drag-reorder comes with the full rack in Phase 4.
- Reverb is JUCE's Freeverb-style reverb — fine for Phase 1, will be replaced by a custom design.
- Filter display draws the analog prototype response (very close, not bit-exact above ~10 kHz).
- No MIDI learn or MPE yet. AU and Windows builds are produced by CI and haven't been run in a real DAW by us.

---

## 5. UI plan

- **Window:** 1672 × 940 design grid (matches the mockup), freely resizable at a fixed aspect ratio (55–160%), sizes 60–130% in the gear menu; scale is saved with the session.
- **Top bar:** logo · workspace switch (CREATE / DESIGN / LAB) · preset bar (prev/next, name → categorized menu, favourite, save, options) · CPU · voices · output meter · MIDI panic · settings.
- **Side nav:** PLAY → CREATE workspace. SYNTH / MOTION / FX → DESIGN and highlight their panels. SOUL / LAB disabled until their phase ships.
- **Workspaces share one state** — switching never touches the sound.
- **Typography:** Inter Regular/Medium/SemiBold; tracked uppercase section titles; 10.5–14 px labels.
- **Components:** one Knob class everywhere (identical behaviour in every workspace), Selector, ChoiceBox, NumberBox, TabRow, PowerButton, IconButton, displays.
- **Palette:** #090A0C background, #111317/#16181D panels, #2A2E36 borders, #F1F1F1 / #9A9DA5 text, violet-white accent #C9B8F2.
