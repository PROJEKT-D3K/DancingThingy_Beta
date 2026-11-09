#pragma once

#include <juce_core/juce_core.h>

#include "BeatClock.h"

namespace dt
{
// Beat packet (plain text, easy to debug):
//   DTB1;<valid>;<playing>;<bpm>;<ppq>;<barStartPpq>;<timestampMs>
constexpr int      defaultPort    = 53711;
constexpr char     packetHeader[] = "DTB1";
constexpr int      maxPacketBytes = 256;
constexpr uint32_t hostTimeoutMs  = 500;   // packets older than this -> self-listen fallback

inline juce::String makePacket (const TransportSnapshot& s)
{
    return juce::String (packetHeader)
         + ";" + (s.valid     ? "1" : "0")
         + ";" + (s.isPlaying ? "1" : "0")
         + ";" + juce::String (s.bpm, 4)
         + ";" + juce::String (s.ppqPosition, 6)
         + ";" + juce::String (s.barStartPpq, 6)
         + ";" + juce::String ((juce::int64) s.timestampMs);
}

inline bool parsePacket (const juce::String& text, TransportSnapshot& out)
{
    const auto t = text.trim();
    if (! t.startsWith (packetHeader))
        return false;

    const auto parts = juce::StringArray::fromTokens (t, ";", "");
    if (parts.size() < 7)
        return false;

    out.valid       = parts[1].getIntValue() != 0;
    out.isPlaying   = parts[2].getIntValue() != 0;
    out.bpm         = parts[3].getDoubleValue();
    out.ppqPosition = parts[4].getDoubleValue();
    out.barStartPpq = parts[5].getDoubleValue();
    out.timestampMs = (uint32_t) parts[6].getLargeIntValue();
    return true;
}

} // namespace dt
