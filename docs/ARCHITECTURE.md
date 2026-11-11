# DIVINITY — Engineering Notes (Phase 1)

TRIPLE SEVEN AUDIO · v0.3.0 · core synth + SOUL ENGINE + new UI with full-page tabs + Guardian Angel

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
│  ├─ SoulEngine.h/.cpp     SOUL ENGINE emotion mapping + Divine Intervention (control-rate offsets)
│  ├─ DSPCore.h             Envelope, LFO, ZDF SVF, ZDF ladder, comb, noise, PolyBLEP, RNG
│  ├─ Wavetables.h/.cpp     Factory wavetable bank, band-limited mipmaps (shared across instances)
│  ├─ SynthEngine.h/.cpp    Voices, unison, glide/legato, mod matrix, sample-accurate MIDI
│  ├─ FxCatalog.h/.cpp      The 16 rack effects: keys, names, parameters, help, default order
│  └─ FXChain.h/.cpp        Reorderable rack (lock-free order handoff, per-module bypass crossfade), limiter, master
├─ Assistant/
│  └─ GuardianBrain.h/.cpp  Guardian Angel's offline brain: knowledge base, sound recipes, ideas, safe randomiser
├─ Presets/
│  ├─ PresetManager.h/.cpp  Versioned XML presets, user folder, import/export, favourites, dirty flag
│  └─ FactoryPresets.cpp    Factory library defined as overrides on top of defaults
├─ PluginEditor.h/.cpp      Window shell: top bar, side nav, workspaces, scaling, undo/redo
└─ UI/
   ├─ DivinityLookAndFeel   Palette, typography, knob/combobox/menu/tooltip rendering
   ├─ Components            Knob, Selector, ChoiceBox, NumberBox, TabRow, displays, meters
   ├─ Panels                Oscillators, filter, envelope, LFO, matrix, macros, FX, global, keyboard
   ├─ CreateView            CREATE workspace (preset browser, scope, mini SOUL pad, Divine, macros, 8 controls)
   ├─ SoulView              SOUL page: XY pad, emotion mapping editor, Divine Intervention
   ├─ Pages                 CREATE / EFFECTS / PLAY / LAB full-page tabs, spectrum analyzer
   └─ GuardianPanel         Guardian Angel character (vector, animated), speech bubble, actions, ask box
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
- M7 SOUL ENGINE: XY pad + editable emotion mapping, curves, smoothing, per-patch recall → *neutral = bit-identical; aggressive brighter; melancholy longer tails; exclusions; click-free jumps; recall.* ✅ (v0.2.0)
- M8 Divine Intervention macro with 5 transformation modes → *0% = untouched; every mode audibly different, finite, under the ceiling.* ✅ (v0.2.0)
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

### SOUL ENGINE (v0.2.0)
- XY pad: SOFT ↔ AGGRESSIVE, MELANCHOLY ↔ EUPHORIA; centre = patch untouched (tested bit-identical).
- 10 targets: Brightness, Drive, Attack, Tail, Detune, Width, Space, Instability, Movement, Harmonics. Each has an on/off switch and a bipolar weight per axis — fully editable and saved with the patch.
- 5 mapping presets (Classic, Cinematic, Bass Pressure, Dream, Subtle); factory presets pick one by category.
- Intensity, response curve and transition smoothing (one-pole glide, 0–2 s). X, Y and Divine amount are host-automatable and are mod-matrix sources/destinations.
- Applied as control-rate offsets on top of the patch (filters, envelopes, unison, fold, LFO depth, per-voice drift, FX sends) — no allocation, no locks.

### Divine Intervention (v0.2.0)
- One 0–100% control (smoothed) with 5 modes: Ascension, Collapse, Shatter, Bloom, Abyss — each moves brightness, drive, envelope times, detune, width, space, delay, instability, movement and harmonics differently, so it reshapes the sound rather than just adding distortion.

### v0.3.0 interface
- 1536x1024 design grid (scales 50-160%), "celestial blue" theme, TRIPLE SEVEN emblem, full-page tabs: CREATE (engine), EFFECTS (chain + output + spectrum), PLAY (presets, macros, performance, keyboard), SOUL, LAB (matrix, deep osc settings, filters, LFOs, global).
- Bottom bar: settings, import, save, undo/redo, category-filtered preset navigation, voices, glide.

### Guardian Angel (v0.3.0)
- Fully offline. Answers questions from a knowledge base + every parameter's help text; builds sounds from plain-English descriptions (type + vibe words → factory base + SOUL/macro/mod adjustments); suggests one idea at a time with one-click DO IT; musically-safe randomiser (all outputs tested audible, finite and under the ceiling); explain mode (click any control); "make it brighter / wider / grittier..." tweaks. Every change is undoable.

### v1.0.0 (UI 1.0 + night mode, mod drag, drawn LFOs, segment curves, more grit)
- Skin: `ui::colours` are mutable tokens; `setNightMode()` swaps day (porcelain, default) / night (v0.7 palette), persisted in a PropertiesFile (`nightMode`). `DivinityLookAndFeel::applyPalette()` re-pushes JUCE colour IDs; editor re-tints labels.
- Mod drag: `ModHandle` (DragAndDropContainer = editor) sends `mod:<source>`; `Knob` is a DragAndDropTarget that fills (or reuses) a mod-matrix slot (amount 50 %, LFO switched on).
- New params (appended, defaults reproduce 0.9 sound exactly): `env*_a_curve/d_curve/r_curve`, `lfo*_pt1..16` + LFO shape `Custom`, `mb_low_x`, `mb_high_x`, `mb_{low,mid,high}_{gain,dthr,dratio,uthr,uamt}`; distortion types 5-11 (ADAA).
- Default UI scale 1.15.

### v0.9.1 (one preset per core sound)
- 0.9.0 drew 777 presets from ~14-18 SOURCES per category; the audio-fingerprint variety check counted reverb / LFO differences as "different", but by ear the same core sound returned every 8-10 presets. Now each SOURCE is used at most once per category (twice for a percussion sample, with different patterns), enforced both in the picker and in `buildFromRecipe`. 130 new core sounds (mostly built on the wavetable library); bank = 271.

### v0.9.0 (wave bank)
- New factory bank (`FactoryPresetsV8Recipes.inc`): REESE 120 . BASS 80 . LEAD 90 . SYNTH 90 . PAD 95 . TEXTURE 70 . SEQUENCE 90 . PERCUSSION 62 . FX 80. Each preset = SOURCE x MOVEMENT x FILTER x PLAY STYLE x SPACE (+ optional character chain). Combinations whose words wouldn't hold (a "Growling" pure sine, a filter LFO with no filter, a "Cavern" 808 that has decayed before the reverb) are excluded at draw time.
- Names are assembled from the ingredient words (layouts like "M S", "P S", "M F S"), never numbered. `claimFailures` in the tests renders each preset and checks every promising word: Reese = harmonic beating with all modulation removed; moving words = centroid / level movement on a held note; Pulsing / Gated = 10 ms gate depth; Dark = 0.6+ octave darker than the same patch unfiltered; Cathedral / Abyss / Distant / Cavern / Hall = long tail; Echo / Tunnel = delay; Riser / Downlifter / Drop / Impact judged dry (rise, pitch or level fall, early peak); arp directions, glide and swell checked against the patch. Synth-family presets also need a strong tail after release; basses need a mono fundamental (L/R correlation below 100 Hz); drums need clear transients.
- `DivinityTests <dir> --pick <inc>` renders ~4x candidates per category, drops every candidate whose name fails a claim, then keeps the most different ones (farthest-point sampling on audio fingerprints) plus ranked spares for name clashes. `--levels <inc> [manifest.csv]` level-matches to -18 dBFS RMS and writes the manifest.
- Guardian Angel resolves sounds by category + name words instead of hard-coded preset names.

### v0.8.0 (warp, user tables, MPE, MIDI learn, quality)
- WARP per oscillator (`osc*_warp`, `osc*_warp_amt`, `osc*_warp_src`; mod dests `OscAWarp..OscDWarp`). Off = the old render paths untouched (bit-identical). Any warp or an imported table goes through `SynthEngine::renderWarpedUnison`.
- Sync / FM / AM sources are rendered once per voice per block by `renderModulator` (centre pitch, pre-level, so a switched-off oscillator can still modulate) together with the sample-accurate wrap positions. Sync: slave ratio 2^(4·amt), reset on the master's wrap with a 2-point polyBLEP on the jump plus a polyBLAMP on the slope change. FM is phase modulation (depth amt²·1.5 cycles); the mip level follows the effective increment (carrier + FM sidebands, or the warp's steepest phase speed).
- Phase warps (Bend ±, Phase Dist, PWM, Mirror) use curves whose phase *speed* is continuous across the wrap — a kink on a saw edge aliases at ≈ −35 dB, the smooth curves measure ≈ −85 dB at 220 Hz. Warped oscillators run a 6 Hz DC tracker seeded with the cycle mean (no thump at note-on).
- `UserWavetableSlot` (per oscillator): WAV split into 2048-sample frames (≤256), single cycles resampled, other lengths cut into 64 frames; built into the same compact half-octave mip layout as the factory bank (`WavetableBank::layout()` / `writeCompactFrame`); lock-free pointer swap + deferred delete. "User Wavetable" is appended to the table choices.
- MPE lower zone (ch 2-16): per-channel bend / pressure / CC74 kept per channel so values sent before the note-on apply; new mod sources MPE Pressure / MPE Slide. MIDI learn: CC → parameter map applied on the audio thread, saved in the state tree (`midiMap`).
- Quality: Eco caps unison at 4; HQ runs the whole voice engine at 2x through `juce::dsp::Oversampling` (polyphase IIR, latency reported); "HQ when rendering" uses `isNonRealtime()`. Quality / MPE are global settings (`isGlobalSetting`), never stored in presets. Measured (220 Hz analog saw, warp 60%): every mode < −38 dB inharmonic in Standard, < −47 dB in HQ; extreme sync (C6, slave ≈ 11 kHz) ≈ −36 dB.

### v0.7.3 (preset variety)
- Problem: `FactoryPresetsV7` gave each category ~10 archetypes chosen by `i % 10`, so presets 1, 11, 21... were the same core patch with seeded jitter and a different FX flavour.
- Now (`FactoryPresetsV7Recipes.inc`): each category recipe has independent decks of SOURCES, MOTIONS, FILTERS, envelope/play styles and FX; each candidate draws from every deck (shuffled decks of different lengths, no fixed period), and no two share source + motion + filter. Descriptions are assembled from the chosen ingredients.
- Selection (`FactoryPresetsV7Picks.inc`): a few hundred candidates per category were rendered and fingerprinted (spectrum shape, loudness envelope, per-band movement, stereo width; level-independent), then picked greedily for maximum nearest-neighbour distance; hero names keep their character. Result: median nearest-neighbour distance up 1.5-2x per category (SUB about 6x) and 0 near-duplicates (the 0.7.2 bank had 63, 45 of them in SUB).
- Tests: `soundFingerprint` + a variety check (no near-duplicates per category) in `testFactoryPresets`; `DivinityTests <dir> --presets` runs only the preset checks.
- FX: hard ceiling after `juce::dsp::Limiter` (it can overshoot by a fraction of a dB).

### v0.7.2
- `ScopeFifo` is now a broadcast ring (one writer, any number of readers with their own cursors). Before, the scope, the CREATE spectrum and the EFFECTS analyzer shared a single-reader FIFO and whichever pulled first starved the others, which froze the EFFECTS graph.
- EFFECTS output: `StereoImager`: per-FFT-bin pan (|R|²-|L|²)/(|R|²+|L|²) on x, log frequency on y, mid/side spread as a streak and colour (cyan to violet), fading trail; correlation + width readouts.
- MIDI generator: `Midi/MidiGenerator` (seeded progressions per style, voice-led chords, chord-tone-on-strong-beat leads kept in scale, style bass patterns with approach notes, GM drum grooves with trap rolls / drill skips / swing) and `writeMidiFile` (type-1, 960 PPQ, tempo + name). `Midi/MidiPreview` auditions on the audio thread: melodic parts become MIDI into the synth, drums trigger the factory kit after the FX. UI: `UI/MidiGenPanel` (PLAY tab) with piano roll, playhead, SAVE .MID and a DRAG MIDI tile that writes a temp .mid and calls `performExternalDragDropOfFiles`.

### v0.7.1 (UI to reference)
- Theme: black-glass palette sampled from the reference (`colours` in `DivinityLookAndFeel.h`), `drawPanelBackground` (lit top rim), `drawDisplayBackground`, `strokeGlow` (halo + hot core for every trace, arc and envelope), `drawGlassControl` (rows, dropdowns, tabs; selected = cyan glow). Applies to every page.
- Header art (logo, wordmark, mountains) is `Resources/Images/header_band.png`, cut from the reference at 2x with the live areas (stats, icons, tabs) inpainted; stats, icons and tabs are drawn live on top. With Guardian Angel open the sky is mirrored to fill the extra width.
- Knobs can show values (`Knob::setValueStyle`: inline line or pill), panel header buttons are boxed `IconButton`s with real actions (copy oscillator, filter select / routing / keytrack, open envelope or LFO editor, reset, x2 times, LFO "Route" to the matrix).
- Keyboard bar: `GlassKeyboard` (glossy keys, glowing pressed keys), cylinder wheels, ±bend display + range menu.

### v0.7.0
- **Sources:** OSC D (same engine as A-C; mod destinations appended after the old ones via `oscModDest()`), unison BLEND, and a **Sampler** (`DSP/Sampler`): 26 factory one-shots / loops synthesized at startup + user import (WAV/AIFF/FLAC, ≤ 30 s) published to the audio thread with an atomic pointer swap; the old buffer is freed on the message thread by a timer. The user file path saves with sessions and presets.
- **Wavetables:** 82 generated library tables (`DSP/WavetableLibrary.cpp`) on top of the 6 originals = 88. Library tables use compact int16 storage with 21 half-octave mip levels and Hermite read-out (measured alias −78 to −88 dB, same as the originals).
- **Modulation:** ENV 4, LFO power switches (`lfoN_on`, default off; legacy sessions/presets without the switch load with all LFOs on so they sound unchanged), Saw Up / Saw Down LFO shapes, RANDOM source, 16-step sequencer (`DSP/StepSequencer`) in TRIGGER mode (rewrites MIDI: holds a key → host-tempo loop with level, ratchet, gate and pitch per step, swing, triplet/dotted rates) or MODULATE mode (STEP mod source). Macros 5-8: CHARACTER (saturation), WIDTH, PUNCH (transient), AIR (high shelf) — hard-wired to FX plus free for routing.
- **Filters:** Low Pass 24, High Pass 24, Comb −, Formant appended to the type list.
- **UI:** new shell (`PluginEditor`, `UI/CreatePageV7.cpp`, `UI/BrowserPage.cpp`): header with logo / wordmark / CPU-RAM-VOICES, six tabs, preset strip, ENGINE list, per-source panel, filter / amp / LFO cards, 8 macros, ENV/LFO/STEP/RANDOM/MATRIX editor tabs, mod matrix, effects chain, output spectrum, global keyboard bar. EFFECTS / PLAY / SOUL / LAB pages are hosted unchanged.
- **Presets:** `FactoryPresetsV7.cpp` builds 777 presets from per-category archetypes × seeded variation × effect flavours (Wide, Dirty, Space, Lo-Fi, Moving, Pumping, OTT, Shimmer, Tape, Gritty, Dreamy, Clean). A final pass turns each LFO on only if a mod slot uses it. Loudness trims are measured from renders (`FactoryPresetsV7Levels.inc`). Tests check counts per category, LFO power vs routing, and that every PERCUSSION preset is a TRIGGER loop.

### v0.6.0 — Sound Engine 2.0
Full write-up, audit and before/after measurements: [`docs/SOUND_ENGINE_2.md`](SOUND_ENGINE_2.md).
- **Engine switch** (`engine_mode`, LAB → GLOBAL "1.0 / 2.0"): Divine 2.0 is the default for new instances and factory presets. Sessions and user preset files saved before 0.6 have no `engine_mode` and load on **Classic 1.0**, which is measured to reproduce v0.5 exactly (`DivinitySoundLab --classic`).
- **Oscillators:** half-octave wavetable mipmaps (Classic reads the even levels = the old tables, bit-identical); analog saw/square/triangle/pulse read exact Fourier-series tables, pulse = difference of two band-limited saws (alias-free PWM); first-order ADAA wavefold; BLAMP triangle sub.
- **Unison / organic motion:** irregular detune spacing, per-unison-voice drift, whole-voice pitch + cutoff wander and per-note variation, all scaled by **Analog Drift** (`analog`, LAB → GLOBAL "DRIFT"). 0% = static and deterministic.
- **Filters:** SVF band-pass integrator softly saturated (self-limiting resonance, warmer drive), loudness-compensated filter drive, stronger ladder passband compensation, velocity → brightness (±0.6 oct around velocity 0.8).
- **FX:** 8-line modulated FDN reverb with pre-delay (`reverb_predelay`), early reflections, input + in-loop diffusion and RT-matched damping; 3-voice-per-side dimension chorus (true stereo); delay feedback high-passed and softly saturated. Classic mode keeps the old FX.
- **Presets:** finishing pass sets drift, pre-delay, mod wheel and aftertouch routes per category; wave/neogrime level trims re-measured.
- **Tools:** `DivinitySoundLab` measures aliasing, top-end, unison width/mono-compat/motion, filter levels, reverb smoothness/density/decay/level, chorus width, CPU, and renders every factory preset.

### v0.5.0
- **Arpeggiator** (`DSP/Arpeggiator`): rewrites the MIDI buffer before the engine. Up, Down, Up/Down, Down/Up, Random (no back-to-back repeats), As Played, Chord; 12 synced rates; 1-4 octaves; gate; swing; latch. Follows the host grid while the transport plays (starts on the next step), free-runs from the first key otherwise. Fixed-size storage, no allocation on the audio thread. Turning it off hands held keys back to the synth so nothing hangs.
- **Voice modes:** Poly, Legato, Mono (appended so old sessions keep their meaning). Mono retriggers envelopes from their current level (click-free); Legato slides without retriggering.
- **Layout:** tabs row, then the preset bar, both global. Guardian Angel collapses: the editor switches between 1536 and 1276 base width at the same scale. Default scale is now 100%.
- **Presets (replaced in 0.7):** `FactoryPresetsWave.cpp` built 200 sounds from 23 archetypes plus named traits; every trait is an audible parameter change and the description/tags are generated from the same traits. Levels are trimmed from measured renders (`FactoryPresetsWaveLevels.inc`) to a common peak.

### FX rack (v0.4.0)
- 16 modules in `FxCatalog`; each has a stable key (`eq`, `comp`, `sat`, `dist`, `crush`, `trans`, `mb`, `chorus`, `flanger`, `phaser`, `trem`, `ring`, `fshift`, `delay`, `reverb`, `stereo`) and stable parameter IDs. Older parameter IDs (sat/chorus/delay/reverb) are unchanged, so v0.3 sessions load.
- The order lives as the `fxOrder` property on the APVTS state (undoable, saved in sessions and preset files; factory presets use the default order). Bad or partial orders are repaired to a full permutation.
- The audio thread picks up a new order with a try-lock and never waits. Every module crossfades in and out (~20 ms) so switching mid-note doesn't click. Modules that are off cost nothing.
- Distortion uses first-order ADAA shapers; the frequency shifter is a Hilbert-pair design; the multiband is a 3-band Linkwitz-Riley up/down compressor.

### Known limitations (honest list)
- Wavefold uses first-order ADAA in 2.0 (not oversampled): extreme fold on top-octave notes still leaves some residue (≈ −55 dB at fold 0.9, note 100).
- Classic 1.0 keeps the original Freeverb-style reverb and mono chorus on purpose (old sessions must not change).
- Not yet: spectral warp, comb/formant filter models beyond the existing set, per-oscillator filter routing. Hard sync pushed to the top octave (slave above ~10 kHz) still leaves some residue in Standard quality (≈ −36 dB); HQ helps.
- Filter display draws the analog prototype response (very close, not bit-exact above ~10 kHz).
- MPE is lower-zone only (no upper zone / MCM auto-config yet). AU and Windows builds are produced by CI and haven't been run in a real DAW by us.
- Windows CI earlier reported pluginval "crashes": that was the CI script not waiting for pluginval.exe (a GUI-subsystem app), not the plugin. Fixed in v0.3.0 with Start-Process -Wait.

---

## 5. UI plan

- **Window:** 1672 × 940 design grid (matches the mockup), freely resizable at a fixed aspect ratio (55–160%), sizes 60–130% in the gear menu; scale is saved with the session.
- **Top bar:** logo · workspace switch (CREATE / DESIGN / LAB) · preset bar (prev/next, name → categorized menu, favourite, save, options) · CPU · voices · output meter · MIDI panic · settings.
- **Side nav:** PLAY → CREATE workspace. SYNTH / MOTION / FX → DESIGN and highlight their panels. SOUL / LAB disabled until their phase ships.
- **Workspaces share one state** — switching never touches the sound.
- **Typography:** Inter Regular/Medium/SemiBold; tracked uppercase section titles; 10.5–14 px labels.
- **Components:** one Knob class everywhere (identical behaviour in every workspace), Selector, ChoiceBox, NumberBox, TabRow, PowerButton, IconButton, displays.
- **Palette:** #090A0C background, #111317/#16181D panels, #2A2E36 borders, #F1F1F1 / #9A9DA5 text, violet-white accent #C9B8F2.
