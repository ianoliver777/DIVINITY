# DIVINITY — Sound Engine 2.0

TRIPLE SEVEN AUDIO · v0.6.0 · DSP overhaul, measured.

## 1. Audit: the five biggest sound-quality limitations of v0.5

Measured on the untouched v0.5.0 engine with the new `DivinitySoundLab` tool before any change.

| # | Limitation | Evidence (v0.5) | Cause in the code |
|---|---|---|---|
| 1 | Chorus added **no width at all** | L/R correlation 1.00 on a mono source | `juce::dsp::Chorus` runs one LFO for both channels |
| 2 | **Metallic reverb** tail | tail spectrum 99th-percentile peak 19.9 dB above its smoothed envelope (Gaussian noise ≈ 9 dB) | `juce::dsp::Reverb` (Freeverb): fixed, unmodulated combs |
| 3 | **Dull top end** on wavetables | C4 saw: −20.0 dB of energy above 8 kHz (ideal band-limited saw ≈ −18.8 dB); factory presets average −40 dB above 6 kHz | octave-spaced mipmaps selected at 0.45 fs → band edge as low as 0.225 fs |
| 4 | **Static, mechanical voices** | pitch of a held note never moves (0.00 cents wander); unison detune perfectly evenly spaced, so the beating pattern repeats | no analog drift; evenly spaced unison detune |
| 5 | **Aliasing highs / weak filter character** | analog saw −39 to −43 dB audible-band alias (notes 84–103), pulse −38 dB; ladder loses 5.2 dB at res 0.9 and 3.9 dB when driven | 2-point PolyBLEP; un-anti-aliased wavefold; linear SVF; ladder passband compensation too small |

The order of work followed the audible impact: oscillators/top end → unison/motion → reverb + chorus → filters.

## 2. What changed (all in the real engine, all parameters automatable)

| Area | Divine 2.0 |
|---|---|
| Wavetables | 21 half-octave mip levels (was 11 octave levels), selected at 0.6 fs: anything folding back lands above 0.4 fs (≥ 17.6 kHz at 44.1 kHz) and the band edge never drops below 0.42 fs. |
| Analog osc | Saw / square / triangle read exact Fourier-series tables (same phase, polarity and level as before). Pulse = difference of two band-limited saws → alias-free PWM. Sub triangle uses PolyBLAMP. |
| Wavefold | First-order antiderivative anti-aliasing (same fold curve). |
| Unison | Slightly irregular detune spacing (no perfectly periodic beating / centre cancellation) + independent slow drift per unison voice. |
| Analog Drift (new, LAB → GLOBAL "DRIFT") | Per-voice pitch and cutoff wander + a small per-note offset ("every note a different take"). 0 % = static and deterministic. |
| Filters | SVF band-pass integrator soft-saturates (resonance self-limits, warmer when driven). Filter drive is loudness-compensated. Ladder passband compensation 0.3 → 0.55. Velocity → brightness ±0.6 oct around velocity 0.8. Velocity → amp curve slightly expanded. |
| Reverb | 8-line FDN: input band-limiting (90 Hz–13 kHz), pre-delay (new knob), early reflections, 4-stage input diffusion, Householder feedback, deep slow line modulation, in-loop allpasses, frequency-dependent decay (damping sets the HF/LF RT ratio). |
| Chorus | Dimension style: 3 modulated taps per side, decorrelated rates/phases, opposite-phase sides, Hermite interpolation. |
| Delay | Feedback path high-passed (140 Hz) and softly saturated. |
| Presets | Every factory preset: drift and pre-delay per category, mod wheel + aftertouch routes (filter, or fold/amp when the filter is off). Wave/neogrime level trims re-measured. |

### Compatibility
- New parameter `engine_mode` (Classic 1.0 / Divine 2.0). Default 2.0.
- **Sessions and user presets saved before 0.6 have no `engine_mode` and load on Classic 1.0.** Classic is measured to reproduce v0.5: every SoundLab number below is identical to two decimals when run with `--classic` on the new build.
- Classic mode reads the same wavetables (the even mip levels are the old octave levels), the same PolyBLEP, the same Freeverb, chorus, delay and filter maths.
- New IDs only appended (`engine_mode`, `analog`, `reverb_predelay`); nothing renamed. The v0.5 factory presets are preserved in git (tag/branch `main` at 51b3d27) and can be played as they were by switching a patch to 1.0.

## 3. Before / after (SoundLab, 48 kHz, effects off unless noted, drift 0 for dry tests)

| Measurement | v0.5 (Classic) | 2.0 |
|---|---:|---:|
| analog saw alias, note 84 | -42.75 dB | -88.86 dB |
| analog saw alias, note 96 | -40.39 dB | -87.68 dB |
| analog saw alias, note 103 | -39.28 dB | -87.60 dB |
| analog triangle alias, note 84 | -52.45 dB | -92.07 dB |
| analog triangle alias, note 96 | -44.41 dB | -87.25 dB |
| WT saw air (>8k re total), note 60 | -20.00 dB | -18.99 dB |
| WT saw alias, note 60 | -83.04 dB | -79.14 dB |
| WT saw air (>8k re total), note 79 | -16.85 dB | -14.23 dB |
| WT saw alias, note 79 | -87.19 dB | -85.86 dB |
| WT saw air (>8k re total), note 90 | -14.13 dB | -11.46 dB |
| WT saw alias, note 90 | -87.86 dB | -87.77 dB |
| WT saw air (>8k re total), note 96 | -9.93 dB | -9.26 dB |
| WT saw alias, note 96 | -87.71 dB | -87.65 dB |
| wavefold 0.5 alias, note 88 | -88.45 dB | -88.36 dB |
| wavefold 0.5 alias, note 100 | -87.84 dB | -90.14 dB |
| wavefold 0.9 alias, note 88 | -88.20 dB | -88.26 dB |
| wavefold 0.9 alias, note 100 | -45.20 dB | -55.48 dB |
| analog pulse 20% alias, note 90 | -39.16 dB | -88.09 dB |
| analog pulse 20% alias, note 100 | -38.38 dB | -88.03 dB |
| analog saw air (>8k re total), note 60 | -20.80 dB | -18.76 dB |
| analog saw level, note 60 | -14.27 dBFS | -14.23 dBFS |
| unison L/R correlation | 0.51  | 0.53  |
| unison mono-sum loss | -1.24 dB | -1.19 dB |
| unison loudness vs single voice | -0.75 dB | -0.85 dB |
| unison movement (10 ms RMS std-dev) | 1.61 dB | 1.65 dB |
| pitch wander over a held note (std-dev) | 0.00 cents | 0.20 cents |
| MG Low 24 res 0.0 level vs dry | -1.49 dB | -1.44 dB |
| MG Low 24 res 0.9 level vs dry | -5.20 dB | -2.17 dB |
| MG Low 24 drive 0.8 level vs dry | -3.86 dB | -0.31 dB |
| Low Pass 12 res 0.0 level vs dry | -0.56 dB | -0.54 dB |
| Low Pass 12 res 0.9 level vs dry | 1.31 dB | 0.64 dB |
| Low Pass 12 drive 0.8 level vs dry | 0.14 dB | 1.95 dB |
| reverb tail spectral peakiness | 19.90 dB | 9.83 dB |
| reverb echo density (early tail) | 0.99  | 1.08  |
| reverb tail L/R correlation | 0.00  | 0.05  |
| reverb decay 0.2s->1.2s | 22.78 dB | 20.22 dB |
| reverb mix 100% level vs dry | 0.12 dB | 0.59 dB |
| reverb size 0.20 wet-only level vs dry | -0.27 dB | -5.79 dB |
| reverb size 0.50 wet-only level vs dry | 1.87 dB | 1.23 dB |
| reverb size 0.70 wet-only level vs dry | 3.67 dB | 0.57 dB |
| reverb size 0.85 wet-only level vs dry | 5.60 dB | 6.30 dB |
| reverb size 0.95 wet-only level vs dry | 7.95 dB | 6.99 dB |
| reverb size 1.00 wet-only level vs dry | 10.19 dB | 3.37 dB |
| chorus L/R correlation | 1.00  | 0.82  |
| chorus mono-sum loss | 0.00 dB | -0.41 dB |

CPU (test runner, 48 kHz): "Celestial Pad" 16 voices 12.5 % → 15.8 % of one core; worst case 32 voices × 3 osc × 8 unison 44.7 % → 55.1 % (≈ +25 %). Not profiled yet — there is room to optimise.

Factory library (220 presets): presets with negative L/R correlation (phasey, mono-unsafe) 14 → 6; presets losing more than 3 dB when summed to mono 15 → 6; none exceed the −0.3 dB ceiling.

### How to read the numbers
- *alias*: energy below 16 kHz that is not on a harmonic of the note, relative to everything below 16 kHz. −88 dB is the analyser's floor.
- *air*: energy above 8 kHz relative to the whole note. A perfect band-limited saw at C4/48 kHz is ≈ −18.8 dB.
- *reverb tail spectral peakiness*: 99th-percentile spike above the smoothed tail spectrum. Decaying Gaussian noise (the ideal) scores ≈ 9 dB. Lower = less metallic.
- *reverb size N wet-only*: depends on the test chord landing on room modes, so single sizes jump around a little; the average sits ~2 dB under Classic on purpose (less wash).

## 4. Testing done
- `DivinityTests`: full existing suite + new Sound Engine 2.0 checks (engine default, pre-2.0 session and user preset → Classic, alias-free analog saw/pulse, drift on/off behaviour and pitch stability, chorus width, reverb bounded at max size/zero damping with size jumps, mod wheel + aftertouch on every factory preset).
- `DivinitySoundLab --wavs`: every factory preset rendered on both engines (`soundlab/presets/*.wav`, `presets.csv`).

## 5. Not done in this pass (honest list)
- **Listening.** Every number here is measured; nobody has auditioned all 220 presets by ear on 2.0 yet. That's the next step and it's a human job.
- **Preset count:** the library has 220 presets, not 210. The redesign here is a library-wide finishing pass + re-levelling, not 220 hand-rebuilt patches.
- **New oscillator features from the spec:** hard sync, FM/PM between oscillators, phase distortion, spectral warp, configurable free-running phase. They need new per-oscillator parameters and UI and were deliberately left until the core sound was fixed ("prioritise audible quality over features").
- **Filters:** no new models (comb/ladder/SVF set unchanged), no per-oscillator routing.
- **Builds:** Linux VST3 + Standalone built locally; GitHub Actions built and tested macOS universal (VST3 / AU / Standalone: test suite, auval, pluginval) and Windows x64 (VST3 / Standalone: test suite, pluginval) green. Not yet run inside a real DAW session, and still unsigned / not notarised.
