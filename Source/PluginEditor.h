#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

namespace dt
{

// Minimal status panel: transport detection + UDP heartbeat info.
class DanceThingyAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit DanceThingyAudioProcessorEditor (DanceThingyAudioProcessor& p);
    ~DanceThingyAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    DanceThingyAudioProcessor& processor;

    juce::Label statusLabel;
    juce::Label detailLabel;
    juce::Label hintLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DanceThingyAudioProcessorEditor)
};

} // namespace dt
