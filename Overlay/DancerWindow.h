#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "BeatClock.h"
#include "CharacterManager.h"
#include "CharacterRenderer.h"
#include "DancerState.h"

namespace dt
{

class OverlaySession;

// ---------------------------------------------------------------------------
// One desktop character: a transparent, always-on-top layered window that owns
// its own artwork, drag/resize state and right-click menu.
//
// Everything shared between characters (UDP listener, WASAPI self-listen, beat
// arbitration, tray icon, language, settings window) lives on OverlaySession;
// this class asks the session for it.
//
// Hit testing note: Windows decides whether a layered window receives the mouse
// by looking at the pixel's alpha, not the window rectangle. So the window is
// sized to the artwork's bounding box and every pixel in it gets an alpha of at
// least 1 (invisible, but not "transparent" to the hit test) - which is what
// makes "drag/right-click anywhere on the character" reliable. See
// OverlaySession's notes for the trade-off.
// ---------------------------------------------------------------------------
class DancerWindow : public juce::Component,
                     public juce::FileDragAndDropTarget
{
public:
    DancerWindow (OverlaySession& session, const DancerState& initialState);
    ~DancerWindow() override;

    void show();                 // adds to the desktop and places itself
    void renderTick (const BeatState& bs);

    DancerState toState() const;              // current settings + position
    void applyState (const DancerState& s);   // used by the restore path

    const juce::String& getActivePath() const noexcept { return activePath; }
    juce::String getDisplayName() const;      // sheet name, or the built-in label

    void applyNativeStyle();                  // re-applies click-through / topmost
    void snapToCorner (int cascadeIndex);
    juce::Point<int> getDefaultPlacement() const;
    void notifyGlobalStyleChanged();
    void invalidateFrame() { frameDirty = true; }

    // --- Component ---------------------------------------------------------
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent& e) override;
    void userTriedToCloseWindow() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void buildMenu (juce::PopupMenu& menu);
    void handleMenuResult (int id);
    void showMenuAt (juce::Point<int> screenPos);

    // A WS_EX_NOACTIVATION window is never focused, so a popup menu opened from
    // it has no JUCE component with focus and JUCE's menu dismisses itself
    // immediately (PopupMenu's checkButtonState). Allow activation just for the
    // duration of the menu, then take it back.
    void setActivatableForMenu (bool activatable);

    void selectCharacterPath (const juce::String& path);
    bool loadFromPath (const juce::String& path);   // false = not scannable this run
    void selectProcedural();
    void selectSpriteEntry (int entryIndex);
    void applyCharacterSize();
    void refreshSpriteSettings();
    void pushLayeredImage (const juce::Image& img);
    juce::Point<int> getNewDancerPlacement() const;   // cascade next to this one

    OverlaySession& session;

    // --- per-character state ----------------------------------------------
    DancerState state;
    juce::String activePath;              // empty = procedural
    ProceduralCharacter procChar;
    std::unique_ptr<SpriteCharacter> spriteChar;

    bool mouseHeld = false;

    // A right-click opens its menu on the *release*: showing a modal menu while
    // the button is still down means the button-up lands on this window (it
    // holds the mouse capture), which JUCE treats as an input attempt during
    // modality and the menu closes instantly.
    bool contextMenuPending = false;
    juce::Point<int> contextMenuAt {};

    // Shift+drag resize
    bool resizing = false;
    int  dragStartPercent = 100;
    juce::Rectangle<int> dragStartBounds {};
    double displayAspect = 240.0 / 360.0;

    juce::Point<int> dragGrab {};
    juce::Point<int> dragWindowOrigin {};

    // Reused across frames: the layered update needs a fresh bitmap anyway, but
    // there is no reason to reallocate a few hundred KB every 16 ms.
    juce::Image frameImage;

    // "nothing changed" guard for the freeze case (see renderTick)
    BeatState lastBeat {};
    bool lastMouseHeld = false;
    bool frameDirty = true;

    // ---- menu ids (local to this window) ---------------------------------
    static constexpr int menuClickThrough = 1, menuAlwaysOnTop = 2;
    static constexpr int menuSnap = 3, menuQuit = 4, menuSettings = 5;
    static constexpr int menuRemove = 6, menuAddProcedural = 7;
    static constexpr int menuImportFolder = 8, menuAddFile = 9;
    static constexpr int menuPatternAuto = 10, menuPatternFirst = 11, menuPatternLast = 13;
    static constexpr int menuRowAuto = 20, menuRowFirst = 200, menuRowLast = 279;
    static constexpr int menuMirror = 33, menuBlend = 34;
    static constexpr int menuSpeedPresetFirst = 50, menuSpeedPresetLast = 58;
    static constexpr int menuSpeedMinus = 59, menuSpeedPlus = 60;
    static constexpr int menuSizeFirst = 70, menuSizeLast = 76;
    static constexpr int menuOpenFolder = 78, menuRefresh = 79;
    static constexpr int menuProcedural = 90;
    static constexpr int menuCharFirst = 100, menuCharLast = 199;
    static constexpr int menuAddSheetFirst = 600, menuAddSheetLast = 699;

    JUCE_DECLARE_NON_COPYABLE (DancerWindow)
};

} // namespace dt
