#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "BeatClock.h"
#include "BeatProtocol.h"

#include <atomic>
#include <memory>

namespace dt
{
class DanceSenderThread;

// Passthrough effect: never touches the samples. Reads the host playhead
// every block and lets a background thread broadcast the beat to the
// DanceOverlay window app over local UDP.
class DanceThingyAudioProcessor : public juce::AudioProcessor
{
public:
    DanceThingyAudioProcessor();
    ~DanceThingyAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                        { return true; }
    const juce::String getName() const override             { return "DanceThingy"; }
    bool acceptsMidi() const override                       { return false; }
    bool producesMidi() const override                      { return false; }
    bool isMidiEffect() const override                      { return false; }
    double getTailLengthSeconds() const override            { return 0.0; }

    int getNumPrograms() override                            { return 1; }
    int getCurrentProgram() override                         { return 0; }
    void setCurrentProgram (int) override                    {}
    const juce::String getProgramName (int) override         { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override   {}
    void setStateInformation (const void*, int) override     {}

    TransportSnapshot getSnapshot() const { return snapshot.load (std::memory_order_relaxed); }

private:
    std::atomic<TransportSnapshot> snapshot { TransportSnapshot{} };
    std::unique_ptr<DanceSenderThread> sender;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DanceThingyAudioProcessor)
};

} // namespace dt
