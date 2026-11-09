#pragma once

#include "BeatClock.h"

namespace dt
{
struct Pose
{
    float armL = 0.35f; // radians from straight-down, outward is positive
    float armR = 0.35f;
    float legL = 0.10f;
    float legR = 0.10f;
    float lean = 0.0f;  // torso lean, radians
    float wave = 0.0f;  // hand wave amplitude (radians)
};

struct DancePattern
{
    const char* name;
    Pose poses[beatsPerBar];
};

// Three patterns; auto-rotation changes every 2 bars (lock >= 0 pins one).
inline const DancePattern& patternFor (long long barIndex, int lock)
{
    static const DancePattern patterns[] =
    {
        { "摆动",
          { { 0.95f, 0.20f, 0.16f, 0.02f, -0.06f, 0.0f },
            { 0.20f, 0.95f, 0.02f, 0.16f,  0.06f, 0.0f },
            { 0.95f, 0.20f, 0.16f, 0.02f, -0.06f, 0.0f },
            { 0.20f, 0.95f, 0.02f, 0.16f,  0.06f, 0.0f } } },
        { "挥手",
          { { 2.40f, 2.40f, 0.06f, 0.06f, 0.0f, 0.45f },
            { 2.70f, 2.70f, 0.12f, 0.12f, 0.0f, 0.90f },
            { 2.40f, 2.40f, 0.06f, 0.06f, 0.0f, 0.45f },
            { 2.10f, 2.10f, 0.12f, 0.12f, 0.0f, 0.90f } } },
        { "踏步",
          { { 0.50f, 1.40f, 0.22f, 0.00f, -0.08f, 0.0f },
            { 1.40f, 0.50f, 0.00f, 0.22f,  0.08f, 0.0f },
            { 0.70f, 1.60f, 0.26f, 0.00f, -0.06f, 0.0f },
            { 1.60f, 0.70f, 0.00f, 0.26f,  0.06f, 0.0f } } },
    };
    constexpr int n = (int) (sizeof (patterns) / sizeof (patterns[0]));

    if (lock >= 0 && lock < n)
        return patterns[lock];

    const long long m = (((barIndex / 2) % n) + n) % n;
    return patterns[(int) m];
}

inline Pose lerpPose (const Pose& a, const Pose& b, float t)
{
    Pose r;
    r.armL = a.armL + (b.armL - a.armL) * t;
    r.armR = a.armR + (b.armR - a.armR) * t;
    r.legL = a.legL + (b.legL - a.legL) * t;
    r.legR = a.legR + (b.legR - a.legR) * t;
    r.lean = a.lean + (b.lean - a.lean) * t;
    r.wave = a.wave + (b.wave - a.wave) * t;
    return r;
}

} // namespace dt
