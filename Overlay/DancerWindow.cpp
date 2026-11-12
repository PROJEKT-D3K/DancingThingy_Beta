#ifdef _WIN32
 // Include windows.h first, before JUCE headers, with a WINVER new enough
 // to expose WS_EX_NOACTIVATION and friends.
 #ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0601
 #endif
 #ifndef WINVER
  #define WINVER 0x0601
 #endif
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #ifndef WS_EX_NOACTIVATION
  #define WS_EX_NOACTIVATION 0x08000000L
 #endif
#endif

#include "DancerWindow.h"

#include "L10n.h"
#include "OverlaySession.h"

#include <cmath>
#include <cstring>

namespace dt
{

namespace
{
    constexpr double speedPresets[] { 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0 };
    constexpr const char* speedLabels[] { "×0.25", "×0.5", "×0.75", "×1",
                                          "×1.25", "×1.5", "×2", "×3", "×4" };
    constexpr int sizePresets[] { 50, 75, 100, 125, 150, 200, 300 };

    juce::String formatSpeed (double v)
    {
        juce::String s (v, 2);
        while (s.endsWithChar ('0') && s.containsChar ('.'))
            s = s.dropLastCharacters (1);
        if (s.endsWithChar ('.'))
            s = s.dropLastCharacters (1);
        return u8 ("×") + s;
    }

    juce::Rectangle<int> primaryUserArea()
    {
        if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            return d->userArea;
        return { 0, 0, 1280, 720 };
    }
}

//==============================================================================
DancerWindow::DancerWindow (OverlaySession& s, const DancerState& initialState)
    : session (s), state (initialState)
{
    setOpaque (false);

    // The context menu needs a focused JUCE component: JUCE's menu window
    // dismisses itself when nothing in the app has focus and the cursor is not
    // over it. A tool window still has to accept focus for that to happen.
    setWantsKeyboardFocus (true);

    state.speed       = juce::jlimit (0.25, 4.0, state.speed);
    state.sizePercent = juce::jlimit (25, 400, state.sizePercent);

    if (state.characterPath.isNotEmpty())
        loadFromPath (state.characterPath);

    refreshSpriteSettings();
    applyCharacterSize();
}

DancerWindow::~DancerWindow() = default;

void DancerWindow::applyState (const DancerState& s)
{
    state = s;
    state.speed       = juce::jlimit (0.25, 4.0, state.speed);
    state.sizePercent = juce::jlimit (25, 400, state.sizePercent);

    spriteChar.reset();
    activePath.clear();

    if (state.characterPath.isNotEmpty() && ! loadFromPath (state.characterPath))
        // Keep the saved path: if the folder shows up again on a later run the
        // character comes back. Only this run falls back to the built-in figure.
        juce::Logger::writeToLog ("[Character] saved sheet not scannable this run: "
                                  + state.characterPath + " -> falling back to procedural");

    refreshSpriteSettings();
    applyCharacterSize();
    frameDirty = true;
}

DancerState DancerWindow::toState() const
{
    auto s = state;
    s.x = getX();
    s.y = getY();
    return s;
}

juce::String DancerWindow::getDisplayName() const
{
    return spriteChar != nullptr ? spriteChar->getDisplayName() : tr (Text::charProcedural);
}

void DancerWindow::show()
{
    const auto area = primaryUserArea();

    const bool placed = state.x > -4000 && state.y > -4000
                        && area.contains (state.x, state.y)
                        && area.contains (state.x + getWidth(), state.y + getHeight());

    if (placed)
        setTopLeftPosition (state.x, state.y);
    else
        setTopLeftPosition (getDefaultPlacement());

    state.x = getX();
    state.y = getY();

    addToDesktop (0);
    setVisible (true);
    applyNativeStyle();
}

juce::Point<int> DancerWindow::getDefaultPlacement() const
{
    const auto area = primaryUserArea();
    return { area.getRight() - getWidth() - 40, area.getBottom() - getHeight() - 40 };
}

void DancerWindow::snapToCorner (int cascadeIndex)
{
    const auto area = primaryUserArea();

    auto p = getDefaultPlacement();
    p.x -= cascadeIndex * 24;
    p.y -= cascadeIndex * 24;

    p.x = juce::jmax (area.getX(), p.x);
    p.y = juce::jmax (area.getY(), p.y);

    setTopLeftPosition (p);
    state.x = getX();
    state.y = getY();
}

juce::Point<int> DancerWindow::getNewDancerPlacement() const
{
    const auto area = primaryUserArea();

    auto p = juce::Point<int> { getX() + 40, getY() + 40 };
    p.x = juce::jlimit (area.getX(), juce::jmax (area.getX(), area.getRight() - getWidth() - 40), p.x);
    p.y = juce::jlimit (area.getY(), juce::jmax (area.getY(), area.getBottom() - getHeight() - 40), p.y);
    return p;
}

void DancerWindow::notifyGlobalStyleChanged()
{
    // Turning click-through off has to rebuild the image too: the frames pushed
    // while it was on skip the alpha floor, so the empty pixels would keep
    // letting the mouse through until the next render.
    frameDirty = true;
    applyNativeStyle();
}

//==============================================================================
#ifdef _WIN32

void DancerWindow::applyNativeStyle()
{
    auto* peer = getPeer();
    if (peer == nullptr)
        return;

    const auto hwnd = (HWND) peer->getNativeHandle();

    LONG_PTR style = GetWindowLongPtrW (hwnd, GWL_STYLE);
    style &= ~(LONG_PTR) (WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    style |= WS_POPUP;
    SetWindowLongPtrW (hwnd, GWL_STYLE, style);

    LONG_PTR ex = GetWindowLongPtrW (hwnd, GWL_EXSTYLE);
    ex |= WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATION;

    if (session.isAlwaysOnTop()) ex |= WS_EX_TOPMOST;     else ex &= ~(LONG_PTR) WS_EX_TOPMOST;
    if (session.isClickThrough()) ex |= WS_EX_TRANSPARENT; else ex &= ~(LONG_PTR) WS_EX_TRANSPARENT;

    SetWindowLongPtrW (hwnd, GWL_EXSTYLE, ex);

    SetWindowPos (hwnd, session.isAlwaysOnTop() ? HWND_TOPMOST : HWND_NOTOPMOST,
                  0, 0, 0, 0,
                  SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_NOACTIVATE);
}

// Per-pixel-alpha "layered window" update from a premultiplied JUCE ARGB image.
void DancerWindow::pushLayeredImage (const juce::Image& img)
{
    auto* peer = getPeer();
    if (peer == nullptr)
        return;

    const auto hwnd = (HWND) peer->getNativeHandle();

    const int w = img.getWidth();
    const int h = img.getHeight();
    if (w <= 0 || h <= 0)
        return;

    const juce::Image::BitmapData bmp (img, juce::Image::BitmapData::readOnly);

    HDC screenDC = GetDC (nullptr);
    if (screenDC == nullptr)
        return;

    BITMAPINFO bmi {};
    bmi.bmiHeader.biSize        = sizeof (BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h;   // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void*  bits = nullptr;
    HBITMAP dib = CreateDIBSection (screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);

    if (dib != nullptr && bits != nullptr)
    {
        // JUCE ARGB pixels are premultiplied 0xAARRGGBB, which in little-endian
        // memory lays out exactly as a 32bpp top-down DIB (B, G, R, A).
        for (int y = 0; y < h; ++y)
            memcpy ((uint8_t*) bits + (size_t) y * (size_t) w * 4u,
                    bmp.getLinePointer (y), (size_t) w * 4u);

        // Windows hit-tests a layered window per pixel: alpha 0 = "the mouse
        // belongs to whatever is underneath". The window is already cropped to
        // the artwork's bounding box, and lifting empty pixels to alpha 1 makes
        // that whole box grabbable while staying invisible. Skipped while
        // click-through is on, because there the window must never take input.
        if (! session.isClickThrough())
        {
            auto* px = static_cast<uint8_t*> (bits);
            const size_t n = (size_t) w * (size_t) h;

            for (size_t i = 0; i < n; ++i)
            {
                auto* a = px + i * 4u + 3u;
                if (*a == 0)
                    *a = 1;
            }
        }

        if (HDC memDC = CreateCompatibleDC (screenDC))
        {
            const HGDIOBJ oldBmp = SelectObject (memDC, dib);

            RECT wr {};
            if (! GetWindowRect (hwnd, &wr))
                wr = RECT { 0, 0, w, h };

            POINT dst  { wr.left, wr.top };
            SIZE  size { w, h };
            POINT src  { 0, 0 };
            BLENDFUNCTION blend { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

            UpdateLayeredWindow (hwnd, screenDC, &dst, &size, memDC, &src,
                                 0, &blend, ULW_ALPHA);

            SelectObject (memDC, oldBmp);
            DeleteDC (memDC);
        }

        DeleteObject (dib);
    }

    ReleaseDC (nullptr, screenDC);
}

#else

void DancerWindow::applyNativeStyle() {}

void DancerWindow::pushLayeredImage (const juce::Image&) {}

#endif

//==============================================================================
void DancerWindow::renderTick (const BeatState& bs)
{
    auto* peer = getPeer();
    if (peer == nullptr)
        return;

    // A sheet without a Waiting row freezes the last frame while the transport
    // is stopped. Nothing but the beat state can change that, so skip the whole
    // layered upload (two or three of those per frame add up).
    const bool frozen = spriteChar != nullptr && ! bs.playing && ! mouseHeld
                        && spriteChar->getWaitingRow() < 0;

    const bool sameBeat = lastBeat.playing == bs.playing
                          && lastBeat.bpm == bs.bpm
                          && lastBeat.barIndex == bs.barIndex
                          && lastBeat.beatInBar == bs.beatInBar
                          && lastBeat.quarterIndex == bs.quarterIndex;

    if (frozen && ! frameDirty && lastMouseHeld == mouseHeld && sameBeat)
        return;

    const auto area = peer->getBounds();   // native window size in pixels
    if (area.isEmpty())
        return;

    const int pw = area.getWidth();
    const int ph = area.getHeight();

    if (frameImage.getWidth() != pw || frameImage.getHeight() != ph)
        frameImage = juce::Image (juce::Image::ARGB, pw, ph, true);
    else
        frameImage.clear (frameImage.getBounds(), juce::Colours::transparentBlack);

    {
        juce::Graphics g (frameImage);

        const float scale = (float) pw / (float) getWidth();
        if (scale != 1.0f)
            g.addTransform (juce::AffineTransform::scale (scale));

        CharacterRenderer* renderer = (spriteChar != nullptr)
                                          ? static_cast<CharacterRenderer*> (spriteChar.get())
                                          : static_cast<CharacterRenderer*> (&procChar);

        renderer->render (g, getWidth(), getHeight(), bs,
                          juce::Time::getMillisecondCounterHiRes(), mouseHeld);

        // "Where can I grab it?" made visible. Drawn on top of the character so
        // the outline is never hidden by opaque artwork.
        if (session.isShowGrabArea())
        {
            g.setColour (juce::Colours::white.withAlpha (0.6f));
            g.drawRect (getLocalBounds(), 1);
        }
    }

    pushLayeredImage (frameImage);

    lastBeat      = bs;
    lastMouseHeld = mouseHeld;
    frameDirty    = false;
}

//==============================================================================
void DancerWindow::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
    {
        contextMenuPending = true;
        contextMenuAt = { e.getScreenX(), e.getScreenY() };
        return;
    }

    // Shift+drag resizes instead of moving. The window already hugs the
    // artwork, so any pixel of it is a valid starting point.
    if (e.mods.isShiftDown())
    {
        resizing         = true;
        dragGrab         = e.getScreenPosition().toInt();
        dragStartBounds  = getBounds();
        dragStartPercent = state.sizePercent;
        mouseHeld        = true;
        frameDirty       = true;
        return;
    }

    dragGrab         = e.getScreenPosition().toInt();
    dragWindowOrigin = getPosition();
    mouseHeld        = true;
    frameDirty       = true;
}

void DancerWindow::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
        return;

    if (resizing)
    {
        const auto d = e.getScreenPosition().toInt() - dragGrab;
        const double delta = ((double) d.x + (double) d.y) * 0.5;

        const int startW = juce::jmax (16, dragStartBounds.getWidth());
        const int newW   = juce::jmax (16, juce::roundToInt ((double) startW + delta));
        const int pct    = juce::jlimit (25, 400,
                                         juce::roundToInt ((double) dragStartPercent
                                                           * (double) newW / (double) startW));

        if (pct != state.sizePercent)
        {
            state.sizePercent = pct;
            applyCharacterSize();   // keeps the aspect ratio and screen limits
        }
        return;
    }

    setTopLeftPosition (dragWindowOrigin
                        + (e.getScreenPosition().toInt() - dragGrab));

    state.x = getX();
    state.y = getY();
}

void DancerWindow::mouseUp (const juce::MouseEvent& e)
{
    mouseHeld  = false;
    frameDirty = true;

    if (contextMenuPending)
    {
        contextMenuPending = false;

        // Released over the character: open the menu. Dragged away and released
        // elsewhere: treat it as a cancelled gesture.
        if (contains (e.getPosition()))
        {
            const auto at = contextMenuAt;
            juce::Component::SafePointer<DancerWindow> safeThis (this);

            juce::MessageManager::callAsync ([safeThis, at]
                                             {
                                                 if (safeThis != nullptr)
                                                     safeThis->showMenuAt (at);
                                             });
        }
        return;
    }

    if (resizing)
    {
        resizing = false;
        session.saveToProperties();   // resize does not go through the menu path
    }
}

void DancerWindow::userTriedToCloseWindow()
{
    session.quitApplication();
}

//==============================================================================
bool DancerWindow::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        const auto ext = juce::File (f).getFileExtension().toLowerCase();
        if (ext == ".png" || ext == ".gif" || ext == ".jpg" || ext == ".jpeg")
            return true;
    }
    return false;
}

void DancerWindow::filesDropped (const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused (x, y);

    for (const auto& f : files)
    {
        const auto file = juce::File (f);
        const auto ext  = file.getFileExtension().toLowerCase();

        if (ext == ".png" || ext == ".gif" || ext == ".jpg" || ext == ".jpeg")
        {
            juce::Logger::writeToLog ("[Character] sprite sheet dropped: " + f);
            session.addCharacterRoot (file.getParentDirectory());
            break;
        }
    }
}

//==============================================================================
void DancerWindow::setActivatableForMenu (bool activatable)
{
   #ifdef _WIN32
    auto* peer = getPeer();
    if (peer == nullptr)
        return;

    const auto hwnd = (HWND) peer->getNativeHandle();

    auto ex = GetWindowLongPtrW (hwnd, GWL_EXSTYLE);
    ex = activatable ? (ex & ~(LONG_PTR) WS_EX_NOACTIVATION)
                     : (ex | WS_EX_NOACTIVATION);
    SetWindowLongPtrW (hwnd, GWL_EXSTYLE, ex);

    if (activatable)
    {
        // The user just clicked this window, so we are allowed to take the
        // foreground; without it the menu window cannot get focus either.
        SetForegroundWindow (hwnd);
        SetFocus (hwnd);
    }
   #else
    juce::ignoreUnused (activatable);
   #endif
}

void DancerWindow::showMenuAt (juce::Point<int> screenPos)
{
    juce::PopupMenu menu;
    buildMenu (menu);

    setActivatableForMenu (true);

    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 }),
                        [safeThis = juce::Component::SafePointer<DancerWindow> (this)] (int r)
                        {
                            // One line per menu interaction: enough to tell "the
                            // menu never opened" from "it opened and was dismissed".
                            juce::Logger::writeToLog ("[Input] menu result " + juce::String (r));

                            if (safeThis != nullptr)
                            {
                                safeThis->setActivatableForMenu (false);
                                safeThis->handleMenuResult (r);
                            }
                        });
}

void DancerWindow::buildMenu (juce::PopupMenu& menu)
{
    const auto& list = session.getCharacters();

    // ---- which artwork ----------------------------------------------------
    {
        juce::PopupMenu chars;
        chars.addItem (menuProcedural, tr (Text::charProcedural), true, spriteChar == nullptr);

        const int n = list.getNumEntries();
        if (n > 0)
            chars.addSeparator();

        for (int i = 0; i < n && (menuCharFirst + i) <= menuCharLast; ++i)
        {
            const auto& e = list.getEntry (i);
            const bool has = list.entryHasTxt (i);

            chars.addItem (menuCharFirst + i,
                           has ? e.displayName
                               : e.displayName + u8 ("  ") + tr (Text::charMissingTxt),
                           has,
                           activePath.isNotEmpty() && e.imageFile == juce::File (activePath));
        }

        chars.addSeparator();
        chars.addItem (menuImportFolder, tr (Text::charImportFolder));
        chars.addItem (menuAddFile,      tr (Text::charAddSheet));
        chars.addSeparator();
        chars.addItem (menuOpenFolder, tr (Text::charOpenFolder));
        chars.addItem (menuRefresh,    tr (Text::charRefresh));

        menu.addSubMenu (tr (Text::menuCharacter), chars);
    }

    // ---- dance rows / animation ------------------------------------------
    {
        juce::PopupMenu dances;

        if (spriteChar == nullptr)
        {
            dances.addItem (menuPatternAuto,      tr (Text::danceAutoRotate), true, state.pattern < 0);
            dances.addItem (menuPatternFirst + 0, tr (Text::danceSway),       true, state.pattern == 0);
            dances.addItem (menuPatternFirst + 1, tr (Text::danceWave),       true, state.pattern == 1);
            dances.addItem (menuPatternFirst + 2, tr (Text::danceStep),       true, state.pattern == 2);
        }
        else
        {
            dances.addItem (menuRowAuto, tr (Text::danceAutoRotate), true, state.rowLock < 0);

            const int danceRows = spriteChar->getDanceRowCount();
            for (int i = 0; i < danceRows && (menuRowFirst + i) <= menuRowLast; ++i)
            {
                const auto rowName = spriteChar->getDanceRowName (i).trim();
                dances.addItem (menuRowFirst + i,
                                rowName.isEmpty()
                                    ? trf (Text::danceUnnamedRow, { juce::String (i + 1) })
                                    : rowName,
                                true, state.rowLock == i);
            }

            dances.addSeparator();

            juce::PopupMenu anim;
            anim.addItem (-1, trf (Text::danceCurrent, { formatSpeed (state.speed) }), false, false);
            anim.addSeparator();

            for (int i = 0; i < 9; ++i)
                anim.addItem (menuSpeedPresetFirst + i, u8 (speedLabels[i]), true,
                              std::abs (state.speed - speedPresets[i]) < 1.0e-6);

            anim.addSeparator();
            anim.addItem (menuSpeedMinus, u8 ("− 0.05"), state.speed > 0.25, false);
            anim.addItem (menuSpeedPlus,  u8 ("+ 0.05"), state.speed < 4.0,  false);
            anim.addSeparator();
            anim.addItem (menuMirror, tr (Text::animMirror), true, state.mirror);
            anim.addItem (menuBlend,  tr (Text::animBlend),  true, state.blend);

            dances.addSubMenu (tr (Text::menuAnimation), anim);
        }

        menu.addSubMenu (tr (Text::menuDance), dances);
    }

    // ---- size -------------------------------------------------------------
    {
        juce::PopupMenu sizes;
        sizes.addItem (-1, trf (Text::sizeCurrent, { juce::String (state.sizePercent) }), false, false);
        sizes.addSeparator();

        for (int i = 0; i < 7; ++i)
            sizes.addItem (menuSizeFirst + i, juce::String (sizePresets[i]) + "%", true,
                           state.sizePercent == sizePresets[i]);

        sizes.addSeparator();
        sizes.addItem (-1, tr (Text::sizeShiftHint), false, false);

        menu.addSubMenu (tr (Text::menuSize), sizes);
    }

    menu.addSeparator();

    // ---- add / remove -----------------------------------------------------
    {
        juce::PopupMenu add;
        add.addItem (menuAddProcedural, tr (Text::charProcedural));

        const int n = list.getNumEntries();
        if (n > 0)
        {
            juce::PopupMenu sheets;

            for (int i = 0; i < n && (menuAddSheetFirst + i) <= menuAddSheetLast; ++i)
                sheets.addItem (menuAddSheetFirst + i, list.getEntry (i).displayName,
                                list.entryHasTxt (i));

            add.addSubMenu (tr (Text::menuFromSheets), sheets);
        }

        menu.addSubMenu (tr (Text::menuAddCharacter), add);
    }

    // The last character cannot be removed: with no window there is nothing
    // left to click, and the tray icon would be the only way back.
    menu.addItem (menuRemove, tr (Text::menuRemoveCharacter), session.getNumDancers() > 1);
    menu.addItem (menuSettings, tr (Text::menuSettings));

    menu.addSeparator();
    menu.addItem (menuClickThrough, tr (Text::menuClickThrough), true, session.isClickThrough());
    menu.addItem (menuAlwaysOnTop,  tr (Text::menuAlwaysOnTop),  true, session.isAlwaysOnTop());
    menu.addItem (menuSnap, tr (Text::menuSnapCorner));

    menu.addSeparator();
    menu.addItem (menuQuit, tr (Text::menuQuitOverlay));
}

void DancerWindow::handleMenuResult (int id)
{
    auto& list = session.getCharacters();

    switch (id)
    {
        case menuClickThrough: session.setClickThrough (! session.isClickThrough()); break;
        case menuAlwaysOnTop:  session.setAlwaysOnTop (! session.isAlwaysOnTop());   break;
        case menuSnap:         session.snapAllToCorner(); break;
        case menuSettings:     session.openSettingsWindow(); break;
        case menuQuit:         session.quitApplication(); break;

        case menuRemove:
            session.removeDancer (this);
            return;   // this window is about to go away

        case menuProcedural:    selectCharacterPath ({}); break;
        case menuAddProcedural: session.addDancer ({}, getNewDancerPlacement()); break;

        case menuImportFolder: session.promptForCharacterFolder(); break;
        case menuAddFile:      session.promptForCharacterFile(); break;
        case menuOpenFolder:   list.getCharactersDir().startAsProcess(); break;
        case menuRefresh:      list.rescan(); break;

        case menuPatternAuto: state.pattern = -1; break;
        case menuRowAuto:     state.rowLock = -1; break;

        case menuSpeedMinus: state.speed = juce::jlimit (0.25, 4.0, state.speed - 0.05); break;
        case menuSpeedPlus:  state.speed = juce::jlimit (0.25, 4.0, state.speed + 0.05); break;
        case menuMirror:     state.mirror = ! state.mirror; break;
        case menuBlend:      state.blend  = ! state.blend;  break;

        default:
            if (id >= menuCharFirst && id <= menuCharLast)
            {
                const int i = id - menuCharFirst;
                if (i < list.getNumEntries())
                    selectCharacterPath (list.getEntry (i).imageFile.getFullPathName());
            }
            else if (id >= menuAddSheetFirst && id <= menuAddSheetLast)
            {
                const int i = id - menuAddSheetFirst;
                if (i < list.getNumEntries())
                    session.addDancerFromSheet (list.getEntry (i).imageFile, getNewDancerPlacement());
            }
            else if (id >= menuRowFirst && id <= menuRowLast)
            {
                state.rowLock = id - menuRowFirst;
            }
            else if (id >= menuSpeedPresetFirst && id <= menuSpeedPresetLast)
            {
                state.speed = speedPresets[id - menuSpeedPresetFirst];
            }
            else if (id >= menuSizeFirst && id <= menuSizeLast)
            {
                state.sizePercent = sizePresets[id - menuSizeFirst];
                applyCharacterSize();
            }
            else if (id >= menuPatternFirst && id <= menuPatternLast)
            {
                state.pattern = id - menuPatternFirst;
            }
            break;
    }

    refreshSpriteSettings();
    frameDirty = true;
    session.saveToProperties();
}

//==============================================================================
bool DancerWindow::loadFromPath (const juce::String& path)
{
    auto& list = session.getCharacters();
    const juce::File file (path);

    auto findEntry = [&list, &file]() -> int
    {
        for (int i = 0; i < list.getNumEntries(); ++i)
            if (list.getEntry (i).imageFile == file)
                return i;

        return -1;
    };

    int idx = findEntry();

    if (idx < 0)
    {
        list.rescan();   // the folder may have been added after the last scan
        idx = findEntry();
    }

    if (idx < 0)
        return false;

    auto sheet = list.loadSheet (idx);

    if (! sheet.ok)
    {
        juce::Logger::writeToLog ("[Character] failed to load '"
                                  + list.getEntry (idx).displayName + "': " + sheet.error);
        return false;
    }

    activePath = sheet.sourceFile.getFullPathName();
    spriteChar = std::make_unique<SpriteCharacter> (std::move (sheet));

    refreshSpriteSettings();
    applyCharacterSize();
    frameDirty = true;
    return true;
}

void DancerWindow::selectCharacterPath (const juce::String& path)
{
    if (path.isEmpty())
    {
        state.characterPath.clear();
        selectProcedural();
        return;
    }

    if (loadFromPath (path))
    {
        state.characterPath = activePath;
        return;
    }

    juce::Logger::writeToLog ("[Character] sheet not scannable: " + path
                              + " -> falling back to procedural");
    state.characterPath.clear();
    selectProcedural();
}

void DancerWindow::selectProcedural()
{
    spriteChar.reset();
    activePath.clear();
    refreshSpriteSettings();
    applyCharacterSize();
    frameDirty = true;
}

void DancerWindow::selectSpriteEntry (int entryIndex)
{
    const auto& list = session.getCharacters();
    if (entryIndex < 0 || entryIndex >= list.getNumEntries())
        return;

    selectCharacterPath (list.getEntry (entryIndex).imageFile.getFullPathName());
}

void DancerWindow::applyCharacterSize()
{
    const double user = juce::jlimit (0.25, 4.0, state.sizePercent / 100.0);

    int nw = 0, nh = 0;
    int cw = 0, ch = 0;

    if (spriteChar != nullptr && spriteChar->getContentSize (cw, ch))
    {
        displayAspect = (double) cw / (double) ch;

        const double targetH = 320.0, capW = 340.0;
        double scale = juce::jmin (targetH / (double) ch, capW / (double) cw);
        scale = juce::jlimit (1.0, 4.0, scale);

        nw = juce::jmax (16, juce::roundToInt ((double) cw * scale * user));
        nh = juce::jmax (16, juce::roundToInt ((double) ch * scale * user));
    }
    else
    {
        displayAspect = 240.0 / 360.0;
        nw = juce::jmax (16, juce::roundToInt (240.0 * user));
        nh = juce::jmax (16, juce::roundToInt (360.0 * user));
    }

    // Clamp as one scale factor, not width and height separately, otherwise a
    // large size on a small screen would squash the character.
    const auto area = primaryUserArea();
    const double maxW = juce::jmax (16.0, (double) area.getWidth()  - 20.0);
    const double maxH = juce::jmax (16.0, (double) area.getHeight() - 40.0);

    double fit = 1.0;
    if ((double) nw > maxW) fit = juce::jmin (fit, maxW / (double) nw);
    if ((double) nh > maxH) fit = juce::jmin (fit, maxH / (double) nh);

    nw = juce::jmax (16, juce::roundToInt ((double) nw * fit));
    nh = juce::jmax (16, juce::roundToInt ((double) nh * fit));

    setSize (nw, nh);
    frameDirty = true;

    if (isOnDesktop() && ! area.contains (juce::Rectangle<int> { getX(), getY(), nw, nh }))
        snapToCorner (0);
}

void DancerWindow::refreshSpriteSettings()
{
    procChar.setPatternLock (state.pattern);

    if (spriteChar == nullptr)
        return;

    spriteChar->setSpeed   (state.speed);
    spriteChar->setMirror  (state.mirror);
    spriteChar->setBlend   (state.blend);
    spriteChar->setRowLock (state.rowLock);
}

} // namespace dt
