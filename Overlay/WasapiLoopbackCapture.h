#pragma once

#include <juce_core/juce_core.h>

#include <functional>
#include <memory>

namespace dt
{

// Captures whatever the system plays through the DEFAULT output device via
// WASAPI loopback (AUDCLNT_STREAMFLAGS_LOOPBACK).
//
// JUCE's AudioDeviceManager cannot do this on Windows: its input device list
// only contains capture endpoints (eCapture), and nothing in juce_audio_devices
// requests the render-side loopback stream. So this class talks to WASAPI
// directly.
//
// All COM work happens on the capture thread (MTA). Doing it on the caller's
// STA thread deadlocks inside IAudioClient::Initialize when no message pump is
// running yet, which is exactly the situation during overlay construction.
//
// Blocks arrive as interleaved frames normalised to [-1, 1] floats. Silent
// packets are delivered as zeros (not skipped) so the caller's level history
// can decay.
class WasapiLoopbackCapture
{
public:
    using BlockCallback = std::function<void (const float* interleaved,
                                             int numFrames,
                                             int numChannels,
                                             double sampleRate)>;

    WasapiLoopbackCapture();
    ~WasapiLoopbackCapture();

    // Opens the default render endpoint and starts pushing blocks to cb.
    // Returns false (and sets getLastError()) when the device cannot be opened.
    bool start (BlockCallback cb);
    void stop();

    bool isRunning() const;
    juce::String getLastError() const;    // last failure, empty if healthy
    juce::String getDeviceName() const;   // friendly name of the captured endpoint
    double getSampleRate() const;

private:
    struct Impl;
    // shared: a capture thread wedged inside a COM call keeps its Impl alive
    // after we abandon it, so the destructor can never pull the rug out.
    std::shared_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WasapiLoopbackCapture)
};

} // namespace dt
