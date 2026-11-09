#pragma once

#include <juce_graphics/juce_graphics.h>

#include "BeatClock.h"
#include "Choreography.h"

#include <cmath>
#include <utility>

namespace dt
{
// ---- palette: white / blue / gold vestal style (original character) ----
inline juce::Colour dancerSkin()       { return juce::Colour (0xffF7DFC8); }
inline juce::Colour dancerHair()       { return juce::Colour (0xffE9BC57); }
inline juce::Colour dancerDress()      { return juce::Colour (0xff6FA8DC); }
inline juce::Colour dancerDressLight() { return juce::Colour (0xffA9CCE8); }
inline juce::Colour dancerTrim()       { return juce::Colour (0xffFDF7EA); }
inline juce::Colour dancerStocking()   { return juce::Colour (0xffFCFCF4); }
inline juce::Colour dancerShoe()       { return juce::Colour (0xff4F81BD); }
inline juce::Colour dancerLine()       { return juce::Colour (0xff5B4632); }
inline juce::Colour dancerBlush()      { return juce::Colour (0x33E88A8A); }
inline juce::Colour dancerSparkle()    { return juce::Colour (0xffF2C94C); }

// Rounded-capsule limb: line with round end caps.
inline void drawLimb (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b,
                      float thickness, juce::Colour c)
{
    g.setColour (c);
    juce::Path p;
    p.addLineSegment ({ a, b }, thickness);
    g.fillPath (p);
    g.fillEllipse (juce::Rectangle<float> (thickness, thickness).withCentre (a));
    g.fillEllipse (juce::Rectangle<float> (thickness, thickness).withCentre (b));
}

inline float easeSmooth (float t)
{
    return t * t * (3.0f - 2.0f * t);
}

inline juce::Point<float> armEndFor (juce::Point<float> shoulder, float angleRad,
                                     float len, float side, float waveRad)
{
    const float a = angleRad + waveRad;
    return { shoulder.x + side * std::sin (a) * len,
             shoulder.y + std::cos (a) * len };
}

// Draws the procedural placeholder dancer onto a transparent ARGB context.
// Design size is 240 x 360; everything scales with the drawing size.
//
// bs          : beat state (host-synced, self-detected, or idle)
// nowMs       : wall clock, drives idle sway / blinking
// patternLock : -1 = auto-rotate, 0..2 = pin one dance pattern
inline void drawDancer (juce::Graphics& g, int w, int h, const BeatState& bs,
                        double nowMs, int patternLock = -1)
{
    // The upper bound must cover the overlay's user size factor (up to 4x on
    // top of the 240x360 design size); below 1.0 the drawing shrinks to fit.
    const float s       = juce::jlimit (0.35f, 8.0f, juce::jmin ((float) w / 240.0f, (float) h / 360.0f));
    const float cx      = (float) w * 0.5f;
    const float groundY = (float) h - 24.0f * s;
    const float t       = (float) std::fmod (nowMs, 1000000.0);

    float airborne = 0.0f;  // 0 = on the floor, 1 = at the bounce peak
    bool  downbeat = false;
    Pose  pose;

    if (bs.playing)
    {
        const DancePattern& pat = patternFor (bs.barIndex, patternLock);
        const int idx = juce::jlimit (0, beatsPerBar - 1, bs.beatInBar);
        const int nxt = (idx + 1) % beatsPerBar;

        float blend = 0.0f;
        if (bs.beatPhase > 0.7)
            blend = easeSmooth ((float) ((bs.beatPhase - 0.7) / 0.3));
        pose = lerpPose (pat.poses[idx], pat.poses[nxt], blend);

        airborne = std::abs (std::sin (juce::MathConstants<float>::pi * (float) bs.beatPhase));
        downbeat = (bs.beatInBar == 0) && (bs.beatPhase < 0.35);
    }
    else
    {
        pose.armL = 0.30f + 0.06f * std::sin (t * 0.0011f);
        pose.armR = 0.30f + 0.06f * std::sin (t * 0.0011f + 1.7f);
        pose.legL = 0.07f;
        pose.legR = 0.07f;
        pose.lean = 0.035f * std::sin (t * 0.0007f);
        pose.wave = 0.0f;
    }

    const float bounce   = airborne * 30.0f * s;                 // vertical lift
    const float stretchY = 1.0f + 0.13f * airborne;              // taller mid-air
    const float squeezeX = 1.0f / (1.0f + 0.22f * airborne);     // squash on landing

    // ---- shadow (shrinks and fades while airborne) ----
    {
        const float k  = 1.0f - 0.45f * airborne;
        const float sw = 46.0f * s * k;
        g.setColour (juce::Colours::black.withAlpha (0.28f * k));
        g.fillEllipse (cx - sw * 0.5f, groundY - 6.0f * s, sw, 9.0f * s * k);
    }

    // ---- rig ----
    const float legLen  = 46.0f * s;
    const float bodyLen = 88.0f * s;
    const float headR   = 26.0f * s;
    const float armLen  = 40.0f * s;

    const float hipY  = groundY - legLen - bounce;
    const float neckY = hipY - bodyLen * stretchY;
    const float leanX = std::sin (pose.lean) * bodyLen * 0.5f;
    const float headX = cx + leanX;
    const float headY = neckY - headR * 1.05f * stretchY;

    // legs + shoes
    const juce::Point<float> hipL { cx - 9.0f * s, hipY };
    const juce::Point<float> hipR { cx + 9.0f * s, hipY };
    const juce::Point<float> footL { cx - (9.0f + 26.0f * pose.legL) * s, groundY - bounce * 0.15f };
    const juce::Point<float> footR { cx + (9.0f + 26.0f * pose.legR) * s, groundY - bounce * 0.15f };
    drawLimb (g, hipL, footL, 9.0f * s, dancerStocking());
    drawLimb (g, hipR, footR, 9.0f * s, dancerStocking());

    g.setColour (dancerShoe());
    g.fillEllipse (juce::Rectangle<float> (17.0f * s, 10.0f * s).withCentre (footL));
    g.fillEllipse (juce::Rectangle<float> (17.0f * s, 10.0f * s).withCentre (footR));

    // dress (trapezoid, flares mid-air)
    {
        juce::Path dress;
        const float flare   = 1.0f + 0.35f * airborne;
        const float hemHalf = 36.0f * s * flare;
        const float topHalf = 14.0f * s;
        const float topY    = hipY - bodyLen * 0.45f;
        const float hemY    = hipY + 15.0f * s;
        dress.startNewSubPath (cx - topHalf, topY);
        dress.lineTo (cx + topHalf, topY);
        dress.lineTo (cx + hemHalf, hemY);
        dress.lineTo (cx - hemHalf, hemY);
        dress.closeSubPath();
        g.setColour (dancerDress());
        g.fillPath (dress);

        juce::Path hem;
        hem.addLineSegment ({ juce::Point<float> { cx - hemHalf, hemY },
                              juce::Point<float> { cx + hemHalf, hemY } }, 4.0f * s);
        g.setColour (dancerDressLight());
        g.fillPath (hem);
    }

    // torso + collar
    const juce::Point<float> torsoTop { cx + leanX * 0.55f, neckY + headR * 0.45f };
    g.setColour (dancerTrim());
    drawLimb (g, { cx, hipY }, torsoTop, 20.0f * s, dancerTrim());
    g.setColour (dancerDress());
    g.fillEllipse (juce::Rectangle<float> (26.0f * s, 13.0f * s).withCentre (torsoTop));

    // arms + hands
    const juce::Point<float> shoulderL { cx - 10.0f * s, neckY + headR * 0.55f };
    const juce::Point<float> shoulderR { cx + 10.0f * s, neckY + headR * 0.55f };
    const float waveL = pose.wave * std::sin (t * 0.012f);
    const float waveR = pose.wave * std::sin (t * 0.012f + 2.1f);
    drawLimb (g, shoulderL, armEndFor (shoulderL, pose.armL, armLen, -1.0f, waveL), 8.0f * s, dancerTrim());
    drawLimb (g, shoulderR, armEndFor (shoulderR, pose.armR, armLen, +1.0f, waveR), 8.0f * s, dancerTrim());

    g.setColour (dancerSkin());
    g.fillEllipse (juce::Rectangle<float> (13.0f * s, 13.0f * s)
                       .withCentre (armEndFor (shoulderL, pose.armL, armLen, -1.0f, waveL)));
    g.fillEllipse (juce::Rectangle<float> (13.0f * s, 13.0f * s)
                       .withCentre (armEndFor (shoulderR, pose.armR, armLen, +1.0f, waveR)));

    // back hair
    g.setColour (dancerHair().withAlpha (0.9f));
    g.fillEllipse (juce::Rectangle<float> (headR * 2.5f, headR * 3.0f)
                       .withCentre ({ headX, headY + headR * 0.35f }));

    // face
    g.setColour (dancerSkin());
    g.fillEllipse (juce::Rectangle<float> (headR * 1.85f, headR * 1.9f * stretchY)
                       .withCentre ({ headX, headY + headR * 0.12f }));

    // fringe
    g.setColour (dancerHair());
    g.fillEllipse (juce::Rectangle<float> (headR * 1.95f, headR * 1.2f)
                       .withCentre ({ headX, headY - headR * 0.45f }));

    // eyes (blink ~every 4.6 s), blush, smile
    {
        const bool  blink = std::fmod (nowMs, 4600.0) < 150.0;
        const float eyeY  = headY + headR * 0.22f;
        const float eyeDX = headR * 0.38f;
        const juce::Point<float> eyeL { headX - eyeDX, eyeY };
        const juce::Point<float> eyeR { headX + eyeDX, eyeY };

        if (blink)
        {
            drawLimb (g, eyeL, { eyeL.x + 7.0f * s, eyeL.y }, 2.6f * s, dancerLine());
            drawLimb (g, { eyeR.x - 7.0f * s, eyeR.y }, eyeR, 2.6f * s, dancerLine());
        }
        else
        {
            g.setColour (dancerLine());
            g.fillEllipse (juce::Rectangle<float> (7.0f * s, 9.5f * s).withCentre (eyeL));
            g.fillEllipse (juce::Rectangle<float> (7.0f * s, 9.5f * s).withCentre (eyeR));
        }

        g.setColour (dancerBlush());
        g.fillEllipse (juce::Rectangle<float> (9.0f * s, 5.0f * s).withCentre ({ eyeL.x - 5.0f * s, eyeY + 7.0f * s }));
        g.fillEllipse (juce::Rectangle<float> (9.0f * s, 5.0f * s).withCentre ({ eyeR.x + 5.0f * s, eyeY + 7.0f * s }));

        const float mouthY = headY + headR * 0.6f;
        drawLimb (g, { headX - 6.0f * s, mouthY }, { headX + 6.0f * s, mouthY }, 2.6f * s, dancerLine());
    }

    // downbeat sparkles on beat 1 of each bar
    if (downbeat)
    {
        const float k = juce::jlimit (0.0f, 1.0f, 1.0f - (float) (bs.beatPhase / 0.35));
        const juce::Colour sp = dancerSparkle().withAlpha (0.85f * k);
        const float r = (0.4f + 0.8f * k) * 5.0f * s;

        for (const auto& p : { std::make_pair (-1.7f, -0.95f),
                               std::make_pair ( 1.7f, -1.15f),
                               std::make_pair (-1.95f, 0.2f) })
        {
            const float px = headX + p.first  * headR;
            const float py = headY + p.second * headR;
            drawLimb (g, { px - r, py }, { px + r, py }, 1.8f * s, sp);
            drawLimb (g, { px, py - r }, { px, py + r }, 1.8f * s, sp);
        }
    }
}

} // namespace dt
