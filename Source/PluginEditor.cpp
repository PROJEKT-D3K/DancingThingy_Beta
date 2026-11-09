#include "PluginEditor.h"

#include "L10n.h"

namespace dt
{

DanceThingyAudioProcessorEditor::DanceThingyAudioProcessorEditor (DanceThingyAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (360, 216);

    statusLabel.setText (u8 ("等待宿主 transport 信息…"), juce::dontSendNotification);
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
    addAndMakeVisible (statusLabel);

    detailLabel.setJustificationType (juce::Justification::centredLeft);
    detailLabel.setFont (juce::Font (juce::FontOptions (14.0f)));
    addAndMakeVisible (detailLabel);

    hintLabel.setText (u8 (" DanceOverlay 悬浮窗启动后，小人就会踩着这里的拍子跳舞。\n"
                           " UDP -> 127.0.0.1:53711（15 ms 心跳）"),
                       juce::dontSendNotification);
    hintLabel.setJustificationType (juce::Justification::centredLeft);
    hintLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    hintLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (hintLabel);

    startTimerHz (5);
}

DanceThingyAudioProcessorEditor::~DanceThingyAudioProcessorEditor()
{
    stopTimer();
}

void DanceThingyAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff22252d));

    g.setColour (juce::Colour (0xffa9cce8));
    g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    g.drawText ("DanceThingy - beat courier", getLocalBounds().removeFromTop (34),
                juce::Justification::centredLeft, true);

    g.setColour (juce::Colour (0xff465068));
    g.drawHorizontalLine (34, 0.0f, (float) getWidth());
}

void DanceThingyAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (14);
    bounds.removeFromTop (30);

    statusLabel.setBounds (bounds.removeFromTop (28));
    detailLabel.setBounds (bounds.removeFromTop (34));
    hintLabel.setBounds (bounds.removeFromBottom (64));
}

void DanceThingyAudioProcessorEditor::timerCallback()
{
    const auto s = processor.getSnapshot();

    if (! s.valid)
    {
        statusLabel.setText (u8 ("宿主未提供 transport 信息"), juce::dontSendNotification);
        detailLabel.setText (u8 ("有些宿主对普通效果器隐藏播放头；悬浮窗会退回自听模式。"),
                             juce::dontSendNotification);
        return;
    }

    statusLabel.setText (s.isPlaying ? u8 ("● 播放中 - 正在广播节拍")
                                     : u8 ("○ 已暂停 - 心跳仍在对时"),
                         juce::dontSendNotification);

    juce::String detail;
    detail << "BPM " << juce::String (s.bpm, 1)
           << "     " << u8 ("拍位") << " " << juce::String (s.ppqPosition, 2)
           << "     " << u8 ("小节") << " " << (floorDivLL ((long long) std::floor (s.ppqPosition), beatsPerBar) + 1);
    detailLabel.setText (detail, juce::dontSendNotification);
}

} // namespace dt
