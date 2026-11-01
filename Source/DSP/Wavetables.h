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
    static constexpr int kLevels    = 11;                // 1024 .. 1 harmonics

    WavetableBank();

    int getNumTables() const noexcept { return numTables; }

    /** Pointer to kStride samples. */
    const float* getFrame (int table, int frame, int level) const noexcept
    {
        return data.data() + ((size_t) ((table * kFrames + frame) * kLevels + level)) * kStride;
    }

    /** Selects the mip level that keeps all harmonics below ~0.45 * sampleRate. */
    static int levelForIncrement (double phaseIncrement) noexcept
    {
        // highest harmonic at level L is (kSize/2 >> L); its normalised freq = h * inc
        const double maxHarm = 0.45 / juce::jmax (1.0e-9, phaseIncrement);
        int level = 0;
        while (level < kLevels - 1 && (double) ((kSize / 2) >> level) > maxHarm)
            ++level;
        return level;
    }

private:
    int numTables = 0;
    std::vector<float> data;

    void buildTable (int tableIndex, int numTablesTotal);
};

} // namespace divinity
