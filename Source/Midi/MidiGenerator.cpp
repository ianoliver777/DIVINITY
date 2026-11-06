// DIVINITY MIDI generator: chord progressions, leads, basslines and drum loops in
// trap / wave / hip-hop / drill / lo-fi / emotional styles. Everything is seeded,
// so the same settings + seed always give the same clip. Chords, lead and bass
// share one Progression so the parts fit together when layered.
#include "MidiGenerator.h"

namespace divinity::midigen
{
namespace
{
    constexpr double kSixteenth = 0.25;   // beats

    const std::vector<std::vector<int>>& scaleSteps()
    {
        static const std::vector<std::vector<int>> s {
            { 0, 2, 3, 5, 7, 8, 10 },   // Minor
            { 0, 2, 4, 5, 7, 9, 11 },   // Major
            { 0, 2, 3, 5, 7, 8, 11 },   // Harmonic Minor
            { 0, 2, 3, 5, 7, 9, 10 },   // Dorian
            { 0, 1, 3, 5, 7, 8, 10 },   // Phrygian
            { 0, 3, 5, 7, 10 },         // Minor Pentatonic
        };
        return s;
    }

    enum Style { Trap, Wave, HipHop, Drill, LoFi, Emotional };

    /** Chords are built from a 7-note scale (pentatonic borrows natural minor). */
    const std::vector<int>& chordScale (int scale) { return scaleSteps()[(size_t) (scale == 5 ? 0 : juce::jlimit (0, 4, scale))]; }
    const std::vector<int>& melodyScale (int scale) { return scaleSteps()[(size_t) juce::jlimit (0, 5, scale)]; }

    int degreeToPitch (const std::vector<int>& sc, int key, int degree, int baseOctaveNote)
    {
        const int n = (int) sc.size();
        const int oct = (int) std::floor ((double) degree / n);
        const int d = ((degree % n) + n) % n;
        return baseOctaveNote + key + sc[(size_t) d] + 12 * oct;
    }

    int pitchClassOfDegree (const std::vector<int>& sc, int key, int degree)
    {
        return ((degreeToPitch (sc, key, degree, 0) % 12) + 12) % 12;
    }

    /** Pitch classes of the chord on a scale degree (triad / 7th / add9). */
    std::vector<int> chordPcs (const Settings& s, const Progression& p, int degree)
    {
        const auto& sc = chordScale (s.scale);
        std::vector<int> pcs { pitchClassOfDegree (sc, s.key, degree), pitchClassOfDegree (sc, s.key, degree + 2), pitchClassOfDegree (sc, s.key, degree + 4) };
        if (p.sevenths) pcs.push_back (pitchClassOfDegree (sc, s.key, degree + 6));
        if (s.style == Emotional || s.style == LoFi) pcs.push_back (pitchClassOfDegree (sc, s.key, degree + 8));   // add9
        return pcs;
    }

    /** Closest voicing (in 52..80) to the previous one: smooth voice leading. */
    std::vector<int> voice (const std::vector<int>& pcs, const std::vector<int>& prev)
    {
        std::vector<int> best;
        double bestScore = 1.0e9;
        const int n = (int) pcs.size();
        for (int rot = 0; rot < n; ++rot)
            for (int base = 50; base <= 66; ++base)
            {
                if ((base % 12 + 12) % 12 != pcs[(size_t) rot]) continue;
                std::vector<int> v { base };
                for (int k = 1; k < n; ++k)
                {
                    int x = v.back() + 1;
                    while ((x % 12 + 12) % 12 != pcs[(size_t) ((rot + k) % n)]) ++x;
                    v.push_back (x);
                }
                if (v.back() > 82) continue;
                double score = 0.0;
                if (prev.size() == v.size()) for (size_t k = 0; k < v.size(); ++k) score += std::abs (v[k] - prev[k]);
                else { double mean = 0; for (int x : v) mean += x; score = std::abs (mean / n - 64.0) * n; }
                if (score < bestScore) { bestScore = score; best = v; }
            }
        return best;
    }

    juce::String chordName (const Settings& s, const Progression& p, int degree)
    {
        static const char* names[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        const auto& sc = chordScale (s.scale);
        const int root = pitchClassOfDegree (sc, s.key, degree);
        const int third = (pitchClassOfDegree (sc, s.key, degree + 2) - root + 12) % 12;
        const int fifth = (pitchClassOfDegree (sc, s.key, degree + 4) - root + 12) % 12;
        juce::String q = third == 3 ? (fifth == 6 ? "dim" : "m") : (fifth == 8 ? "aug" : "");
        if (p.sevenths)
        {
            const int sev = (pitchClassOfDegree (sc, s.key, degree + 6) - root + 12) % 12;
            if (q == "m") q = sev == 10 ? "m7" : "m(maj7)";
            else if (q == "dim") q = sev == 10 ? "m7b5" : "dim7";
            else if (q.isEmpty()) q = sev == 11 ? "maj7" : "7";
        }
        else if (s.style == Emotional || s.style == LoFi) q += "add9";
        return juce::String (names[root]) + q;
    }

    float humanVel (juce::Random& r, float base, float spread = 0.06f) { return juce::jlimit (0.15f, 1.0f, base + spread * (r.nextFloat() * 2.0f - 1.0f)); }

    /** Bars per chord and how many chords the clip holds. */
    struct Layout { int chords; double beatsPerChord; };
    Layout layoutFor (const Settings& s, const Progression& p)
    {
        const int n = (int) p.degrees.size();
        if (s.bars <= 2) return { n, 8.0 / n };          // 4 chords in 2 bars
        return { s.bars, 4.0 };                           // one chord per bar, progression repeats
    }
    int degreeAt (const Progression& p, int chordIndex) { return p.degrees[(size_t) (chordIndex % (int) p.degrees.size())]; }

    //==========================================================================
    void makeChords (const Settings& s, const Progression& p, Clip& c, juce::Random& r)
    {
        const auto L = layoutFor (s, p);
        std::vector<int> prev;
        for (int i = 0; i < L.chords; ++i)
        {
            const int deg = degreeAt (p, i);
            auto v = voice (chordPcs (s, p, deg), prev);
            prev = v;
            const double t0 = i * L.beatsPerChord, span = L.beatsPerChord;
            std::vector<std::pair<double, double>> hits;   // (offset, length)
            switch (s.style)
            {
                case HipHop: hits = span >= 4.0 ? std::vector<std::pair<double, double>> { { 0.0, 1.5 }, { 2.5, 1.25 } } : std::vector<std::pair<double, double>> { { 0.0, 1.25 } }; break;
                case Drill:  hits = span >= 4.0 ? std::vector<std::pair<double, double>> { { 0.0, 1.5 }, { 1.75, 0.75 }, { 3.0, 0.9 } } : std::vector<std::pair<double, double>> { { 0.0, 1.0 }, { 1.25, 0.6 } }; break;
                case LoFi:   hits = { { 0.0, span - 0.5 }, { span - 0.5, 0.45 } }; break;
                default:     hits = { { 0.0, span - 0.02 } }; break;
            }
            for (auto [off, len] : hits)
            {
                const float base = off == 0.0 ? 0.78f : 0.66f;
                for (size_t k = 0; k < v.size(); ++k)
                {
                    const double strum = (s.style == LoFi || s.style == Emotional) ? 0.012 * (double) k : 0.0;
                    c.notes.push_back ({ t0 + off + strum, len - strum, v[k], humanVel (r, base) });
                }
            }
            // a low root under spread styles
            if (s.style == Wave || s.style == Emotional)
            {
                int root = 36 + pitchClassOfDegree (chordScale (s.scale), s.key, deg);
                if (root > 47) root -= 12;
                c.notes.push_back ({ t0, span - 0.02, root, humanVel (r, 0.7f) });
            }
        }
    }

    //==========================================================================
    void makeBass (const Settings& s, const Progression& p, Clip& c, juce::Random& r)
    {
        const auto L = layoutFor (s, p);
        const auto& sc = chordScale (s.scale);
        struct Hit { int step, len, interval; };   // interval: 0 root, 7 fifth, 12 octave, -1 approach
        std::vector<std::vector<Hit>> pool;
        switch (s.style)
        {
            case Trap:      pool = { { { 0, 8, 0 }, { 10, 6, 0 } }, { { 0, 6, 0 }, { 6, 2, 12 }, { 10, 6, 0 } }, { { 0, 10, 0 }, { 11, 2, 0 }, { 14, 2, 7 } }, { { 0, 3, 0 }, { 3, 7, 0 }, { 12, 4, 12 } } }; break;
            case Drill:     pool = { { { 0, 3, 0 }, { 3, 3, 0 }, { 6, 4, 7 }, { 12, 4, 0 } }, { { 0, 5, 0 }, { 6, 2, 12 }, { 8, 4, 0 }, { 13, 3, 7 } }, { { 0, 2, 0 }, { 3, 5, 0 }, { 10, 2, 3 }, { 12, 4, 0 } } }; break;
            case Wave:      pool = { { { 0, 16, 0 } }, { { 0, 12, 0 }, { 12, 4, 7 } }, { { 0, 8, 0 }, { 8, 8, 12 } } }; break;
            case HipHop:    pool = { { { 0, 3, 0 }, { 3, 1, 0 }, { 6, 2, 7 }, { 10, 2, 0 }, { 12, 4, 12 } }, { { 0, 4, 0 }, { 7, 2, 0 }, { 10, 4, 7 }, { 14, 2, -1 } } }; break;
            case LoFi:      pool = { { { 0, 4, 0 }, { 4, 4, 7 }, { 8, 4, 12 }, { 12, 4, -1 } }, { { 0, 6, 0 }, { 6, 2, 7 }, { 8, 6, 0 }, { 14, 2, -1 } } }; break;
            default:        pool = { { { 0, 8, 0 }, { 8, 8, 7 } }, { { 0, 16, 0 } } }; break;
        }
        const auto& pattern = pool[(size_t) r.nextInt ((int) pool.size())];
        const int stepsPerChord = (int) std::lround (L.beatsPerChord / kSixteenth);
        for (int i = 0; i < L.chords; ++i)
        {
            const int deg = degreeAt (p, i);
            int root = 36 + pitchClassOfDegree (sc, s.key, deg);
            if (root > 46) root -= 12;
            const int nextRoot = [&] { int nr = 36 + pitchClassOfDegree (sc, s.key, degreeAt (p, i + 1)); return nr > 46 ? nr - 12 : nr; }();
            for (auto h : pattern)
            {
                if (h.step >= stepsPerChord) continue;
                const int len = juce::jmin (h.len, stepsPerChord - h.step);
                int pitch = root + (h.interval >= 0 ? h.interval : 0);
                if (h.interval == -1) pitch = nextRoot + (r.nextBool() ? 1 : -1);   // chromatic approach into the next chord
                const double t = i * L.beatsPerChord + h.step * kSixteenth;
                c.notes.push_back ({ t, len * kSixteenth - 0.01, pitch, humanVel (r, h.step == 0 ? 0.9f : 0.78f) });
            }
        }
    }

    //==========================================================================
    void makeLead (const Settings& s, const Progression& p, Clip& c, juce::Random& r)
    {
        const auto L = layoutFor (s, p);
        const auto& ms = melodyScale (s.scale);
        const double totalBeats = s.bars * 4.0;
        const int bars = s.bars;

        double density = 0.38;
        switch (s.style) { case Wave: density = 0.26; break; case Emotional: density = 0.22; break; case Drill: density = 0.45; break; case LoFi: density = 0.3; break; default: break; }

        // one-bar rhythm motifs: A for most bars, B for the answer / turnaround
        auto makeRhythm = [&] ()
        {
            std::vector<int> on;
            static const float weight[16] = { 1.0f, 0.25f, 0.55f, 0.35f, 0.8f, 0.25f, 0.6f, 0.4f, 0.9f, 0.25f, 0.55f, 0.35f, 0.75f, 0.3f, 0.55f, 0.3f };
            for (int st = 0; st < 16; ++st) if (r.nextFloat() < weight[st] * density * 1.6f) on.push_back (st);
            if (on.empty() || on.front() > 2) on.insert (on.begin(), r.nextBool() ? 0 : 2);
            return on;
        };
        const auto A = makeRhythm(), B = makeRhythm();

        auto nearestScalePitch = [&] (int target)
        {
            int best = target, bestD = 99;
            for (int x = target - 6; x <= target + 6; ++x)
            {
                const int pc = ((x - s.key) % 12 + 12) % 12;
                if (std::find (ms.begin(), ms.end(), pc) != ms.end() && std::abs (x - target) < bestD) { bestD = std::abs (x - target); best = x; }
            }
            return best;
        };
        auto stepScale = [&] (int from, int steps)
        {
            int x = from;
            const int dir = steps > 0 ? 1 : -1;
            for (int k = 0; k < std::abs (steps); ++k)
            {
                do { x += dir; } while (std::find (ms.begin(), ms.end(), ((x - s.key) % 12 + 12) % 12) == ms.end());
            }
            return x;
        };

        int prev = nearestScalePitch (72 + s.key % 12 > 77 ? 72 + s.key % 12 - 12 : 72 + s.key % 12);
        for (int bar = 0; bar < bars; ++bar)
        {
            const bool answer = (bar % 4) == 3;
            const auto& rhythm = answer ? B : A;
            for (size_t k = 0; k < rhythm.size(); ++k)
            {
                const int st = rhythm[k];
                const double t = bar * 4.0 + st * kSixteenth;
                const int chordIdx = juce::jmin (L.chords - 1, (int) std::floor (t / L.beatsPerChord));
                const auto pcs = chordPcs (s, p, degreeAt (p, chordIdx));
                int pitch;
                if (st % 8 == 0 || k == 0)
                {
                    // strong beat: nearest chord tone
                    // strong beat: nearest chord tone that is also in the melody scale (pentatonic stays pentatonic)
                    auto inScale = [&] (int x) { return std::find (ms.begin(), ms.end(), ((x - s.key) % 12 + 12) % 12) != ms.end(); };
                    int best = nearestScalePitch (prev), bestD = 99;
                    for (int x = prev - 7; x <= prev + 7; ++x)
                        if (inScale (x) && std::find (pcs.begin(), pcs.end(), (x % 12 + 12) % 12) != pcs.end() && std::abs (x - prev) < bestD) { bestD = std::abs (x - prev); best = x; }
                    pitch = best;
                }
                else
                {
                    int move = r.nextInt (5) - 2;                 // -2..+2 scale steps
                    if (move == 0) move = r.nextBool() ? 1 : -1;
                    if (prev > 82) move = -std::abs (move);
                    if (prev < 66) move = std::abs (move);
                    pitch = stepScale (prev, move);
                }
                if (pitch < 62) pitch = nearestScalePitch (pitch + 12);
                if (pitch > 88) pitch = nearestScalePitch (pitch - 12);
                const int nextSt = k + 1 < rhythm.size() ? rhythm[k + 1] : 16;
                double len = (nextSt - st) * kSixteenth;
                if (s.style == Trap || s.style == Drill || s.style == HipHop) len = juce::jmin (len, 0.75);
                len = juce::jmax (0.12, len - 0.02);
                c.notes.push_back ({ t, len, pitch, humanVel (r, st % 4 == 0 ? 0.86f : 0.72f) });
                prev = pitch;
            }
        }
        // land the phrase: last note becomes a long tonic / chord tone
        if (! c.notes.empty())
        {
            auto& last = c.notes.back();
            last.length = juce::jmax (last.length, totalBeats - last.start - 0.02);
            const int tonicPc = s.key % 12;
            for (int x = last.pitch - 6; x <= last.pitch + 6; ++x) if ((x % 12 + 12) % 12 == tonicPc) { last.pitch = x; break; }
        }
    }

    //==========================================================================
    void makeDrums (const Settings& s, Clip& c, juce::Random& r)
    {
        using namespace drums;
        const int bars = s.bars;
        double swing = 0.0;   // offset for odd 16ths, in beats
        if (s.style == HipHop) swing = 0.035;
        if (s.style == LoFi) swing = 0.055;

        auto hit = [&] (int bar, double step, int note, float vel, double len = 0.1)
        {
            double t = bar * 4.0 + step * kSixteenth;
            const double frac = step - std::floor (step);
            if (frac == 0.0 && ((int) step % 2) == 1) t += swing;
            c.notes.push_back ({ t, len, note, humanVel (r, vel, 0.05f) });
        };

        // per-style kick + snare skeletons (16th steps)
        std::vector<std::vector<int>> kicks;
        std::vector<int> snares;
        switch (s.style)
        {
            case Trap:      kicks = { { 0, 10 }, { 0, 7, 10 }, { 0, 3, 10, 14 }, { 0, 11 } }; snares = { 8 }; break;
            case Drill:     kicks = { { 0, 6, 10 }, { 0, 5, 11 }, { 0, 3, 10 } }; snares = { 8 }; break;
            case Wave:      kicks = { { 0, 10, 11 }, { 0, 7 }, { 0, 14 } }; snares = { 8 }; break;
            case HipHop:    kicks = { { 0, 7, 10 }, { 0, 3, 10 }, { 0, 9, 10 } }; snares = { 4, 12 }; break;
            case LoFi:      kicks = { { 0, 10 }, { 0, 6, 10 }, { 0, 3, 8, 10 } }; snares = { 4, 12 }; break;
            default:        kicks = { { 0, 11 }, { 0 } }; snares = { 8 }; break;
        }
        const auto& kickA = kicks[(size_t) r.nextInt ((int) kicks.size())];
        const auto& kickB = kicks[(size_t) r.nextInt ((int) kicks.size())];

        for (int bar = 0; bar < bars; ++bar)
        {
            const bool fillBar = (bar % 4) == 3 || bar == bars - 1;
            const auto& kp = (bar % 2 == 1) ? kickB : kickA;
            for (int st : kp) hit (bar, st, kick, st == 0 ? 0.95f : 0.85f, 0.2);

            for (int st : snares)
            {
                hit (bar, st, snare, 0.9f, 0.15);
                if (s.style == Trap || s.style == Wave || s.style == Emotional) hit (bar, st, clap, 0.85f, 0.15);
            }
            if (s.style == Drill && r.nextFloat() < 0.6f) hit (bar, 13, snare, 0.7f, 0.12);   // drill's late snare
            if ((s.style == HipHop || s.style == LoFi) && r.nextFloat() < 0.5f) hit (bar, 15, snare, 0.35f, 0.08);   // ghost

            // hats
            switch (s.style)
            {
                case Trap:
                case Drill:
                {
                    const int mode = r.nextInt (3);   // 0 = 8ths, 1 = 16ths, 2 = 8ths with 16th doubles
                    for (int st = 0; st < 16; ++st)
                    {
                        if (fillBar && st >= 12) break;
                        const bool on = mode == 1 || st % 2 == 0 || (mode == 2 && (st == 7 || st == 11));
                        if (s.style == Drill && (st == 3 || st == 9) && mode != 1) { hit (bar, st, hatClosed, 0.62f); continue; }   // drill skips
                        if (on) hit (bar, st, hatClosed, st % 4 == 0 ? 0.82f : 0.6f);
                    }
                    if (fillBar)
                    {
                        // rolls: triplet 16ths then a 32nd ratchet into the next bar
                        for (int k = 0; k < 6; ++k) hit (bar, 12.0 + k * (2.0 / 3.0), hatClosed, 0.55f + 0.05f * k, 0.05);
                        for (int k = 0; k < 4; ++k) hit (bar, 14.0 + k * 0.5, hatClosed, 0.65f + 0.06f * k, 0.04);
                    }
                    else if (r.nextFloat() < 0.35f)
                        for (int k = 0; k < 3; ++k) hit (bar, 6.0 + k * (2.0 / 3.0), hatClosed, 0.6f, 0.05);   // a quick triplet flick
                    if (r.nextFloat() < 0.4f) hit (bar, 14, hatOpen, 0.6f, 0.4);
                    break;
                }
                case Wave:
                case Emotional:
                {
                    for (int st = 0; st < 16; st += (s.style == Emotional ? 4 : 2)) hit (bar, st, hatClosed, st % 4 == 0 ? 0.7f : 0.5f);
                    if (fillBar) for (int k = 0; k < 6; ++k) hit (bar, 12.0 + k * (2.0 / 3.0), hatClosed, 0.45f + 0.06f * k, 0.05);
                    hit (bar, 15, hatOpen, 0.45f, 0.5);
                    if (r.nextFloat() < 0.5f) hit (bar, 6, perc, 0.45f, 0.1);
                    break;
                }
                default:   // hip-hop / lo-fi: swung 8ths, open hat on the "and" of 4
                {
                    for (int st = 0; st < 16; st += 2) hit (bar, st, hatClosed, st % 4 == 0 ? 0.72f : 0.5f);
                    if (r.nextFloat() < 0.6f) hit (bar, 14, hatOpen, 0.5f, 0.3);
                    if (s.style == LoFi) { hit (bar, 4, rim, 0.4f); if (r.nextBool()) hit (bar, 11, rim, 0.3f); }
                    break;
                }
            }
        }
        std::sort (c.notes.begin(), c.notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
    }
}

//==============================================================================
const juce::StringArray& partNames()  { static const juce::StringArray a { "CHORDS", "LEAD", "BASS", "DRUMS" }; return a; }
const juce::StringArray& keyNames()   { static const juce::StringArray a { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return a; }
const juce::StringArray& scaleNames() { static const juce::StringArray a { "Minor", "Major", "Harmonic Minor", "Dorian", "Phrygian", "Minor Pentatonic" }; return a; }
const juce::StringArray& styleNames() { static const juce::StringArray a { "Trap", "Wave", "Hip-Hop", "Drill", "Lo-Fi", "Emotional" }; return a; }

Progression makeProgression (int style, juce::uint32 seed)
{
    juce::Random r ((juce::int64) seed * 7919 + style);
    std::vector<std::vector<int>> pool;
    switch (style)
    {
        case Trap:      pool = { { 0, 5, 2, 6 }, { 0, 3, 5, 4 }, { 0, 6, 5, 6 }, { 0, 0, 5, 4 }, { 0, 5, 3, 4 } }; break;
        case Wave:      pool = { { 0, 5, 2, 6 }, { 5, 3, 0, 4 }, { 0, 2, 5, 6 }, { 3, 0, 5, 4 }, { 5, 6, 0, 2 } }; break;
        case HipHop:    pool = { { 1, 4, 0, 0 }, { 0, 3, 6, 2 }, { 3, 4, 0, 5 }, { 0, 5, 1, 4 } }; break;
        case Drill:     pool = { { 0, 5, 4, 0 }, { 0, 1, 0, 6 }, { 0, 6, 5, 4 }, { 0, 3, 1, 0 } }; break;
        case LoFi:      pool = { { 1, 4, 0, 3 }, { 3, 2, 1, 0 }, { 0, 5, 1, 4 }, { 3, 4, 2, 5 } }; break;
        default:        pool = { { 5, 3, 0, 4 }, { 0, 4, 5, 3 }, { 0, 5, 3, 4 }, { 3, 4, 5, 0 } }; break;
    }
    Progression p;
    p.degrees = pool[(size_t) r.nextInt ((int) pool.size())];
    p.sevenths = style == Wave || style == HipHop || style == LoFi;
    return p;
}

juce::String describeProgression (const Settings& s, const Progression& p)
{
    juce::StringArray parts;
    for (int d : p.degrees) parts.add (chordName (s, p, d));
    return parts.joinIntoString (" - ");
}

Clip generate (const Settings& s, const Progression& p)
{
    Clip c;
    c.part = s.part;
    c.lengthBeats = juce::jlimit (2, 8, s.bars) * 4.0;
    juce::Random r ((juce::int64) s.seed * 104729 + (int) s.part * 31 + s.style);
    Settings st = s;
    st.bars = juce::jlimit (2, 8, s.bars);
    switch (s.part)
    {
        case Part::Chords: makeChords (st, p, c, r); break;
        case Part::Lead:   makeLead (st, p, c, r); break;
        case Part::Bass:   makeBass (st, p, c, r); break;
        case Part::Drums:  makeDrums (st, c, r); break;
    }
    for (auto& n : c.notes) n.length = juce::jmax (0.03, juce::jmin (n.length, c.lengthBeats - n.start));
    c.summary = s.part == Part::Drums ? styleNames()[s.style] + " drums" : describeProgression (st, p);
    const juce::String partWord = juce::StringArray { "Chords", "Lead", "Bass", "Drums" }[(int) s.part];
    c.fileStem = "DIVINITY " + styleNames()[s.style] + " " + partWord
               + (s.part == Part::Drums ? juce::String() : " " + keyNames()[s.key] + " " + scaleNames()[s.scale])
               + " " + juce::String (st.bars) + " bars";
    return c;
}

bool writeMidiFile (const Clip& c, double bpm, const juce::File& dest)
{
    constexpr int ppq = 960;
    juce::MidiMessageSequence seq;
    auto meta = juce::MidiMessage::textMetaEvent (3, c.fileStem);   // track name
    meta.setTimeStamp (0); seq.addEvent (meta);
    auto tempo = juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / juce::jlimit (20.0, 400.0, bpm)));
    tempo.setTimeStamp (0); seq.addEvent (tempo);
    auto sig = juce::MidiMessage::timeSignatureMetaEvent (4, 4);
    sig.setTimeStamp (0); seq.addEvent (sig);
    for (auto& n : c.notes)
    {
        const double on = std::round (n.start * ppq), off = std::round ((n.start + n.length) * ppq);
        seq.addEvent (juce::MidiMessage::noteOn (1, n.pitch, juce::jlimit (0.01f, 1.0f, n.velocity)), on);
        seq.addEvent (juce::MidiMessage::noteOff (1, n.pitch), juce::jmax (on + 1.0, off));
    }
    auto end = juce::MidiMessage::endOfTrack();
    end.setTimeStamp (c.lengthBeats * ppq); seq.addEvent (end);
    seq.updateMatchedPairs();
    seq.sort();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (ppq);
    mf.addTrack (seq);
    dest.getParentDirectory().createDirectory();
    dest.deleteFile();
    juce::FileOutputStream out (dest);
    if (! out.openedOk()) return false;
    return mf.writeTo (out, 1);
}

} // namespace divinity::midigen
