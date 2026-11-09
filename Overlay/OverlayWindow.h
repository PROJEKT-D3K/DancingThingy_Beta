#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_data_structures/juce_data_structures.h>

#include "BeatClock.h"
#include "BeatProtocol.h"
#include "CharacterManager.h"
#include "CharacterRenderer.h"
#include "LoopbackListener.h"

namespace dt
{
class TrayIconComponent;

// Transparent, always-on-top, optionally click-through desktop dancer.
// The beat source is selectable (see BeatSource): the companion VST over UDP,
// WASAPI loopback self-listen (onset detection), either by automatic
// fallback, or nothing at all.
// Artwork is either the built-in procedural vestal or Fruity Dance-style
// sprite sheets managed through CharacterManager (see its format notes).
class OverlayWindow : public juce::Component,
                      private juce::Timer,
                      public  juce::FileDragAndDropTarget
{
public:
    OverlayWindow (juce::PropertiesFile& settings);
    ~OverlayWindow() override;

    void show();
    void saveToProperties();   // writes overview state into the settings file

    // drag to move (shows the sheet's "Held" row while dragging);
    // right-click opens the menu
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent& e) override;

    // WM_CLOSE / "end task" without /F: quit cleanly so settings are written.
    void userTriedToCloseWindow() override;

    // drop a Fruity Dance sprite sheet onto the character registers its
    // folder as a new character root (same behaviour as FL's drag-drop)
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    void buildTrayMenu (juce::PopupMenu& menu);
    void handleMenuResult (int resultId);

private:
    // Which beat source the user pinned down. Auto keeps the old authority
    // order (VST while fresh, then system loopback, then idle); the others
    // force one source and never fall back, so "system" stays usable even
    // while a DanceThingy VST instance is streaming paused transports.
    enum class BeatSource { Auto = 0, Vst = 1, System = 2, Off = 3 };

    void timerCallback() override;

    void drainUdp();
    BeatState computeBeatState();
    void applySourceMode();
    void maybeRestartLoopback();
    void renderFrame (const BeatState& bs);
    void applyNativeStyle();
    void pushLayeredImage (const juce::Image& img);

    void selectProceduralCharacter();
    void selectSpriteCharacter (int entryIndex);
    void applyCharacterSize();
    void snapToCorner();
    void refreshSpriteSettings();
    bool isSpriteActive (const CharacterManager::Entry& e) const;
    void addRootFromChoice (const juce::File& dir);
    void handleDirectoryResult (const juce::File& dir);
    void handleFileResult (const juce::File& file);
    void promptForDirectory();
    void promptForFile();

    juce::PropertiesFile& settings;
    CharacterManager characters;
    ProceduralCharacter procChar;
    std::unique_ptr<SpriteCharacter> spriteChar;

    std::unique_ptr<TrayIconComponent>    tray;
    std::unique_ptr<juce::DatagramSocket> listener;
    std::unique_ptr<LoopbackListener>     loopback;

    TransportSnapshot hostSnapshot {};
    juce::uint32 lastHostPacketMs = 0;
    bool hostHasPacket = false;

    BeatSource sourceMode   = BeatSource::Auto;
    BeatSource activeSource = BeatSource::Off;   // what computeBeatState() last used
    BeatSource lastLoggedSource = BeatSource::Off;
    juce::uint32 lastLoopbackAttemptMs = 0;
    int          loopbackRetryCount = 0;
    juce::String lastLoopbackError;

    bool clickThrough = false;
    bool alwaysOnTop  = true;
    int  patternLock  = -1;   // procedural dance pattern
    int  rowLock      = -1;   // sprite dance row
    double speedMultiplier = 1.0;   // 0.25 .. 4.0 (sprite sheets)
    int  sizePercent  = 100;        // 25 .. 400, on top of the fit-to-cell size
    bool mirror       = false;
    bool blend        = false;
    bool mouseHeld    = false;

    // Shift+drag resize state
    bool resizing = false;
    int  dragStartPercent = 100;
    juce::Rectangle<int> dragStartBounds {};
    double displayAspect = 240.0 / 360.0;

    juce::String activePath;  // full path of the active sprite sheet; empty = procedural

    juce::Point<int> dragGrab {};
    juce::Point<int> dragWindowOrigin {};

    std::unique_ptr<juce::FileChooser> chooser;

    // menu ids
    static constexpr int proceduralId = 90;
    static constexpr int charFirst = 100, charLast = 199;
    static constexpr int rowAuto = 20, rowFirst = 200, rowLast = 279;
    static constexpr int beatSourceAuto = 40, beatSourceVst = 41;
    static constexpr int beatSourceSystem = 42, beatSourceOff = 43;
    static constexpr int speedPresetFirst = 50, speedPresetLast = 58;
    static constexpr int speedMinus = 59, speedPlus = 60;
    static constexpr int sizeFirst = 70, sizeLast = 76;
    static constexpr int mirrorId = 33, blendId = 34;
    static constexpr int importDirId = 5, addFileId = 8;
    static constexpr int openDirId = 6, refreshId = 7;

    JUCE_DECLARE_NON_COPYABLE (OverlayWindow)
};

} // namespace dt
