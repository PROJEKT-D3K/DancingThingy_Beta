#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "OverlaySession.h"

namespace dt
{

// ---------------------------------------------------------------------------
// The settings panel: language, beat source, UDP interface, retry policy,
// refresh rate, the grab-area overlay and the character list.
//
// Every control writes through OverlaySession, which persists immediately, so
// there is no OK/Cancel - changes are live.
// ---------------------------------------------------------------------------
class SettingsPanel : public juce::Component,
                      private juce::ListBoxModel
{
public:
    explicit SettingsPanel (OverlaySession& session);
    ~SettingsPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void retranslate();
    void refreshDancers();
    void refreshAll();

    // juce::ListBoxModel
    int  getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics& g, int width, int height,
                           bool rowIsSelected) override;

private:
    void buildCharacterMenu (juce::PopupMenu& menu);
    void handleCharacterMenuResult (int id);

    OverlaySession& session;

    juce::Label languageLabel, beatLabel, portLabel, portStatusLabel,
                timeoutLabel, retriesLabel, hzLabel;
    juce::ComboBox languageBox, beatBox, hzBox;
    juce::Slider portSlider, timeoutSlider, retriesSlider;
    juce::ToggleButton grabToggle;
    juce::ListBox dancerList;
    juce::TextButton addButton, removeButton, closeButton;

    juce::Label sectionLabel;

    JUCE_DECLARE_NON_COPYABLE (SettingsPanel)
};

//==============================================================================
class SettingsWindow : public juce::DocumentWindow
{
public:
    explicit SettingsWindow (OverlaySession& session);
    ~SettingsWindow() override;

    void closeButtonPressed() override;

    void retranslateAll();
    void refreshDancers();
    void refreshAll();

private:
    SettingsPanel* panel = nullptr;

    JUCE_DECLARE_NON_COPYABLE (SettingsWindow)
};

} // namespace dt
