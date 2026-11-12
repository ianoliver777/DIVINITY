# DIVINITY

**TRIPLE SEVEN AUDIO — Engineered to Feel.**
Hybrid software synthesizer · VST3 / AU / Standalone · macOS (Apple Silicon + Intel) & Windows · JUCE / C++17

Current version: **1.1.0** — teal porcelain skin matching the 1.1 reference (exact DIVINITY plaque, TRIPLE SEVEN AUDIO logo and barbed-frown Guardian Angel) with NIGHT MODE as its true inverse; opens at 125 % (size is now remembered per user, not per project); MIDI controller mapping mode (MIDI button, click a knob, move a controller; mappings follow you into every instance); the whole bank re-voiced warm and thick (analog drift, tape saturation, warm EQ tilt, chorus on melodic sounds); 80 new genre presets in WAVE, TRAP, TECHNO, DUBSTEP, GARAGE, DARK GARAGE, DNB and CINEMATIC (351 total). 1.0.0: new porcelain day skin (matches the 1.0 reference design, real T7A logo) with a one-click NIGHT MODE toggle (moon/sun icon, remembered between sessions); drag-and-drop modulation (grab the ✥ handle on any LFO / ENV / STEP / RANDOM and drop it on a knob); draw your own LFO shapes (Custom shape, 16 points); bend each envelope segment (attack / decay / release curves); 7 new distortion types (12 total); a fully customisable multiband (two movable crossovers, per-band gain + down/up threshold and ratio); larger default window. 0.9.1: the wave / Reese bank rebuilt as 271 presets where every preset has its own core sound (REESE 40, BASS 30, LEAD 35, SYNTH 35, PAD 35, TEXTURE 25, SEQUENCE 30, PERCUSSION 16, FX 25); 0.9.0 had 777 presets that reused ~15 core sounds per category; every name is built from what the sound actually does and checked by measurement (manifest: [`docs/PRESET_NAMES.csv`](docs/PRESET_NAMES.csv)); 0.8.0: oscillator WARP (hard sync, FM, AM, bend, PWM, phase distortion, mirror), import your own wavetables, MIDI learn + MPE, Eco / Standard / HQ quality; 0.7.3: preset bank redesigned for variety; 0.7.2: — MIDI generator with drag-to-DAW, spectral stereo imager on EFFECTS (0.7.1: — the interface matches the v0.7 reference design; 0.7.0 brought the new UI, OSC D + SAMPLER, 8 macros, a 16-step sequencer, 82 new wavetables (88 total) and a brand-new 777-preset library in nine categories). Built on Sound Engine 2.0 (old projects still auto-load on the Classic 1.0 engine). Measurements: [`docs/SOUND_ENGINE_2.md`](docs/SOUND_ENGINE_2.md). See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the architecture, roadmap, UI plan and an honest list of what's in and what isn't yet.

---

## Build

You need **CMake 3.22+** and a C++17 compiler. JUCE 8.0.4 is downloaded automatically on first configure (or drop a JUCE checkout into `./JUCE`).

### macOS (Xcode command-line tools)
```bash
cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release --target DIVINITY_VST3 DIVINITY_AU DIVINITY_Standalone
```
Plugins land in `build/DIVINITY_artefacts/Release/`. Copy them to:
- VST3 → `~/Library/Audio/Plug-Ins/VST3/`
- AU → `~/Library/Audio/Plug-Ins/Components/`

Unsigned builds downloaded from CI need the quarantine flag removed once:
```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/DIVINITY.vst3 ~/Library/Audio/Plug-Ins/Components/DIVINITY.component
```

### Windows (Visual Studio 2022 or newer)
```bat
cmake -S . -B build -A x64
cmake --build build --config Release --target DIVINITY_VST3 DIVINITY_Standalone
```
Copy `DIVINITY.vst3` to `C:\Program Files\Common Files\VST3\`.

### Linux (dev / CI)
```bash
sudo apt install libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libfreetype-dev libfontconfig1-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build DIVINITY_VST3 DIVINITY_Standalone DivinityTests
```

### GitHub Actions
Push to GitHub and `.github/workflows/build.yml` builds a **universal macOS** zip (VST3 + AU + app) and a **Windows x64** zip, runs the test suite, `auval`, and pluginval (strictness 10) on both, and uploads everything as artifacts.

---

## Test
```bash
./build/DivinityTests_artefacts/Release/DivinityTests test-output
# objective sound measurements (+ every factory preset as a WAV)
./build/DivinitySoundLab_artefacts/Release/DivinitySoundLab soundlab-output --wavs
```
186 automated checks: pitch accuracy, envelope timing, filter stability, modulation routing, state round-trip & older-session compatibility, deterministic recall, user preset save/load, MIDI/sustain/legato/voice stealing, 60 random-patch fuzz runs, 44.1–192 kHz × 32–1024 sample sweeps, 60 s sustained notes, denormal/silence, CPU, factory preset levels. It also writes a WAV demo of every factory preset and PNG snapshots of both workspaces to `test-output/`.

---

## Using it
- **Top of the window (every tab):** logo, wordmark, CPU / RAM / VOICES, workspace tabs + SHOW/HIDE ANGEL, then the preset bar: import, save, undo/redo, copy, favourites, preset browser, ARP, POLY / MONO / LEGATO, voices and glide. Bottom bar: pitch + mod wheels, bend range, keyboard, PORTA / VEL / KEYTRK / PAN / VOL and a scope.
- **Presets (0.9.1 wave bank):** 271 factory presets, loudness-matched, one per core sound — REESE 40 (only real reeses: two or more detuned saw-family voices that audibly beat), BASS 30 (808s, subs, growl / FM / vowel / sync basses), LEAD 35, SYNTH 35 (keys, stabs, plucks, organs), PAD 35, TEXTURE 25 (rain, vinyl, wind, drones), SEQUENCE 30 (arps and step sequences: hold a chord or one key), PERCUSSION 16 (tempo-locked hat / perc loops: hold one key), FX 25 (risers, downlifters, impacts, drops, sweeps, whooshes, sirens, glitches, atmospheres). Synths, leads, pads, textures and sequences are drenched in reverb and delay; basses keep a mono low end with space on top; drums keep dry transients. Names are assembled from the ingredients each sound actually uses ("Breathing Hollow Reese", "Cathedral Glass Lead", "Rising Reese Arp") and the test suite checks the claims by rendering every preset.
- **CREATE** — ENGINE list (OSC A-D, SUB, NOISE, SAMPLER), the selected source, FILTER 1/2, AMP envelope, LFO 1-4, MACROS 1-8 (TONE, MOVEMENT, TEXTURE, SPACE, CHARACTER, WIDTH, PUNCH, AIR), ENV 1-4 / LFO 1-4 / STEP / RANDOM / MATRIX editors, MOD MATRIX, EFFECTS CHAIN and OUTPUT SPECTRUM (PRE / POST / MIX).
- **SAMPLER** — 26 built-in synthesized one-shots / loops (hats, claps, rims, impacts, choir, breath, rain, vinyl...) or drag in your own WAV / AIFF / FLAC; key-track, start, loop. User samples save with the session.
- **BROWSER** — category tiles, search across names / tags / descriptions, details + favourites.
- **EFFECTS** — the FX rack: 16 effects (EQ, compressor, saturation, distortion, bitcrusher, transient shaper, multiband, chorus, flanger, phaser, tremolo/autopan, ring mod, frequency shifter, delay, reverb, stereo tools). Drag rows (or use the arrows) to reorder; click a dot to switch an effect on. The order saves with presets and sessions and is undoable. The safety limiter always stays last. **PLAY** — preset browser, arpeggiator (7 modes, synced rates incl. triplets/dotted, 1-4 octaves, gate, swing, latch), SOUL pad, macros, performance controls, keyboard. **LAB** — matrix and deep settings.
- **Guardian Angel** (right side) — ask questions, describe a sound to build, get ideas, explain any control, randomize.
- **SOUL** — drag the emotion pad (soft ↔ aggressive, melancholy ↔ euphoria), edit how each target responds, and hit **Divine Intervention** (Ascension / Collapse / Shatter / Bloom / Abyss).
- Every control: hover for help · double-click to reset · shift-drag for fine · right-click to type a value.
- **WARP (every oscillator, 0.8.0):** the selector next to the table / wave picks a mode — **Sync** (hard sync to another oscillator), **FM** (phase modulation from another oscillator), **AM**, **Bend + / Bend -**, **PWM**, **Phase Dist**, **Mirror** — and the WARP knob sets the amount (it's a mod-matrix destination: "Osc A Warp" etc.). Sync / FM / AM pick their source oscillator in the box beside it; the source can be switched off and still modulate.
- **Your own wavetables:** drop a WAV / AIFF / FLAC onto an oscillator (or click its display → Import wavetable). Serum / Vital-style tables (2048-sample frames, up to 256) load frame by frame; a single cycle works too. It shows up as "User Wavetable" and saves with the session and with presets.
- **MIDI learn:** right-click any knob → MIDI learn, then move a controller. Right-click again to forget it. Mappings save with the session.
- **Settings (gear):** Quality — Eco (lighter CPU, unison max 4), Standard, HQ (2x oversampled engine) and "Always HQ when rendering"; MPE (lower zone, per-note bend / pressure / slide; bend range 2-96). These are global: loading a preset doesn't change them.
- Click a waveform display to change wavetable / wave. Drag the envelope points. `…` on a card opens all its settings.
- Undo / redo: Cmd/Ctrl+Z, Cmd/Ctrl+Shift+Z. Presets are saved in `Documents/TRIPLE SEVEN AUDIO/DIVINITY/Presets`.

---

Fonts: Inter by Rasmus Andersson, SIL Open Font License 1.1 (`Resources/Fonts/Inter-LICENSE.txt`).
