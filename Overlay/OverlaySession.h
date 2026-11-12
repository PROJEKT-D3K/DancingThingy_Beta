#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "AppConfig.h"
#include "BeatClock.h"
#include "BeatProtocol.h"
#include "CharacterManager.h"
#include "DancerState.h"
#include "L10n.h"
#include "LoopbackListener.h"

namespace dt
{

class DancerWindow;
class SettingsWindow;
class TrayIconComponent;

// ---------------------------------------------------------------------------
// Everything the characters share, plus the code that used to live in the
// single-window overlay of v0.3.x:
//
//   - the UDP listener for the companion VST and the WASAPI self-listen
//   - beat arbitration (Auto = VST while fresh, then system audio, then idle)
//   - one timer that renders every DancerWindow at the configured refresh rate
//   - the tray icon, the settings window and the UI language
//
// DancerWindow owns the per-character parts (artwork, drag/resize, its own
// right-click menu).
// ---------------------------------------------------------------------------
class OverlaySession : private juce::Timer
{
public:
    // Which beat source the user pinned down. Auto keeps the v0.3 authority
    // order (VST while fresh, then system loopback, then idle); the others
    // force one source and never fall back, so "system" stays usable even
    // while a DanceThingy VST instance streams paused transports.
    enum class BeatSource { Auto = 0, Vst = 1, System = 2, Off = 3 };

    explicit OverlaySession (juce::PropertiesFile& settings);
    ~OverlaySession() override;

    void start();          // restores the saved characters and starts rendering
    void shutdown();       // saves everything, stops the timer and the capture
    void saveToProperties();

    juce::PropertiesFile&         getSettings()   noexcept { return settings; }
    CharacterManager&             getCharacters() noexcept { return characters; }
    const CharacterManager&       getCharacters() const    { return characters; }

    // --- beat source -------------------------------------------------------
    BeatSource getSourceMode() const noexcept { return sourceMode; }
    void       setSourceMode (BeatSource s);
    juce::String describeActiveSource() const;   // localized

    // --- global switches ---------------------------------------------------
    bool isClickThrough() const noexcept { return clickThrough; }
    void setClickThrough (bool v);
    bool isAlwaysOnTop() const noexcept  { return alwaysOnTop; }
    void setAlwaysOnTop (bool v);
    bool isShowGrabArea() const noexcept { return showGrabArea; }
    void setShowGrabArea (bool v);

    // --- tunables (settings window) ---------------------------------------
    int  getUdpPort() const noexcept       { return udpPort; }
    bool setUdpPort (int port, juce::String& errorOut);   // false -> still on the old port
    juce::String getUdpStatusText() const;
    int  getHostTimeoutMs() const noexcept { return hostTimeoutMs; }
    void setHostTimeoutMs (int ms);
    int  getSystemRetries() const noexcept { return systemRetries; }
    void setSystemRetries (int n);
    int  getRefreshHz() const noexcept     { return refreshHz; }
    void setRefreshHz (int hz);

    // --- language ----------------------------------------------------------
    void setLanguageAndApply (Language l);           // persists + retranslates
    void pollExternalLanguage();                     // picks up a VST-side change

    // --- dancers -----------------------------------------------------------
    void addDancer (const juce::String& characterPath, juce::Point<int> screenPos);
    void addDancerAuto (const juce::String& characterPath);   // cascaded corner slot
    void addDancerFromSheet (const juce::File& sheetFile, juce::Point<int> screenPos);
    void addDancerFromSheetAuto (const juce::File& sheetFile);
    void removeDancer (DancerWindow* w);
    int  getNumDancers() const;
    DancerWindow* getDancer (int index) const;
    void snapAllToCorner();
    juce::String makeDancerLabel (int index) const;
    void addCharacterRoot (const juce::File& dir);

    // --- menus / windows ---------------------------------------------------
    void buildTrayMenu (juce::PopupMenu& menu);
    // The part both menus share, in the same order: character list, settings,
    // click-through / always-on-top / snap, quit. The character menu appends
    // its own per-character entries and then calls this, so the tray menu is
    // literally the tail of the character menu.
    void buildSharedMenuTail (juce::PopupMenu& menu);
    void handleTrayMenuResult (int id);
    void showTrayMenu (juce::Point<int> screenPos);
    void openSettingsWindow();
    void promptForCharacterFolder();
    void promptForCharacterFile();
    void quitApplication();

private:
    void timerCallback() override;

    void drainUdp();
    BeatState computeBeatState();
    void applySourceMode();
    void maybeRestartLoopback();
    bool bindListener (int port, juce::String& errorOut);

    void restoreDancers();
    void applyGlobalStyle();
    void notifyDancerListChanged();
    void invalidateAllFrames();
    void applyLanguageUi();

    juce::PropertiesFile& settings;
    CharacterManager characters;

    juce::OwnedArray<DancerWindow> dancers;
    std::unique_ptr<TrayIconComponent> tray;
    std::unique_ptr<SettingsWindow>    settingsWindow;

    std::unique_ptr<juce::DatagramSocket> listener;
    std::unique_ptr<LoopbackListener>     loopback;

    TransportSnapshot hostSnapshot {};
    juce::uint32 lastHostPacketMs = 0;
    bool hostHasPacket = false;

    BeatSource sourceMode       = BeatSource::Auto;
    BeatSource activeSource     = BeatSource::Off;   // what computeBeatState() last used
    BeatSource lastLoggedSource = BeatSource::Off;
    juce::uint32 lastLoopbackAttemptMs = 0;
    int          loopbackRetryCount    = 0;
    juce::String lastLoopbackError;

    bool clickThrough = false;
    bool alwaysOnTop  = true;
    bool showGrabArea = false;

    int udpPort       = defaultPort;
    int hostTimeoutMs = (int) dt::hostTimeoutMs;
    int systemRetries = 3;
    int refreshHz     = 60;

    juce::String lastSeenLanguageTag;
    int languagePollTicks = 0;
    juce::String udpStatus;                 // last bind result, shown in settings

    std::unique_ptr<juce::FileChooser> chooser;

    // tray menu ids
    static constexpr int traySettings = 900, trayQuit = 901;
    static constexpr int trayClickThrough = 902, trayAlwaysOnTop = 903;
    static constexpr int trayAddProcedural = 904, traySnap = 905;
    static constexpr int trayDancerFirst = 910, trayDancerLast = 969;
    static constexpr int trayAddSheetFirst = 970, trayAddSheetLast = 999;

    JUCE_DECLARE_NON_COPYABLE (OverlaySession)
};

} // namespace dt
