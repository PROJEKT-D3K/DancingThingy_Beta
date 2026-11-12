#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

namespace dt
{

// Minimal status panel: transport detection, the UDP heartbeat target and the
// UI language (shared with the overlay through %APPDATA%\DanceOverlay\language.txt).
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
    void retranslate();
    void refreshStatus();

    DanceThingyAudioProcessor& processor;

    juce::Label statusLabel;
    juce::Label detailLabel;
    juce::Label hintLabel;

    juce::Label      languageLabel;
    juce::ComboBox   languageBox;

    int languagePollTicks = 0;
    int lastShownPort = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DanceThingyAudioProcessorEditor)
};

} // namespace dt
