#pragma once

#include <juce_core/juce_core.h>

namespace dt
{

// ---------------------------------------------------------------------------
// One on-screen character: everything that belongs to that window alone.
//
// v0.4.0 keeps one of these per DancerWindow and persists the whole list as a
// JSON array under the "dancers" key of settings.xml. Global choices (beat
// source, click-through, language, UDP port...) live on OverlaySession.
// ---------------------------------------------------------------------------
struct DancerState
{
    juce::String characterPath;   // empty = built-in procedural figure
    int    pattern     = -1;      // procedural figure: -1 auto, 0 sway, 1 wave, 2 step
    int    rowLock     = -1;      // -1 = auto-cycle dance rows
    double speed       = 1.0;     // 0.25 .. 4.0 (sprite sheets only)
    int    sizePercent = 100;     // 25 .. 400
    bool   mirror      = false;
    bool   blend       = false;
    int    x = -9999, y = -9999;  // -9999 = "no saved position"

    juce::var toVar() const
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("path",   characterPath);
        o->setProperty ("pattern", pattern);
        o->setProperty ("row",    rowLock);
        o->setProperty ("speed",  speed);
        o->setProperty ("size",   sizePercent);
        o->setProperty ("mirror", mirror);
        o->setProperty ("blend",  blend);
        o->setProperty ("x",      x);
        o->setProperty ("y",      y);
        return juce::var (o);
    }

    static int intProp (const juce::DynamicObject& o, const char* name, int fallback)
    {
        return o.hasProperty (name) ? (int) o.getProperty (name) : fallback;
    }

    static double doubleProp (const juce::DynamicObject& o, const char* name, double fallback)
    {
        return o.hasProperty (name) ? (double) o.getProperty (name) : fallback;
    }

    static bool boolProp (const juce::DynamicObject& o, const char* name, bool fallback)
    {
        return o.hasProperty (name) ? (bool) o.getProperty (name) : fallback;
    }

    static DancerState fromVar (const juce::var& v)
    {
        DancerState s;

        if (auto* o = v.getDynamicObject())
        {
            s.characterPath = o->getProperty ("path").toString();
            s.pattern     = intProp (*o, "pattern", -1);
            s.rowLock     = intProp (*o, "row", -1);
            s.speed       = doubleProp (*o, "speed", 1.0);
            s.sizePercent = intProp (*o, "size", 100);
            s.mirror      = boolProp (*o, "mirror", false);
            s.blend       = boolProp (*o, "blend", false);
            s.x           = intProp (*o, "x", -9999);
            s.y           = intProp (*o, "y", -9999);
        }

        return s;
    }
};

inline juce::String serialiseDancers (const juce::Array<DancerState>& list)
{
    juce::Array<juce::var> arr;

    for (const auto& d : list)
        arr.add (d.toVar());

    return juce::JSON::toString (juce::var (arr), false);
}

inline juce::Array<DancerState> deserialiseDancers (const juce::String& json)
{
    juce::Array<DancerState> out;

    const auto parsed = juce::JSON::parse (json);
    if (const auto* arr = parsed.getArray())
        for (const auto& v : *arr)
            out.add (DancerState::fromVar (v));

    return out;
}

} // namespace dt
