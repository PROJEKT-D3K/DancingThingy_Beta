#pragma once

#include <juce_core/juce_core.h>

#include "BeatClock.h"
#include "WasapiLoopbackCapture.h"

#include <atomic>

namespace dt
{

// Fallback beat source: listens to whatever the system plays through the
// default output device (WASAPI loopback) and derives an approximate beat
// from onsets.
//  - an onset "is" the current quarter note
//  - tempo is estimated from smoothed inter-onset intervals
//
// The capture plumbing lives in WasapiLoopbackCapture; everything below is
// the detection maths and can be started/stopped at will so the menu can
// switch beat sources without recreating the overlay.
class LoopbackListener
{
public:
    LoopbackListener();
    ~LoopbackListener();

    bool start();   // false -> getLastError() explains why
    void stop();

    bool   isRunning() const { return capture.isRunning(); }
    double getLastLevel() const { return lastLevel.load(); }

    juce::String getLastError() const
    {
        return lastError.isNotEmpty() ? lastError : capture.getLastError();
    }
    juce::String getDeviceName() const { return capture.getDeviceName(); }

    BeatState getBeatState() const;

private:
    void processAudioBlock (const float* interleaved, int numFrames, int numChannels);
    void handleOnset (double ms);
    void resetDetection();

    WasapiLoopbackCapture capture;

    // audio-thread-only detection state
    double history[64] {};
    int    historyIdx = 0;
    int    historyFilled = 0;
    double lastDetectedOnsetMs = -1000.0;
    double smoothedIntervalMs = 0.0;

    // shared state
    std::atomic<double>    lastLevel       { 0.0 };
    std::atomic<double>    estimatedBpm    { 120.0 };
    std::atomic<double>    lastOnsetTimeMs { -1000.0 };
    std::atomic<long long> onsetCount      { 0 };

    juce::String lastError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopbackListener)
};

} // namespace dt
