#include "SettingsWindow.h"

#include "DancerWindow.h"
#include "L10n.h"

namespace dt
{

namespace
{
    juce::Label* styleLabel (juce::Label& l, juce::Justification just = juce::Justification::centredLeft)
    {
        l.setJustificationType (just);
        l.setFont (juce::Font (juce::FontOptions (14.0f)));
        l.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        return &l;
    }
}

//==============================================================================
SettingsPanel::SettingsPanel (OverlaySession& s)
    : session (s)
{
    addAndMakeVisible (*styleLabel (languageLabel));
    addAndMakeVisible (*styleLabel (beatLabel));
    addAndMakeVisible (*styleLabel (portLabel));
    addAndMakeVisible (*styleLabel (timeoutLabel));
    addAndMakeVisible (*styleLabel (retriesLabel));
    addAndMakeVisible (*styleLabel (hzLabel));

    addAndMakeVisible (*styleLabel (portStatusLabel));
    portStatusLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    portStatusLabel.setColour (juce::Label::textColourId, juce::Colours::grey);

    sectionLabel.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    sectionLabel.setColour (juce::Label::textColourId, juce::Colour (0xffa9cce8));
    addAndMakeVisible (sectionLabel);

    // ---- language ---------------------------------------------------------
    languageBox.addItem ("中文", 1);
    languageBox.addItem ("English", 2);
    languageBox.addItem (u8 ("日本語"), 3);
    languageBox.onChange = [this]
    {
        const auto lang = (Language) juce::jlimit (0, 2, languageBox.getSelectedId() - 1);
        if (lang != getLanguage())
            session.setLanguageAndApply (lang);
    };
    addAndMakeVisible (languageBox);

    // ---- beat source ------------------------------------------------------
    // Items need their real text here: ComboBox refuses empty ones, and
    // changeItemText() cannot rescue an item that was never added.
    beatBox.addItem (tr (Text::beatAutoVst),    1);
    beatBox.addItem (tr (Text::beatVstOnly),    2);
    beatBox.addItem (tr (Text::beatSystemOnly), 3);
    beatBox.addItem (tr (Text::beatOff),        4);
    beatBox.onChange = [this]
    {
        const int id = beatBox.getSelectedId();
        if (id >= 1 && id <= 4)
            session.setSourceMode ((OverlaySession::BeatSource) (id - 1));
    };
    addAndMakeVisible (beatBox);

    // ---- UDP port ---------------------------------------------------------
    portSlider.setSliderStyle (juce::Slider::IncDecButtons);
    portSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 78, 22);
    portSlider.setRange (1024.0, 65535.0, 1.0);
    portSlider.onValueChange = [this]
    {
        const int want = juce::roundToInt (portSlider.getValue());
        if (want == session.getUdpPort())
            return;

        juce::String error;
        const bool ok = session.setUdpPort (want, error);

        if (! ok)
            portSlider.setValue (session.getUdpPort(), juce::dontSendNotification);

        portStatusLabel.setColour (juce::Label::textColourId,
                                   ok ? juce::Colours::lightgreen : juce::Colours::orange);
        portStatusLabel.setText (session.getUdpStatusText(), juce::dontSendNotification);
    };
    addAndMakeVisible (portSlider);

    // ---- VST heartbeat timeout -------------------------------------------
    timeoutSlider.setSliderStyle (juce::Slider::IncDecButtons);
    timeoutSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 78, 22);
    timeoutSlider.setRange (100.0, 2000.0, 25.0);
    timeoutSlider.onValueChange = [this]
    {
        session.setHostTimeoutMs (juce::roundToInt (timeoutSlider.getValue()));
    };
    addAndMakeVisible (timeoutSlider);

    // ---- system-audio retries --------------------------------------------
    retriesSlider.setSliderStyle (juce::Slider::IncDecButtons);
    retriesSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 78, 22);
    retriesSlider.setRange (0.0, 10.0, 1.0);
    retriesSlider.onValueChange = [this]
    {
        session.setSystemRetries (juce::roundToInt (retriesSlider.getValue()));
    };
    addAndMakeVisible (retriesSlider);

    // ---- refresh rate -----------------------------------------------------
    hzBox.addItem ("30 Hz", 30);
    hzBox.addItem ("60 Hz", 60);
    hzBox.addItem ("120 Hz", 120);
    hzBox.onChange = [this]
    {
        const int hz = hzBox.getSelectedId();
        if (hz > 0)
            session.setRefreshHz (hz);
    };
    addAndMakeVisible (hzBox);

    // ---- grab area --------------------------------------------------------
    grabToggle.onClick = [this]
    {
        session.setShowGrabArea (grabToggle.getToggleState());
    };
    addAndMakeVisible (grabToggle);

    // ---- character list ---------------------------------------------------
    dancerList.setModel (this);
    dancerList.setRowHeight (24);
    dancerList.setColour (juce::ListBox::backgroundColourId, juce::Colours::black.withAlpha (0.25f));
    addAndMakeVisible (dancerList);

    addButton.onClick = [this]
    {
        juce::PopupMenu menu;
        buildCharacterMenu (menu);

        const juce::Component::SafePointer<SettingsPanel> safeThis (this);

        menu.showMenuAsync (juce::PopupMenu::Options()
                                .withTargetComponent (&addButton),
                            [safeThis] (int r)
                            {
                                if (safeThis != nullptr)
                                    safeThis->handleCharacterMenuResult (r);
                            });
    };
    addAndMakeVisible (addButton);

    removeButton.onClick = [this]
    {
        const int row = dancerList.getSelectedRow();

        if (row >= 0 && session.getNumDancers() > 1)
            session.removeDancer (session.getDancer (row));

        refreshDancers();
    };
    addAndMakeVisible (removeButton);

    closeButton.onClick = [this]
    {
        if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
            window->setVisible (false);
    };
    addAndMakeVisible (closeButton);

    retranslate();
    refreshAll();
}

SettingsPanel::~SettingsPanel()
{
    // The ListBox calls back into this model while painting.
    dancerList.setModel (nullptr);
}

void SettingsPanel::retranslate()
{
    languageLabel.setText (tr (Text::settingsLanguage), juce::dontSendNotification);
    beatLabel.setText (tr (Text::menuBeatSource), juce::dontSendNotification);
    portLabel.setText (tr (Text::settingsUdpPort), juce::dontSendNotification);
    timeoutLabel.setText (tr (Text::settingsVstTimeout), juce::dontSendNotification);
    retriesLabel.setText (tr (Text::settingsSysRetries), juce::dontSendNotification);
    hzLabel.setText (tr (Text::settingsRefreshRate), juce::dontSendNotification);
    sectionLabel.setText (tr (Text::settingsCharacters), juce::dontSendNotification);

    grabToggle.setButtonText (tr (Text::settingsShowGrabArea));
    addButton.setButtonText (tr (Text::settingsAddCharacter));
    removeButton.setButtonText (tr (Text::remove));
    closeButton.setButtonText (tr (Text::close));

    languageBox.changeItemText (1, u8 ("中文"));
    languageBox.changeItemText (2, "English");
    languageBox.changeItemText (3, u8 ("日本語"));

    beatBox.changeItemText (1, tr (Text::beatAutoVst));
    beatBox.changeItemText (2, tr (Text::beatVstOnly));
    beatBox.changeItemText (3, tr (Text::beatSystemOnly));
    beatBox.changeItemText (4, tr (Text::beatOff));

    portStatusLabel.setText (session.getUdpStatusText(), juce::dontSendNotification);

    repaint();
}

void SettingsPanel::refreshAll()
{
    languageBox.setSelectedId ((int) getLanguage() + 1, juce::dontSendNotification);
    beatBox.setSelectedId ((int) session.getSourceMode() + 1, juce::dontSendNotification);
    portSlider.setValue (session.getUdpPort(), juce::dontSendNotification);
    portSlider.setTooltip (tr (Text::settingsUdpHint));
    timeoutSlider.setValue (session.getHostTimeoutMs(), juce::dontSendNotification);
    retriesSlider.setValue (session.getSystemRetries(), juce::dontSendNotification);
    hzBox.setSelectedId (session.getRefreshHz(), juce::dontSendNotification);
    grabToggle.setToggleState (session.isShowGrabArea(), juce::dontSendNotification);
    portStatusLabel.setText (session.getUdpStatusText(), juce::dontSendNotification);

    refreshDancers();
}

void SettingsPanel::refreshDancers()
{
    dancerList.updateContent();
    dancerList.repaint();
    removeButton.setEnabled (dancerList.getSelectedRow() >= 0 && session.getNumDancers() > 1);
}

//==============================================================================
void SettingsPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff22252d));
}

void SettingsPanel::resized()
{
    auto r = getLocalBounds().reduced (16);

    // Wide enough for the longest label ("システム音声の再試行回数").
    constexpr int labelWidth = 210;

    auto row = [&r] (int height)
    {
        auto x = r.removeFromTop (height);
        r.removeFromTop (4);
        return x;
    };

    // Takes the line by reference: removeFromLeft has to advance the caller's
    // rectangle, otherwise the next control is laid out on top of the label.
    auto labelIn = [] (juce::Rectangle<int>& line, juce::Label& label, int width)
    {
        label.setBounds (line.removeFromLeft (width));
    };

    {
        auto line = row (28);
        labelIn (line, languageLabel, labelWidth);
        languageBox.setBounds (line.removeFromLeft (170));
    }

    {
        auto line = row (28);
        labelIn (line, beatLabel, labelWidth);
        beatBox.setBounds (line.removeFromLeft (260));
    }

    {
        auto line = row (28);
        labelIn (line, portLabel, labelWidth);
        portSlider.setBounds (line.removeFromLeft (170));
    }

    {
        auto line = row (20);
        line.removeFromLeft (labelWidth);
        portStatusLabel.setBounds (line);
    }

    {
        auto line = row (28);
        labelIn (line, timeoutLabel, labelWidth);
        timeoutSlider.setBounds (line.removeFromLeft (170));
    }

    {
        auto line = row (28);
        labelIn (line, retriesLabel, labelWidth);
        retriesSlider.setBounds (line.removeFromLeft (170));
    }

    {
        auto line = row (28);
        labelIn (line, hzLabel, labelWidth);
        hzBox.setBounds (line.removeFromLeft (120));
    }

    {
        auto line = row (28);
        grabToggle.setBounds (line.removeFromLeft (240));
    }

    r.removeFromTop (6);
    sectionLabel.setBounds (row (24));

    {
        auto buttons = r.removeFromBottom (30);
        closeButton.setBounds (buttons.removeFromRight (100));
        buttons.removeFromRight (8);
        removeButton.setBounds (buttons.removeFromRight (100));
        buttons.removeFromRight (8);
        addButton.setBounds (buttons.removeFromLeft (150));

        r.removeFromBottom (8);
        dancerList.setBounds (r);
    }
}

//==============================================================================
int SettingsPanel::getNumRows()
{
    return session.getNumDancers();
}

void SettingsPanel::paintListBoxItem (int row, juce::Graphics& g, int width, int height,
                                      bool rowIsSelected)
{
    if (rowIsSelected)
        g.fillAll (juce::Colour (0xff3a4252));

    g.setColour (juce::Colours::lightgrey);
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawText (session.makeDancerLabel (row), 8, 0, width - 12, height,
                juce::Justification::centredLeft, true);
}

//==============================================================================
void SettingsPanel::buildCharacterMenu (juce::PopupMenu& menu)
{
    menu.addItem (1, tr (Text::charProcedural));

    const auto& list = session.getCharacters();
    const int n = list.getNumEntries();

    if (n > 0)
    {
        juce::PopupMenu sheets;

        for (int i = 0; i < n; ++i)
            sheets.addItem (100 + i, list.getEntry (i).displayName, list.entryHasTxt (i));

        menu.addSubMenu (tr (Text::menuFromSheets), sheets);
    }
}

void SettingsPanel::handleCharacterMenuResult (int id)
{
    if (id == 1)
    {
        session.addDancerAuto ({});
    }
    else if (id >= 100)
    {
        const int i = id - 100;
        const auto& list = session.getCharacters();

        if (i < list.getNumEntries())
            session.addDancerFromSheetAuto (list.getEntry (i).imageFile);
    }

    refreshDancers();
}

//==============================================================================
SettingsWindow::SettingsWindow (OverlaySession& session)
    : DocumentWindow (tr (Text::settingsTitle),
                      juce::Colour (0xff22252d),
                      DocumentWindow::closeButton)
{
    setUsingNativeTitleBar (true);
    setResizable (false, false);

    panel = new SettingsPanel (session);
    setContentOwned (panel, true);

    centreWithSize (600, 540);
    setVisible (true);
}

SettingsWindow::~SettingsWindow()
{
    panel = nullptr;
}

void SettingsWindow::closeButtonPressed()
{
    setVisible (false);
}

void SettingsWindow::retranslateAll()
{
    setName (tr (Text::settingsTitle));

    if (panel != nullptr)
        panel->retranslate();
}

void SettingsWindow::refreshDancers()
{
    if (panel != nullptr)
        panel->refreshDancers();
}

void SettingsWindow::refreshAll()
{
    if (panel != nullptr)
    {
        panel->retranslate();
        panel->refreshAll();
    }
}

} // namespace dt
