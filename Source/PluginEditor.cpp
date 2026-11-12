#include "PluginEditor.h"

#include "AppConfig.h"
#include "L10n.h"

namespace dt
{

DanceThingyAudioProcessorEditor::DanceThingyAudioProcessorEditor (DanceThingyAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (380, 250);

    // Follow the language the overlay (or a previous session) persisted.
    setLanguage (loadLanguage());

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
    addAndMakeVisible (statusLabel);

    detailLabel.setJustificationType (juce::Justification::centredLeft);
    detailLabel.setFont (juce::Font (juce::FontOptions (14.0f)));
    addAndMakeVisible (detailLabel);

    hintLabel.setJustificationType (juce::Justification::centredLeft);
    hintLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    hintLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (hintLabel);

    languageLabel.setJustificationType (juce::Justification::centredRight);
    languageLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    languageLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (languageLabel);

    languageBox.addItem ("中文", 1);
    languageBox.addItem ("English", 2);
    languageBox.addItem (u8 ("日本語"), 3);
    languageBox.setSelectedId ((int) getLanguage() + 1, juce::dontSendNotification);
    languageBox.onChange = [this]
    {
        const auto lang = (Language) juce::jlimit (0, 2, languageBox.getSelectedId() - 1);

        if (lang != getLanguage())
        {
            setLanguage (lang);
            storeLanguage (lang);   // the overlay picks this up on its own poll
            retranslate();
        }
    };
    addAndMakeVisible (languageBox);

    startTimerHz (5);

    retranslate();
    refreshStatus();
}

DanceThingyAudioProcessorEditor::~DanceThingyAudioProcessorEditor()
{
    stopTimer();
}

void DanceThingyAudioProcessorEditor::retranslate()
{
    languageLabel.setText (tr (Text::pluginLanguage), juce::dontSendNotification);

    languageBox.changeItemText (1, u8 ("中文"));
    languageBox.changeItemText (2, "English");
    languageBox.changeItemText (3, u8 ("日本語"));
    languageBox.setSelectedId ((int) getLanguage() + 1, juce::dontSendNotification);

    lastShownPort = -1;   // forces the hint line to re-render with the new text
    refreshStatus();
}

void DanceThingyAudioProcessorEditor::refreshStatus()
{
    const auto port = processor.getTargetPort();

    if (port != lastShownPort)
    {
        lastShownPort = port;
        hintLabel.setText (trf (Text::pluginHint, { juce::String (port) }),
                           juce::dontSendNotification);
    }

    const auto s = processor.getSnapshot();

    if (! s.valid)
    {
        statusLabel.setText (tr (Text::pluginNoTransport), juce::dontSendNotification);
        detailLabel.setText (tr (Text::pluginNoTransportDetail), juce::dontSendNotification);
        return;
    }

    statusLabel.setText (s.isPlaying ? tr (Text::pluginPlaying)
                                     : tr (Text::pluginPaused),
                         juce::dontSendNotification);

    juce::String detail;
    detail << "BPM " << juce::String (s.bpm, 1)
           << "     " << tr (Text::pluginBeat) << " " << juce::String (s.ppqPosition, 2)
           << "     " << tr (Text::pluginBar) << " "
           << (floorDivLL ((long long) std::floor (s.ppqPosition), beatsPerBar) + 1);

    detailLabel.setText (detail, juce::dontSendNotification);
}

void DanceThingyAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff22252d));

    g.setColour (juce::Colour (0xffa9cce8));
    g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    g.drawText ("DanceThingy - beat courier", getLocalBounds().reduced (14).removeFromTop (30),
                juce::Justification::centredLeft, true);

    g.setColour (juce::Colour (0xff465068));
    g.drawHorizontalLine (40, 14.0f, (float) getWidth() - 14.0f);
}

void DanceThingyAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (14);

    auto titleRow = bounds.removeFromTop (30);
    languageBox.setBounds (titleRow.removeFromRight (100));
    languageLabel.setBounds (titleRow.removeFromRight (52));

    bounds.removeFromTop (10);

    statusLabel.setBounds (bounds.removeFromTop (28));
    detailLabel.setBounds (bounds.removeFromTop (34));
    hintLabel.setBounds (bounds.removeFromBottom (64));
}

void DanceThingyAudioProcessorEditor::timerCallback()
{
    // The overlay may switch the language too; pick that up within ~2 seconds.
    if (++languagePollTicks >= 10)
    {
        languagePollTicks = 0;

        const auto disk = loadLanguage();

        if (disk != getLanguage())
        {
            setLanguage (disk);
            retranslate();
        }
    }

    refreshStatus();
}

} // namespace dt
