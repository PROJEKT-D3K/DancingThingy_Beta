#include "LoopbackListener.h"

#include <cmath>

namespace dt
{

LoopbackListener::LoopbackListener() = default;
LoopbackListener::~LoopbackListener() { capture.stop(); }

void LoopbackListener::resetDetection()
{
    historyIdx          = 0;
    historyFilled       = 0;
    lastDetectedOnsetMs = -1000.0;
    smoothedIntervalMs  = 0.0;
    onsetCount.store (0);
}

bool LoopbackListener::start()
{
    if (capture.isRunning())
        return true;

    juce::Logger::writeToLog ("[Loopback] start requested");

    // A restart (device change, menu toggle) should begin from a clean slate
    // rather than inherit half a bar of stale onsets.
    resetDetection();

    const bool ok = capture.start ([this] (const float* data, int frames, int ch, double)
                                   {
                                       processAudioBlock (data, frames, ch);
                                   });

    if (! ok)
    {
        lastError = capture.getLastError();
        juce::Logger::writeToLog ("[Loopback] start failed: " + lastError);
        return false;
    }

    // The device opens asynchronously; Impl::run logs the endpoint once it is
    // actually capturing.
    lastError.clear();
    return true;
}

void LoopbackListener::stop()
{
    capture.stop();
}

void LoopbackListener::processAudioBlock (const float* interleaved, int numFrames, int numChannels)
{
    if (interleaved == nullptr || numFrames <= 0 || numChannels <= 0)
        return;

    const int n = numFrames * numChannels;

    double sum = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double v = (double) interleaved[i];
        sum += v * v;
    }

    const double rms = std::sqrt (sum / (double) n);
    lastLevel.store (rms);

    // adaptive threshold against the recent rolling average
    double mean = 0.0;
    for (int i = 0; i < historyFilled; ++i)
        mean += history[i];
    if (historyFilled > 0)
        mean /= (double) historyFilled;

    history[historyIdx] = rms;
    historyIdx = (historyIdx + 1) % 64;
    historyFilled = juce::jlimit (0, 64, historyFilled + 1);

    const double ms = juce::Time::getMillisecondCounterHiRes();
    const bool loudEnough   = rms > 0.008;
    const bool aboveAverage = rms > mean * 1.6 + 0.002;
    const bool wellSpaced   = (ms - lastDetectedOnsetMs) > 180.0;

    if (loudEnough && aboveAverage && wellSpaced)
        handleOnset (ms);
}

void LoopbackListener::handleOnset (double ms)
{
    lastDetectedOnsetMs = ms;

    if (smoothedIntervalMs <= 0.0)
        smoothedIntervalMs = ms - lastOnsetTimeMs.load();
    else
        smoothedIntervalMs = 0.7 * smoothedIntervalMs + 0.3 * (ms - lastOnsetTimeMs.load());

    if (smoothedIntervalMs > 0.0)
    {
        double bpm = 60000.0 / smoothedIntervalMs;
        while (bpm < 60.0)  bpm *= 2.0;
        while (bpm > 240.0) bpm /= 2.0;
        if (bpm >= 60.0 && bpm <= 240.0)
            estimatedBpm.store (bpm);
    }

    lastOnsetTimeMs.store (ms);
    onsetCount.fetch_add (1);
}

BeatState LoopbackListener::getBeatState() const
{
    BeatState b;
    b.hostSynced = false;

    const double now        = juce::Time::getMillisecondCounterHiRes();
    const double sinceOnset = now - lastOnsetTimeMs.load();

    if (sinceOnset > 2200.0)   // silent for a while -> show as idle
        return b;

    b.playing     = true;
    b.bpm         = juce::jlimit (60.0, 240.0, estimatedBpm.load());
    const double beats = sinceOnset * b.bpm / 60000.0;
    b.quarterIndex = (long long) onsetCount.load() + (long long) std::floor (beats);
    b.beatPhase    = beats - std::floor (beats);
    b.barIndex     = floorDivLL (b.quarterIndex, beatsPerBar);
    b.beatInBar    = (int) (b.quarterIndex - b.barIndex * beatsPerBar);
    return b;
}

} // namespace dt
