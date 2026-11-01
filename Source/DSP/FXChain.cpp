#include "FXChain.h"

namespace divinity
{
//==============================================================================
void StereoDelay::prepare (double sr, int maxSeconds)
{
    sampleRate = sr;
    const size_t size = (size_t) (sr * maxSeconds) + 4;
    bufL.assign (size, 0.0f);
    bufR.assign (size, 0.0f);
    time.reset (sr, 0.25);
    reset();
}

void StereoDelay::reset()
{
    std::fill (bufL.begin(), bufL.end(), 0.0f);
    std::fill (bufR.begin(), bufR.end(), 0.0f);
    writePos = 0; lpL = lpR = 0.0f;
}

void StereoDelay::process (float* L, float* R, int n, float delaySeconds, float feedback, float mix, float tone, bool pingPong) noexcept
{
    const int size = (int) bufL.size();
    if (size < 8) return;
    time.setTargetValue (juce::jlimit (0.001f, (float) (size - 4) / (float) sampleRate, delaySeconds));
    const float toneHz = 800.0f * std::pow (2.0f, tone * 4.5f);   // 800 Hz .. ~18 kHz
    const float a = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * toneHz / (float) sampleRate);

    auto read = [size] (const std::vector<float>& b, float pos)
    {
        if (pos < 0) pos += (float) size;
        const int i0 = juce::jlimit (0, size - 1, (int) pos);
        const int i1 = (i0 + 1) % size;
        const float f = pos - (float) i0;
        return b[(size_t) i0] + f * (b[(size_t) i1] - b[(size_t) i0]);
    };

    for (int s = 0; s < n; ++s)
    {
        const float d = time.getNextValue() * (float) sampleRate;
        const float rp = (float) writePos - d;
        const float outL = read (bufL, rp);
        const float outR = read (bufR, rp);
        lpL += a * (outL - lpL);
        lpR += a * (outR - lpR);
        lpL = dsp::sanitise (lpL); lpR = dsp::sanitise (lpR);

        const float inL = L[s], inR = R[s];
        if (pingPong)
        {
            bufL[(size_t) writePos] = 0.5f * (inL + inR) + feedback * lpR;
            bufR[(size_t) writePos] = feedback * lpL;
        }
        else
        {
            bufL[(size_t) writePos] = inL + feedback * lpL;
            bufR[(size_t) writePos] = inR + feedback * lpR;
        }
        L[s] += mix * outL;
        R[s] += mix * outR;
        writePos = (writePos + 1) % size;
    }
}

//==============================================================================
FXChain::FXChain (juce::AudioProcessorValueTreeState& s) : apvts (s)
{
    satOn = p (ids::satOn); satDrive = p (ids::satDrive); satMix = p (ids::satMix);
    choOn = p (ids::choOn); choRate = p (ids::choRate); choDepth = p (ids::choDepth); choMix = p (ids::choMix);
    dlyOn = p (ids::dlyOn); dlyDiv = p (ids::dlyDiv); dlyFb = p (ids::dlyFeedback); dlyMix = p (ids::dlyMix);
    dlyPing = p (ids::dlyPingPong); dlyTone = p (ids::dlyTone);
    revOn = p (ids::revOn); revSize = p (ids::revSize); revDamp = p (ids::revDamp); revWidth = p (ids::revWidth); revMix = p (ids::revMix);
    limOn = p (ids::limOn); limCeiling = p (ids::limCeiling); master = p (ids::master);
}

void FXChain::prepare (double sr, int maxBlockSize)
{
    sampleRate = sr;
    maxBlock = juce::jmax (32, maxBlockSize);
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };
    chorus.prepare (spec);
    reverb.prepare (spec);
    limiter.prepare (spec);
    delay.prepare (sr, 4);
    wet.setSize (2, maxBlock);
    for (auto* sv : { &satDriveS, &satMixS, &choMixS, &dlyMixS, &revMixS, &masterS })
        sv->reset (sr, 0.03);
    reset();
}

void FXChain::reset()
{
    saturator.reset();
    chorus.reset();
    reverb.reset();
    limiter.reset();
    delay.reset();
    satDriveS.setCurrentAndTargetValue (0); satMixS.setCurrentAndTargetValue (0);
    choMixS.setCurrentAndTargetValue (0); dlyMixS.setCurrentAndTargetValue (0);
    revMixS.setCurrentAndTargetValue (0);
    masterS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (master->load()));
}

void FXChain::process (juce::AudioBuffer<float>& buffer, double bpm, const SynthEngine::GlobalMods& mods,
                       const std::array<float, kNumMacros>& macros) noexcept
{
    const int total = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || total == 0) return;

    const float movement = macros[1], texture = macros[2], space = macros[3];
    auto on = [] (std::atomic<float>* a) { return a->load() > 0.5f; };
    auto clamp01 = [] (float x) { return juce::jlimit (0.0f, 1.0f, x); };

    // ---- effective targets (params + macros + mod matrix) ----
    const bool satActiveP = on (satOn);
    const float satDriveT = clamp01 ((satActiveP ? satDrive->load() : 0.0f) + texture * 0.5f + mods.satDrive);
    const float satMixT   = satActiveP ? satMix->load() : (texture > 0.001f || mods.satDrive > 0.001f ? 1.0f : 0.0f);

    const bool choP = on (choOn);
    const float choDepthT = clamp01 (choDepth->load() + movement * 0.5f + mods.chorusDepth);
    const float choMixT   = clamp01 ((choP ? choMix->load() : 0.0f) + movement * 0.6f * (choP ? 0.0f : 1.0f));

    const float dlyMixT = clamp01 ((on (dlyOn) ? dlyMix->load() : 0.0f) + mods.delayMix);
    const float revMixT = clamp01 ((on (revOn) ? revMix->load() : 0.0f) + space * 0.5f + mods.reverbMix);

    satDriveS.setTargetValue (satDriveT); satMixS.setTargetValue (satMixT);
    choMixS.setTargetValue (choMixT); dlyMixS.setTargetValue (dlyMixT); revMixS.setTargetValue (revMixT);
    masterS.setTargetValue (juce::Decibels::decibelsToGain (master->load(), -60.0f));

    display.sat = satDriveT; display.chorus = choDepthT; display.delay = dlyMixT; display.reverb = revMixT;

    for (int start = 0; start < total; start += maxBlock)
    {
        const int n = juce::jmin (maxBlock, total - start);
        float* L = buffer.getWritePointer (0, start);
        float* R = buffer.getWritePointer (1, start);

        // ---- saturation ----
        if (satMixS.getCurrentValue() > 1.0e-4f || satMixS.getTargetValue() > 1.0e-4f)
        {
            for (int s = 0; s < n; ++s)
            {
                const float drv = satDriveS.getNextValue();
                const float mix = satMixS.getNextValue();
                const float g = juce::Decibels::decibelsToGain (drv * 24.0f);
                const float makeup = 1.0f / std::sqrt (g);
                const float bias = drv * 0.15f;   // a touch of asymmetry = even harmonics
                const float yl = (saturator.process (0, L[s] * g + bias) - std::tanh (bias)) * makeup;
                const float yr = (saturator.process (1, R[s] * g + bias) - std::tanh (bias)) * makeup;
                L[s] += mix * (yl - L[s]);
                R[s] += mix * (yr - R[s]);
            }
        }
        else { satDriveS.skip (n); satMixS.skip (n); }

        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, (size_t) start, (size_t) n);
        juce::dsp::ProcessContextReplacing<float> ctx (block);

        // ---- chorus ----
        const bool chorusActive = choMixS.getCurrentValue() > 1.0e-4f || choMixS.getTargetValue() > 1.0e-4f;
        if (chorusActive)
        {
            if (! chorusWasActive) chorus.reset();
            choMixS.skip (n);
            chorus.setRate (choRate->load());
            chorus.setDepth (choDepthT);
            chorus.setCentreDelay (7.0f);
            chorus.setFeedback (0.0f);
            chorus.setMix (choMixS.getCurrentValue());
            chorus.process (ctx);
        }
        else choMixS.skip (n);
        chorusWasActive = chorusActive;

        // ---- delay ----
        if (dlyMixS.getCurrentValue() > 1.0e-4f || dlyMixS.getTargetValue() > 1.0e-4f)
        {
            dlyMixS.skip (n);
            const double beats = delayDivisionInBeats ((int) std::lround (dlyDiv->load()));
            const float secs = (float) (beats * 60.0 / juce::jmax (20.0, bpm));
            delay.process (L, R, n, secs, dlyFb->load(), dlyMixS.getCurrentValue(), dlyTone->load(), on (dlyPing));
        }
        else dlyMixS.skip (n);

        // ---- reverb ----
        const bool reverbActive = revMixS.getCurrentValue() > 1.0e-4f || revMixS.getTargetValue() > 1.0e-4f;
        if (reverbActive)
        {
            if (! reverbWasActive) reverb.reset();
            juce::dsp::Reverb::Parameters rp;
            rp.roomSize = juce::jlimit (0.0f, 1.0f, revSize->load() + space * 0.2f);
            rp.damping = revDamp->load();
            rp.width = revWidth->load();
            rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.freezeMode = 0.0f;
            reverb.setParameters (rp);
            for (int ch = 0; ch < 2; ++ch) wet.copyFrom (ch, 0, buffer, ch, start, n);
            juce::dsp::AudioBlock<float> wb (wet.getArrayOfWritePointers(), 2, 0, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> wctx (wb);
            reverb.process (wctx);
            const float* wl = wet.getReadPointer (0);
            const float* wr = wet.getReadPointer (1);
            for (int s = 0; s < n; ++s)
            {
                const float m = revMixS.getNextValue();
                const float dryG = 1.0f - 0.4f * m;
                L[s] = L[s] * dryG + wl[s] * m * 0.45f;
                R[s] = R[s] * dryG + wr[s] * m * 0.45f;
            }
        }
        else revMixS.skip (n);
        reverbWasActive = reverbActive;

        // ---- master + limiter ----
        for (int s = 0; s < n; ++s)
        {
            const float g = masterS.getNextValue();
            L[s] *= g; R[s] *= g;
        }
        const bool limiterOn = on (limOn);
        if (limiterOn)
        {
            if (! limiterWasOn) limiter.reset();
            limiter.setThreshold (limCeiling->load());
            limiter.setRelease (80.0f);
            limiter.process (ctx);
        }
        limiterWasOn = limiterOn;

        // ---- last line of defence: never pass non-finite samples to the host ----
        for (int s = 0; s < n; ++s)
        {
            if (! std::isfinite (L[s]) || ! std::isfinite (R[s]))
            {
                buffer.clear (start, n);
                reset();
                break;
            }
        }
    }
}

} // namespace divinity
