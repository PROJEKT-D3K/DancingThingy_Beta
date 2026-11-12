#include "OverlaySession.h"

#include "DancerWindow.h"
#include "SettingsWindow.h"
#include "TrayIcon.h"

#include <cmath>

namespace dt
{

namespace
{
    juce::Rectangle<int> primaryUserArea()
    {
        if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            return d->userArea;

        return { 0, 0, 1280, 720 };
    }
}

//==============================================================================
OverlaySession::OverlaySession (juce::PropertiesFile& props)
    : settings (props)
{
    // ---- language ---------------------------------------------------------
    setLanguage (loadLanguage());
    lastSeenLanguageTag = languageTag (getLanguage());

    // ---- global switches --------------------------------------------------
    sourceMode   = (BeatSource) juce::jlimit (0, 3, settings.getIntValue ("beatSource", 0));
    alwaysOnTop  = settings.getIntValue ("alwaysOnTop", 1) != 0;
    showGrabArea = settings.getIntValue ("showGrabArea", 0) != 0;
    // click-through is never restored: a saved "on" would lock every character
    // away from both drag and right-click.

    // ---- tunables ---------------------------------------------------------
    udpPort       = juce::jlimit (1024, 65535, settings.getIntValue ("udpPort", defaultPort));
    hostTimeoutMs = juce::jlimit (50, 5000, settings.getIntValue ("hostTimeoutMs", (int) dt::hostTimeoutMs));
    systemRetries = juce::jlimit (0, 20, settings.getIntValue ("systemRetries", 3));
    refreshHz     = juce::jlimit (15, 240, settings.getIntValue ("refreshHz", 60));

    characters.setExtraRootsFromString (settings.getValue ("extraRoots", ""));

    bindListener (udpPort, udpStatus);

    loopback = std::make_unique<LoopbackListener>();
}

OverlaySession::~OverlaySession()
{
    stopTimer();
    settingsWindow.reset();
    tray.reset();
    dancers.clear();
    loopback.reset();
    listener.reset();
}

void OverlaySession::start()
{
    restoreDancers();

    applySourceMode();
    startTimerHz (refreshHz);

    tray = std::make_unique<TrayIconComponent> (*this);

    juce::Logger::writeToLog ("[Session] started: dancers=" + juce::String (dancers.size())
                              + " hz=" + juce::String (refreshHz)
                              + " port=" + juce::String (udpPort)
                              + " language=" + juce::String (languageTag (getLanguage())));
}

void OverlaySession::shutdown()
{
    saveToProperties();

    stopTimer();
    settingsWindow.reset();
    tray.reset();

    if (loopback != nullptr)
        loopback->stop();
}

//==============================================================================
void OverlaySession::restoreDancers()
{
    auto list = deserialiseDancers (settings.getValue ("dancers", ""));

    if (list.isEmpty())
    {
        // v0.3.x kept exactly one character in flat keys; fold them into the
        // first entry so an existing install looks unchanged after the update.
        DancerState legacy;
        legacy.characterPath = settings.getValue ("characterPath", "");
        legacy.rowLock       = settings.getIntValue ("rowLock", -1);
        legacy.sizePercent   = settings.getIntValue ("sizePercent", 100);
        legacy.mirror        = settings.getIntValue ("mirror", 0) != 0;
        legacy.blend         = settings.getIntValue ("blend", 0) != 0;
        legacy.x             = settings.getIntValue ("windowX", -9999);
        legacy.y             = settings.getIntValue ("windowY", -9999);

        if (settings.containsKey ("speedMultiplier"))
        {
            legacy.speed = settings.getDoubleValue ("speedMultiplier", 1.0);
        }
        else
        {
            static const double ladder[] { 0.5, 1.0, 2.0 };
            legacy.speed = ladder[juce::jlimit (0, 2, settings.getIntValue ("speedIdx", 1))];
        }

        list.add (legacy);

        juce::Logger::writeToLog ("[Settings] no \"dancers\" list -> migrated the v0.3 keys into 1 dancer");
    }

    if (list.isEmpty())
        list.add (DancerState {});

    for (const auto& s : list)
    {
        auto* w = new DancerWindow (*this, s);
        dancers.add (w);
        w->show();
    }

    juce::Logger::writeToLog ("[Settings] restored " + juce::String (dancers.size()) + " dancer(s)");
}

void OverlaySession::saveToProperties()
{
    juce::Array<DancerState> list;
    for (auto* d : dancers)
        list.add (d->toState());

    settings.setValue ("dancers", serialiseDancers (list));
    settings.setValue ("extraRoots", characters.getExtraRootsAsString());
    settings.setValue ("beatSource", (int) sourceMode);
    settings.setValue ("alwaysOnTop", alwaysOnTop ? 1 : 0);
    settings.setValue ("showGrabArea", showGrabArea ? 1 : 0);
    settings.setValue ("udpPort", udpPort);
    settings.setValue ("hostTimeoutMs", hostTimeoutMs);
    settings.setValue ("systemRetries", systemRetries);
    settings.setValue ("refreshHz", refreshHz);
    settings.saveIfNeeded();
}

//==============================================================================
void OverlaySession::timerCallback()
{
    drainUdp();
    maybeRestartLoopback();

    // Poll the language file once a second: the VST editor may have switched it.
    if (++languagePollTicks >= juce::jmax (1, refreshHz))
    {
        languagePollTicks = 0;
        pollExternalLanguage();
    }

    const auto bs = computeBeatState();

    if (activeSource != lastLoggedSource)
    {
        lastLoggedSource = activeSource;
        juce::Logger::writeToLog ("[Beat] active source -> " + describeActiveSource());
    }

    for (auto* d : dancers)
        d->renderTick (bs);
}

void OverlaySession::drainUdp()
{
    if (listener == nullptr)
        return;

    char buffer[maxPacketBytes];

    for (;;)
    {
        juce::String sourceAddress;
        int sourcePort = 0;

        const int n = listener->read (buffer, maxPacketBytes - 1, false,
                                      sourceAddress, sourcePort);
        if (n <= 0)
            break;

        TransportSnapshot s;

        if (parsePacket (juce::String::fromUTF8 (buffer, n), s)
            && sourceAddress == "127.0.0.1")
        {
            hostSnapshot     = s;
            lastHostPacketMs = juce::Time::getMillisecondCounter();
            hostHasPacket    = true;
        }
    }
}

BeatState OverlaySession::computeBeatState()
{
    const auto now = juce::Time::getMillisecondCounter();
    const bool freshHost = hostHasPacket && (now - lastHostPacketMs) < (juce::uint32) hostTimeoutMs;

    auto fromSystem = [this]() -> BeatState
    {
        if (loopback != nullptr && loopback->isRunning())
        {
            auto bs = loopback->getBeatState();
            if (bs.playing)
                return bs;
        }
        return {};
    };

    switch (sourceMode)
    {
        case BeatSource::Off:
            activeSource = BeatSource::Off;
            return {};

        case BeatSource::Vst:
            // Explicit VST mode never falls back: a pinned source that silently
            // swaps would be worse than showing the character idle.
            if (freshHost)
            {
                activeSource = BeatSource::Vst;
                return deriveBeatState (hostSnapshot);
            }
            activeSource = BeatSource::Off;
            return {};

        case BeatSource::System:
        {
            // Ignore UDP entirely, otherwise a paused VST (which still streams
            // every 15 ms) would permanently shadow the system audio.
            auto bs = fromSystem();
            activeSource = bs.playing ? BeatSource::System : BeatSource::Off;
            return bs;
        }

        case BeatSource::Auto:
        default:
            break;
    }

    // Auto: the v0.3 authority order - fresh host transport, then system
    // loopback, then idle.
    if (freshHost)
    {
        activeSource = BeatSource::Vst;
        return deriveBeatState (hostSnapshot);
    }

    auto bs = fromSystem();
    if (bs.playing)
    {
        activeSource = BeatSource::System;
        return bs;
    }

    activeSource = BeatSource::Off;
    return {};
}

juce::String OverlaySession::describeActiveSource() const
{
    switch (activeSource)
    {
        case BeatSource::Vst:    return tr (Text::beatVst);
        case BeatSource::System: return tr (Text::beatSystem);
        case BeatSource::Auto:   return tr (Text::beatAuto);
        case BeatSource::Off:    break;
    }

    return tr (Text::beatIdle);
}

void OverlaySession::setSourceMode (BeatSource s)
{
    sourceMode = s;
    applySourceMode();
    saveToProperties();
}

void OverlaySession::applySourceMode()
{
    if (loopback == nullptr)
        return;

    const bool wantSystem = (sourceMode == BeatSource::Auto
                             || sourceMode == BeatSource::System);

    juce::Logger::writeToLog ("[Source] mode=" + juce::String ((int) sourceMode)
                              + " wantSystem=" + juce::String (wantSystem ? 1 : 0)
                              + " running=" + juce::String (loopback->isRunning() ? 1 : 0));

    if (wantSystem)
    {
        loopbackRetryCount = 0;

        if (! loopback->isRunning())
            loopback->start();

        lastLoopbackAttemptMs = juce::Time::getMillisecondCounter();
        lastLoopbackError = loopback->getLastError();
    }
    else if (loopback->isRunning())
    {
        loopback->stop();
    }
}

// The default output device can disappear while the overlay runs (headphones
// plugged in, driver reloaded). WASAPI then reports AUDCLNT_E_DEVICE_INVALIDATED
// and the capture stops; retry quietly instead of requiring a restart.
void OverlaySession::maybeRestartLoopback()
{
    if (loopback == nullptr)
        return;

    const bool wantSystem = (sourceMode == BeatSource::Auto
                             || sourceMode == BeatSource::System);

    if (! wantSystem || loopback->isRunning())
        return;

    // Give up after the configured attempts: a broken audio service must not
    // fill the log with device scans every three seconds. Re-selecting the
    // source resets the counter (see applySourceMode).
    if (loopbackRetryCount >= systemRetries)
        return;

    const auto now = juce::Time::getMillisecondCounter();
    if (now - lastLoopbackAttemptMs < 3000)
        return;

    lastLoopbackAttemptMs = now;
    ++loopbackRetryCount;
    loopback->start();

    const auto err = loopback->getLastError();
    if (err.isNotEmpty() && err != lastLoopbackError)
    {
        lastLoopbackError = err;
        juce::Logger::writeToLog ("[Loopback] " + err);
    }
}

//==============================================================================
bool OverlaySession::bindListener (int port, juce::String& errorOut)
{
    listener.reset();

    auto socket = std::make_unique<juce::DatagramSocket>();

    if (socket->bindToPort (port))
    {
        listener = std::move (socket);
        errorOut = trf (Text::settingsUdpListening, { juce::String (port) });
        juce::Logger::writeToLog ("[UDP] listening on 127.0.0.1:" + juce::String (port));
        return true;
    }

    errorOut = trf (Text::settingsUdpBindFailed, { juce::String (port) });
    juce::Logger::writeToLog ("[UDP] could not bind port " + juce::String (port)
                              + " (another overlay instance running?)");
    return false;
}

bool OverlaySession::setUdpPort (int port, juce::String& errorOut)
{
    if (port < 1024 || port > 65535)
    {
        errorOut = trf (Text::settingsUdpBindFailed, { juce::String (port) });
        return false;
    }

    if (port == udpPort && listener != nullptr)
        return true;

    const int previous = udpPort;

    if (! bindListener (port, errorOut))
    {
        // Fall back to the port that was working and tell the caller why.
        udpPort = previous;
        juce::String ignored;
        bindListener (previous, ignored);
        return false;
    }

    udpPort = port;
    udpStatus = errorOut;
    saveToProperties();
    return true;
}

juce::String OverlaySession::getUdpStatusText() const
{
    return udpStatus;
}

void OverlaySession::setHostTimeoutMs (int ms)
{
    hostTimeoutMs = juce::jlimit (50, 5000, ms);
    saveToProperties();
}

void OverlaySession::setSystemRetries (int n)
{
    systemRetries = juce::jlimit (0, 20, n);
    loopbackRetryCount = 0;
    saveToProperties();
}

void OverlaySession::setRefreshHz (int hz)
{
    refreshHz = juce::jlimit (15, 240, hz);
    startTimerHz (refreshHz);
    saveToProperties();
}

//==============================================================================
void OverlaySession::setClickThrough (bool v)
{
    clickThrough = v;
    applyGlobalStyle();
}

void OverlaySession::setAlwaysOnTop (bool v)
{
    alwaysOnTop = v;
    applyGlobalStyle();
    saveToProperties();
}

void OverlaySession::setShowGrabArea (bool v)
{
    showGrabArea = v;
    invalidateAllFrames();
    saveToProperties();
}

void OverlaySession::applyGlobalStyle()
{
    for (auto* d : dancers)
        d->notifyGlobalStyleChanged();
}

void OverlaySession::invalidateAllFrames()
{
    for (auto* d : dancers)
        d->invalidateFrame();
}

//==============================================================================
void OverlaySession::setLanguageAndApply (Language l)
{
    setLanguage (l);
    storeLanguage (l);
    lastSeenLanguageTag = languageTag (l);
    applyLanguageUi();

    juce::Logger::writeToLog ("[L10n] language -> " + juce::String (languageTag (l)));
}

void OverlaySession::pollExternalLanguage()
{
    const auto tag = juce::String (languageTag (loadLanguage()));

    if (tag == lastSeenLanguageTag)
        return;

    setLanguage (loadLanguage());
    lastSeenLanguageTag = tag;
    applyLanguageUi();

    juce::Logger::writeToLog ("[L10n] language picked up from disk -> " + tag);
}

void OverlaySession::applyLanguageUi()
{
    if (SettingsWindow* w = settingsWindow.get())
        w->retranslateAll();

    notifyDancerListChanged();
}

//==============================================================================
void OverlaySession::addDancer (const juce::String& characterPath, juce::Point<int> screenPos)
{
    DancerState s;
    s.characterPath = characterPath;
    s.x = screenPos.x;
    s.y = screenPos.y;

    auto* w = new DancerWindow (*this, s);
    const bool placed = screenPos.x > -4000 && screenPos.y > -4000;

    dancers.add (w);
    w->show();

    if (! placed)
        w->snapToCorner (dancers.size() - 1);

    saveToProperties();
    notifyDancerListChanged();

    juce::Logger::writeToLog ("[Character] added dancer -> " + juce::String (dancers.size()) + " total");
}

void OverlaySession::addDancerAuto (const juce::String& characterPath)
{
    addDancer (characterPath, { -9999, -9999 });
}

void OverlaySession::addDancerFromSheet (const juce::File& sheetFile, juce::Point<int> screenPos)
{
    addDancer (sheetFile.getFullPathName(), screenPos);
}

void OverlaySession::addDancerFromSheetAuto (const juce::File& sheetFile)
{
    addDancerAuto (sheetFile.getFullPathName());
}

void OverlaySession::removeDancer (DancerWindow* w)
{
    if (w == nullptr || dancers.size() <= 1)
        return;

    if (! dancers.contains (w))
        return;

    // Shared rather than unique: callAsync takes a std::function, which has to
    // be copyable. The deferred reset below still runs exactly once.
    auto owned = std::shared_ptr<DancerWindow> (w);
    dancers.removeObject (w, false);   // detach without deleting here

    juce::Logger::writeToLog ("[Character] removed one dancer -> "
                              + juce::String (dancers.size()) + " left");

    saveToProperties();
    notifyDancerListChanged();

    // Deferred: this runs from a popup-menu callback, which may be invoked
    // while the window is still inside its own mouse handler.
    juce::MessageManager::callAsync ([owned]() mutable
                                     {
                                         owned.reset();
                                     });
}

int OverlaySession::getNumDancers() const
{
    return dancers.size();
}

DancerWindow* OverlaySession::getDancer (int index) const
{
    if (index < 0 || index >= dancers.size())
        return nullptr;

    return dancers.getUnchecked (index);
}

void OverlaySession::snapAllToCorner()
{
    for (int i = 0; i < dancers.size(); ++i)
        dancers.getUnchecked (i)->snapToCorner (i);
}

juce::String OverlaySession::makeDancerLabel (int index) const
{
    if (auto* w = getDancer (index))
        return juce::String (index + 1) + ". " + w->getDisplayName();

    return {};
}

void OverlaySession::notifyDancerListChanged()
{
    if (tray != nullptr)
        tray->refreshTooltip();

    if (SettingsWindow* w = settingsWindow.get())
        w->refreshDancers();
}

//==============================================================================
void OverlaySession::addCharacterRoot (const juce::File& dir)
{
    if (! dir.isDirectory())
    {
        juce::Logger::writeToLog ("[Character] import skipped, not a folder: "
                                  + dir.getFullPathName());
        return;
    }

    auto roots = characters.getExtraRoots();
    roots.addIfNotAlreadyThere (dir.getFullPathName());
    characters.setExtraRoots (roots);   // rescans

    saveToProperties();
    notifyDancerListChanged();

    juce::Logger::writeToLog ("[Character] added root folder: " + dir.getFullPathName()
                              + " -> now " + juce::String (characters.getNumEntries())
                              + " sheet(s) accessible");
}

void OverlaySession::promptForCharacterFolder()
{
    chooser = std::make_unique<juce::FileChooser> (tr (Text::chooserFolderTitle),
                                                   characters.getCharactersDir());

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fc)
                          {
                              addCharacterRoot (fc.getResult());
                          });
}

void OverlaySession::promptForCharacterFile()
{
    chooser = std::make_unique<juce::FileChooser> (tr (Text::chooserSheetTitle),
                                                   characters.getCharactersDir(),
                                                   "*.png;*.gif;*.jpg;*.jpeg");

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (file.existsAsFile())
                              {
                                  juce::Logger::writeToLog ("[Character] sheet chosen file: "
                                                            + file.getFullPathName());
                                  addCharacterRoot (file.getParentDirectory());
                              }
                          });
}

//==============================================================================
void OverlaySession::buildTrayMenu (juce::PopupMenu& menu)
{
    buildSharedMenuTail (menu);
}

void OverlaySession::buildSharedMenuTail (juce::PopupMenu& menu)
{
    {
        juce::PopupMenu chars;

        if (dancers.isEmpty())
            chars.addItem (-1, tr (Text::settingsNoCharacters), false, false);

        for (int i = 0; i < dancers.size() && (trayDancerFirst + i) <= trayDancerLast; ++i)
            chars.addItem (trayDancerFirst + i, makeDancerLabel (i), true, false);

        chars.addSeparator();
        chars.addItem (-1, tr (Text::trayAddCharacter), false, false);
        chars.addItem (trayAddProcedural, tr (Text::charProcedural));

        const int n = characters.getNumEntries();
        if (n > 0)
        {
            juce::PopupMenu sheets;

            for (int i = 0; i < n && (trayAddSheetFirst + i) <= trayAddSheetLast; ++i)
                sheets.addItem (trayAddSheetFirst + i, characters.getEntry (i).displayName,
                                characters.entryHasTxt (i));

            chars.addSubMenu (tr (Text::menuFromSheets), sheets);
        }

        menu.addSubMenu (tr (Text::trayCharacters), chars);
    }

    menu.addItem (traySettings, tr (Text::menuSettings));
    menu.addSeparator();
    menu.addItem (trayClickThrough, tr (Text::menuClickThrough), true, clickThrough);
    menu.addItem (trayAlwaysOnTop, tr (Text::menuAlwaysOnTop), true, alwaysOnTop);
    menu.addItem (traySnap, tr (Text::menuSnapCorner));
    menu.addSeparator();
    menu.addItem (trayQuit, tr (Text::menuQuitOverlay));
}

void OverlaySession::handleTrayMenuResult (int id)
{
    switch (id)
    {
        case traySettings:     openSettingsWindow(); break;
        case trayQuit:         quitApplication(); break;
        case trayClickThrough: setClickThrough (! clickThrough); break;
        case trayAlwaysOnTop:  setAlwaysOnTop (! alwaysOnTop); break;
        case traySnap:         snapAllToCorner(); break;
        case trayAddProcedural: addDancerAuto ({}); break;

        default:
            if (id >= trayDancerFirst && id <= trayDancerLast)
            {
                if (auto* w = getDancer (id - trayDancerFirst))
                {
                    w->toFront (true);
                    w->applyNativeStyle();
                }
            }
            else if (id >= trayAddSheetFirst && id <= trayAddSheetLast)
            {
                const int i = id - trayAddSheetFirst;
                if (i < characters.getNumEntries())
                    addDancerFromSheetAuto (characters.getEntry (i).imageFile);
            }
            break;
    }
}

void OverlaySession::showTrayMenu (juce::Point<int> screenPos)
{
    juce::PopupMenu menu;
    buildTrayMenu (menu);

    juce::Component::SafePointer<TrayIconComponent> safeTray (tray.get());

    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 }),
                        [safeTray, this] (int r)
                        {
                            if (safeTray != nullptr)
                                handleTrayMenuResult (r);
                        });
}

void OverlaySession::openSettingsWindow()
{
    if (settingsWindow == nullptr)
        settingsWindow = std::make_unique<SettingsWindow> (*this);

    settingsWindow->refreshAll();
    settingsWindow->setVisible (true);
    settingsWindow->toFront (true);
}

void OverlaySession::quitApplication()
{
    if (auto* app = juce::JUCEApplication::getInstance())
        app->systemRequestedQuit();
}

} // namespace dt
