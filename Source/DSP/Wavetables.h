#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>

namespace divinity
{
//==============================================================================
/**
    Immutable bank of factory wavetables, generated once at startup and shared
    between all plugin instances (via juce::SharedResourcePointer).

    Each table has kFrames single-cycle frames. Each frame is stored at
    kLevels mip levels: level L keeps at most (kSize/2 >> L) harmonics, so the
    oscillator can pick a level that never exceeds Nyquist (no aliasing).
    Every stored frame has one guard sample (index kSize == index 0) so the
    interpolator never needs to wrap.
*/
class WavetableBank
{
public:
    static constexpr int kSizeOrder = 11;
    static constexpr int kSize      = 1 << kSizeOrder;   // 2048 samples per cycle
    static constexpr int kStride    = kSize + 1;         // + guard sample
    static constexpr int kFrames    = 32;
    // Half-octave mip levels: level L keeps round(1024 * 2^(-L/2)) harmonics.
    // Even levels are exactly the Classic engine's octave levels (1024 >> L/2),
    // so Classic mode reads bit-identical tables; Divine 2.0 uses every level.
    static constexpr int kLevels    = 21;

    // Frames of the internal (not user-selectable) analog table used by the Divine 2.0 analog engine.
    enum AnalogFrame { kAnalogSine = 0, kAnalogTriangle = 1, kAnalogSaw = 2, kAnalogSquare = 3 };

    WavetableBank();

    /** Number of user-selectable tables. */
    int getNumTables() const noexcept { return numTables; }
    /** Index of the internal band-limited analog table (unnormalised: saw = ideal 2p-1). */
    int getAnalogTable() const noexcept { return numTables; }

    /** Pointer to kStride samples. */
    const float* getFrame (int table, int frame, int level) const noexcept
    {
        return data.data() + ((size_t) ((table * kFrames + frame) * kLevels + level)) * kStride;
    }

    static int harmonicsForLevel (int level) noexcept
    {
        return juce::jmax (1, (int) std::lround (1024.0 * std::pow (2.0, -0.5 * level)));
    }

    /** Selects the mip level for a phase increment.
        Classic (limit 0.45, step 2): octave levels, all partials below 0.45 fs — up to an
        octave of top end is lost on some notes.
        Divine 2.0 (limit 0.6, step 1): half-octave levels; partials between Nyquist and
        0.6 fs fold back above 0.4 fs (>= 17.6 kHz at 44.1 kHz, inaudible) while the band
        edge never drops below 0.42 fs — bright, even top end across the keyboard. */
    static int levelForIncrement (double phaseIncrement, double limit = 0.45, int step = 2) noexcept
    {
        const double maxHarm = limit / juce::jmax (1.0e-9, phaseIncrement);
        int level = 0;
        while (level + step < kLevels && (double) harmonicsForLevel (level) > maxHarm)
            level += step;
        return level;
    }

private:
    int numTables = 0;
    std::vector<float> data;

    void buildTable (int tableIndex, int numTablesTotal);
};

} // namespace divinity
