#pragma once

#include <juce_dsp/juce_dsp.h>
#include <complex>
#include <array>
#include <cstdint>
#include <vector>

namespace divinity
{
/** Harmonic spectrum of one single-cycle frame: bins 0 .. 1024 (complex, sine phase = (0, -amp)). */
using WaveSpectrum = std::vector<std::complex<double>>;

/** The 0.7.0 wavetable library (tables after the 6 originals). Defined in WavetableLibrary.cpp. */
int libraryTableCount();
juce::String libraryTableName (int i);
WaveSpectrum makeLibrarySpectrum (int i, double t);

//==============================================================================
/**
    Immutable bank of factory wavetables, generated once at startup and shared
    between all plugin instances (via juce::SharedResourcePointer).

    Two storages:
    - CLASSIC: the six original tables exactly as v0.5 built them (float, 2048
      samples, 11 octave mip levels) - the Classic 1.0 engine reads only these,
      so old projects stay bit-identical.
    - COMPACT: every table (originals + library + the internal analog table) at
      21 half-octave mip levels. Each level is stored at the smallest power-of-two
      size that oversamples its highest harmonic 16x (so plain linear
      interpolation stays alias-free), as 16-bit samples (-96 dB). The original
      tables keep 32 frames; library tables use 16 (their morphs are smooth
      parametric sweeps). That keeps ~90 tables in a sensible amount of RAM.
*/
class WavetableBank
{
public:
    static constexpr int kSizeOrder = 11;
    static constexpr int kSize      = 1 << kSizeOrder;   // 2048 samples per cycle (level 0)
    static constexpr int kFrames    = 32;
    static constexpr int kLevels    = 21;                // half-octave levels: round(1024 * 2^(-L/2)) harmonics
    static constexpr int kClassicLevels = 11;            // octave levels: 1024 >> L harmonics
    static constexpr int kClassicStride = kSize + 1;
    static constexpr int kNumOriginalTables = 6;
    static constexpr float kStoreScale = 2.0f;           // int16 full scale = +-2.0 (headroom for Gibbs peaks)

    enum AnalogFrame { kAnalogSine = 0, kAnalogTriangle = 1, kAnalogSaw = 2, kAnalogSquare = 3 };

    /** One mip level of one frame in compact storage: `size` samples + 1 guard sample. */
    struct FrameView
    {
        const int16_t* data = nullptr;
        int size = 0;
        /** 4-point Hermite interpolation, phase 0..1 (wraps; size is a power of two). Keeps the
            interpolation images ~90 dB down at the 16x table oversampling used here. */
        float at (double phase) const noexcept
        {
            const double idx = phase * size;
            int i1 = (int) idx;
            const float f = (float) (idx - i1);
            const int m = size - 1;
            i1 &= m;
            const float xm1 = data[(i1 - 1) & m], x0 = data[i1], x1 = data[(i1 + 1) & m], x2 = data[(i1 + 2) & m];
            const float c1 = 0.5f * (x1 - xm1);
            const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            constexpr float k = kStoreScale / 32767.0f;
            return k * (((c3 * f + c2) * f + c1) * f + x0);
        }
    };

    WavetableBank();

    /** Number of user-selectable tables (originals + library). */
    int getNumTables() const noexcept { return numTables; }
    /** Index of the internal band-limited analog table (unnormalised: saw = ideal 2p-1). */
    int getAnalogTable() const noexcept { return numTables; }

    /** `frame` is always on the 0..kFrames-1 scale; tables with fewer stored frames map it down. */
    FrameView getFrame (int table, int frame, int level) const noexcept
    {
        FrameView v;
        v.size = levelSize[(size_t) level];
        const int stored = framesFor (table);
        const int f = stored == kFrames ? frame : (frame * (stored - 1) + (kFrames - 1) / 2) / (kFrames - 1);
        v.data = compact.data() + (tableOffset[(size_t) table] + (size_t) f) * frameStride + (size_t) levelOffset[(size_t) level];
        return v;
    }

    /** Direct access by stored frame index (0 .. framesFor (table) - 1). */
    FrameView getStoredFrame (int table, int storedFrame, int level) const noexcept
    {
        FrameView v;
        v.size = levelSize[(size_t) level];
        v.data = compact.data() + (tableOffset[(size_t) table] + (size_t) storedFrame) * frameStride + (size_t) levelOffset[(size_t) level];
        return v;
    }

    /** Frames actually stored for a table (32 for the originals + analog, 16 for library tables). */
    int framesFor (int table) const noexcept { return table < kNumOriginalTables || table == numTables ? kFrames : kLibraryFrames; }
    static constexpr int kLibraryFrames = 16;

    /** Classic storage (tables 0..5 only): pointer to kClassicStride floats. */
    const float* getClassicFrame (int table, int frame, int octaveLevel) const noexcept
    {
        return classic.data() + ((size_t) ((table * kFrames + frame) * kClassicLevels + octaveLevel)) * kClassicStride;
    }

    static int harmonicsForLevel (int level) noexcept
    {
        return juce::jmax (1, (int) std::lround (1024.0 * std::pow (2.0, -0.5 * level)));
    }

    /** Divine 2.0: half-octave level whose partials stay below `limit` x fs (0.6: fold-back lands above 0.4 fs, inaudible). */
    static int levelForIncrement (double phaseIncrement, double limit = 0.6) noexcept
    {
        const double maxHarm = limit / juce::jmax (1.0e-9, phaseIncrement);
        int level = 0;
        while (level + 1 < kLevels && (double) harmonicsForLevel (level) > maxHarm)
            ++level;
        return level;
    }

    /** Classic 1.0: octave level whose partials stay below 0.45 fs (exactly the v0.5 rule). */
    static int classicLevelForIncrement (double phaseIncrement) noexcept
    {
        const double maxHarm = 0.45 / juce::jmax (1.0e-9, phaseIncrement);
        int level = 0;
        while (level < kClassicLevels - 1 && (double) ((kSize / 2) >> level) > maxHarm)
            ++level;
        return level;
    }

    size_t getMemoryBytes() const noexcept { return compact.size() * sizeof (int16_t) + classic.size() * sizeof (float); }

private:
    int numTables = 0;
    std::vector<float> classic;
    std::vector<int16_t> compact;
    std::array<int, kLevels> levelSize {}, levelOffset {};
    std::vector<size_t> tableOffset;   // in frames
    size_t frameStride = 0;

    void buildClassic (int table);
    void buildCompact (int table, bool analog);
};

} // namespace divinity
