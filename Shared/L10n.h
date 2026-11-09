#pragma once

#include <juce_core/juce_core.h>

namespace dt
{
// Narrow literals compiled with /utf-8 carry UTF-8 bytes, but juce::String's
// char* constructor deliberately treats 8-bit data as ASCII/CP1252 (see
// juce_String.cpp). Wrap user-visible literals like this instead:
//   dt::u8 ("点击穿透")
inline juce::String u8 (const char* s)
{
    return juce::String (juce::CharPointer_UTF8 (s));
}

} // namespace dt
