#pragma once

#include <cmath>
#include <cstdint>

namespace dt
{
// What the companion VST captures from the host playhead each audio block.
// Trivially copyable: kept in std::atomic and shipped over UDP as text.
struct TransportSnapshot
{
    bool     valid        = false;  // host actually provided transport info
    bool     isPlaying    = false;
    double   bpm          = 0.0;
    double   ppqPosition  = -1.0;   // quarter notes since song start
    double   barStartPpq  = -1.0;
    uint32_t timestampMs  = 0;
};

constexpr int beatsPerBar = 4;

// Derived dance state consumed by the renderer.
struct BeatState
{
    bool      playing    = false;
    bool      hostSynced = false; // true = host transport, false = self-listen
    double    beatPhase  = 0.0;   // 0..1 inside the current quarter note
    long long quarterIndex = 0;    // quarter notes since song start
    long long barIndex     = 0;
    int       beatInBar    = 0;    // 0..3
    double    bpm          = 0.0;
};

inline long long floorDivLL (long long a, long long b)
{
    long long q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0)))
        --q;
    return q;
}

inline BeatState deriveBeatState (const TransportSnapshot& s)
{
    BeatState b;
    b.playing    = s.valid && s.isPlaying;
    b.hostSynced = true;
    if (! b.playing)
        return b;

    b.bpm          = s.bpm > 0.0 ? s.bpm : 120.0;
    b.quarterIndex = (long long) std::floor (s.ppqPosition);
    b.beatPhase    = s.ppqPosition - (double) b.quarterIndex;
    if (b.beatPhase < 0.0) b.beatPhase = 0.0;
    if (b.beatPhase > 1.0) b.beatPhase = 1.0;
    b.barIndex  = floorDivLL (b.quarterIndex, beatsPerBar);
    b.beatInBar = (int) (b.quarterIndex - b.barIndex * beatsPerBar);
    return b;
}

} // namespace dt
