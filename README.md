# DIVINITY

**TRIPLE SEVEN AUDIO — Engineered to Feel.**
Hybrid software synthesizer · VST3 / AU / Standalone · macOS (Apple Silicon + Intel) & Windows · JUCE / C++17

Current version: **0.5.0 — arpeggiator, 220 presets (110 wave + 90 neogrime), collapsible Guardian Angel**. See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the architecture, roadmap, UI plan and an honest list of what's in and what isn't yet.

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
```
72 automated checks: pitch accuracy, envelope timing, filter stability, modulation routing, state round-trip & older-session compatibility, deterministic recall, user preset save/load, MIDI/sustain/legato/voice stealing, 60 random-patch fuzz runs, 44.1–192 kHz × 32–1024 sample sweeps, 60 s sustained notes, denormal/silence, CPU, factory preset levels. It also writes a WAV demo of every factory preset and PNG snapshots of both workspaces to `test-output/`.

---

## Using it
- **Top of the window (every tab):** workspace tabs + HIDE/SHOW ANGEL, then the preset bar: settings, import, save, undo/redo, category filter (WAVE / NEOGRIME / CLASSIC groups), preset browser, ARP on/off, POLY / MONO / LEGATO, voices and glide.
- **Presets:** 220 factory presets. 110 wave (leads, pads, reese, 808s, vocal chops, plucks, bells, atmospheres, gated + pumping chords, arps), 90 neogrime (eski square leads, subs, growls, ice bells, stabs, metal plucks, 8-bit leads, 808s, pads, arps, sirens) and the 20 classics. Loudness-matched.
- **CREATE** — the engine: oscillators, sub, noise, filter, amp envelope, LFO, macros, all envelopes/LFOs/matrix, quick FX + spectrum.
- **EFFECTS** — the FX rack: 16 effects (EQ, compressor, saturation, distortion, bitcrusher, transient shaper, multiband, chorus, flanger, phaser, tremolo/autopan, ring mod, frequency shifter, delay, reverb, stereo tools). Drag rows (or use the arrows) to reorder; click a dot to switch an effect on. The order saves with presets and sessions and is undoable. The safety limiter always stays last. **PLAY** — preset browser, arpeggiator (7 modes, synced rates incl. triplets/dotted, 1-4 octaves, gate, swing, latch), SOUL pad, macros, performance controls, keyboard. **LAB** — matrix and deep settings.
- **Guardian Angel** (right side) — ask questions, describe a sound to build, get ideas, explain any control, randomize.
- **SOUL** — drag the emotion pad (soft ↔ aggressive, melancholy ↔ euphoria), edit how each target responds, and hit **Divine Intervention** (Ascension / Collapse / Shatter / Bloom / Abyss).
- Every control: hover for help · double-click to reset · shift-drag for fine · right-click to type a value.
- Click a waveform display to change wavetable / wave. Drag the envelope points. `…` on a card opens all its settings.
- Undo / redo: Cmd/Ctrl+Z, Cmd/Ctrl+Shift+Z. Presets are saved in `Documents/TRIPLE SEVEN AUDIO/DIVINITY/Presets`.

---

Fonts: Inter by Rasmus Andersson, SIL Open Font License 1.1 (`Resources/Fonts/Inter-LICENSE.txt`).
