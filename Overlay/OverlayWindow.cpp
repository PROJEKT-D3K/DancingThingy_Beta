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

#include "OverlayWindow.h"

#include "L10n.h"
#include "TrayIcon.h"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace dt
{

OverlayWindow::OverlayWindow (juce::PropertiesFile& props)
    : settings (props)
{
    setSize (240, 360);
    setOpaque (false);

    listener = std::make_unique<juce::DatagramSocket>();
    if (listener != nullptr && ! listener->bindToPort (defaultPort))
        juce::Logger::writeToLog ("[UDP] could not bind port " + juce::String (defaultPort)
                                  + " (another overlay instance already running?)");

    loopback = std::make_unique<LoopbackListener>();

    // ---- restore character settings -------------------------------------
    alwaysOnTop = props.getIntValue ("alwaysOnTop", 1) != 0;

    // Size must be known before the character is restored: show() validates
    // the saved position against getWidth()/getHeight().
    sizePercent = juce::jlimit (25, 400, props.getIntValue ("sizePercent", 100));

    sourceMode = (BeatSource) juce::jlimit (0, 3, props.getIntValue ("beatSource", 0));

    characters.setExtraRootsFromString (props.getValue ("extraRoots", ""));

    const juce::String savedPath = props.getValue ("characterPath", "");
    if (savedPath.isNotEmpty())
    {
        // normalized comparison: stored settings may use forward slashes
        const juce::File savedFile (savedPath);
        bool restored = false;
        for (int i = 0; i < characters.getNumEntries(); ++i)
        {
            if (characters.getEntry (i).imageFile == savedFile)
            {
                selectSpriteCharacter (i);
                restored = (spriteChar != nullptr);
                break;
            }
        }
        if (! restored)
            juce::Logger::writeToLog ("[Character] saved sheet not scannable this run: "
                                      + savedPath + " -> falling back to procedural");
    }

    rowLock  = props.getIntValue ("rowLock", -1);

    // speedMultiplier supersedes the old 0/1/2 speedIdx ladder
    if (props.containsKey ("speedMultiplier"))
    {
        speedMultiplier = juce::jlimit (0.25, 4.0,
                                        props.getDoubleValue ("speedMultiplier", 1.0));
    }
    else
    {
        static const double legacy[] { 0.5, 1.0, 2.0 };
        speedMultiplier = legacy[juce::jlimit (0, 2, props.getIntValue ("speedIdx", 1))];
    }

    mirror   = props.getIntValue ("mirror", 0) != 0;
    blend    = props.getIntValue ("blend", 0) != 0;
    refreshSpriteSettings();

    applyCharacterSize();   // the procedural branch is not covered by the restore path
    applySourceMode();

    juce::Logger::writeToLog ("[Settings] size=" + juce::String (sizePercent) + "%"
                              + " speed=" + juce::String (speedMultiplier, 2) + "x"
                              + " source=" + juce::String ((int) sourceMode)
                              + " char=" + juce::String (spriteChar != nullptr ? "sprite" : "procedural")
                              + " window=" + juce::String (getWidth()) + "x" + juce::String (getHeight()));

    tray = std::make_unique<TrayIconComponent> (*this);
}

OverlayWindow::~OverlayWindow()
{
    stopTimer();
    tray.reset();
    loopback.reset();
    listener.reset();
}

void OverlayWindow::show()
{
    const int sx = settings.getIntValue ("windowX", -9999);
    const int sy = settings.getIntValue ("windowY", -9999);

    bool placed = false;
    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto area = display->userArea;
        placed = (sx > -4000) && area.contains (sx, sy)
                 && area.contains (sx + getWidth(), sy + getHeight());
        if (placed)
            setTopLeftPosition (sx, sy);
    }
    if (! placed)
        snapToCorner();

    addToDesktop (0);
    setVisible (true);
    applyNativeStyle();
    startTimerHz (60);
}

//==============================================================================
#ifdef _WIN32

void OverlayWindow::applyNativeStyle()
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
    if (alwaysOnTop)   ex |= WS_EX_TOPMOST;     else ex &= ~(LONG_PTR) WS_EX_TOPMOST;
    if (clickThrough)  ex |= WS_EX_TRANSPARENT; else ex &= ~(LONG_PTR) WS_EX_TRANSPARENT;
    SetWindowLongPtrW (hwnd, GWL_EXSTYLE, ex);

    SetWindowPos (hwnd, alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                  0, 0, 0, 0,
                  SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_NOACTIVATE);
}

// Per-pixel-alpha "layered window" update from a premultiplied JUCE ARGB image.
void OverlayWindow::pushLayeredImage (const juce::Image& img)
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

void OverlayWindow::applyNativeStyle() {}

void OverlayWindow::pushLayeredImage (const juce::Image&) {}

#endif

//==============================================================================
void OverlayWindow::timerCallback()
{
    drainUdp();
    maybeRestartLoopback();

    const auto bs = computeBeatState();

    if (activeSource != lastLoggedSource)
    {
        lastLoggedSource = activeSource;

        auto nameOf = [] (BeatSource s) -> juce::String
        {
            switch (s)
            {
                case BeatSource::Vst:    return u8 ("VST");
                case BeatSource::System: return u8 ("系统音源");
                case BeatSource::Auto:   return u8 ("自动");
                case BeatSource::Off:    break;
            }
            return u8 ("待机");
        };

        juce::Logger::writeToLog ("[Beat] active source -> " + nameOf (activeSource));
    }

    renderFrame (bs);
}

void OverlayWindow::drainUdp()
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

BeatState OverlayWindow::computeBeatState()
{
    const auto now = juce::Time::getMillisecondCounter();
    const bool freshHost = hostHasPacket && (now - lastHostPacketMs) < hostTimeoutMs;

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

    // Auto: old authority order — fresh host transport, then system loopback,
    // then idle.
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

void OverlayWindow::applySourceMode()
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
void OverlayWindow::maybeRestartLoopback()
{
    if (loopback == nullptr)
        return;

    const bool wantSystem = (sourceMode == BeatSource::Auto
                             || sourceMode == BeatSource::System);

    if (! wantSystem || loopback->isRunning())
        return;

    // Give up after a few attempts: a broken audio service must not fill the
    // log with device scans every three seconds. Re-selecting the source in the
    // menu resets the counter (see applySourceMode).
    if (loopbackRetryCount >= 3)
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

void OverlayWindow::renderFrame (const BeatState& bs)
{
    auto* peer = getPeer();
    if (peer == nullptr)
        return;

    const auto area = peer->getBounds();   // native window size in pixels
    if (area.isEmpty())
        return;

    const int   pw    = area.getWidth();
    const int   ph    = area.getHeight();
    const float scale = (float) pw / (float) getWidth();

    juce::Image img (juce::Image::ARGB, pw, ph, true);
    {
        juce::Graphics g (img);
        if (scale != 1.0f)
            g.addTransform (juce::AffineTransform::scale (scale));

        CharacterRenderer* renderer =
            (spriteChar != nullptr) ? (CharacterRenderer*) spriteChar.get() : &procChar;

        renderer->render (g, getWidth(), getHeight(), bs,
                          juce::Time::getMillisecondCounterHiRes(), mouseHeld);
    }

    pushLayeredImage (img);
}

//==============================================================================
void OverlayWindow::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        buildTrayMenu (menu);
        menu.showMenuAsync (juce::PopupMenu::Options()
                                .withTargetScreenArea ({ e.getScreenX(), e.getScreenY(), 1, 1 }),
                            [safeThis = juce::Component::SafePointer<OverlayWindow> (this)] (int r)
                            {
                                if (safeThis != nullptr)
                                    safeThis->handleMenuResult (r);
                            });
        return;
    }

    // Shift+drag resizes instead of moving. Starting from any opaque pixel
    // sidesteps the layered-window rule that fully transparent areas never
    // receive mouse input, so no visible grab handle is needed.
    if (e.mods.isShiftDown())
    {
        resizing         = true;
        dragGrab         = e.getScreenPosition().toInt();
        dragStartBounds  = getBounds();
        dragStartPercent = sizePercent;
        mouseHeld        = true;
        return;
    }

    dragGrab         = e.getScreenPosition().toInt();
    dragWindowOrigin = getPosition();
    mouseHeld = true;
}

void OverlayWindow::mouseDrag (const juce::MouseEvent& e)
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

        if (pct != sizePercent)
        {
            sizePercent = pct;
            applyCharacterSize();   // keeps the aspect ratio and screen limits
        }
        return;
    }

    setTopLeftPosition (dragWindowOrigin
                        + (e.getScreenPosition().toInt() - dragGrab));
}

void OverlayWindow::mouseUp (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);

    if (resizing)
    {
        resizing = false;
        saveToProperties();   // resize does not go through the menu path
    }

    mouseHeld = false;
}

void OverlayWindow::userTriedToCloseWindow()
{
    if (auto* app = juce::JUCEApplication::getInstance())
        app->systemRequestedQuit();
}

//==============================================================================
bool OverlayWindow::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        const auto ext = juce::File (f).getFileExtension().toLowerCase();
        if (ext == ".png" || ext == ".gif" || ext == ".jpg" || ext == ".jpeg")
            return true;
    }
    return false;
}

void OverlayWindow::filesDropped (const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused (x, y);

    for (const auto& f : files)
    {
        const auto file = juce::File (f);
        const auto ext  = file.getFileExtension().toLowerCase();

        if (ext == ".png" || ext == ".gif" || ext == ".jpg" || ext == ".jpeg")
        {
            juce::Logger::writeToLog ("[Character] sprite sheet dropped: " + f);
            addRootFromChoice (file.getParentDirectory());
            break;
        }
    }
}

//==============================================================================
void OverlayWindow::buildTrayMenu (juce::PopupMenu& menu)
{
    menu.addItem (1, u8 ("点击穿透"), true, clickThrough);
    menu.addItem (2, u8 ("置顶显示"), true, alwaysOnTop);

    // ---- beat source ------------------------------------------------------
    {
        juce::PopupMenu src;

        src.addItem (beatSourceAuto,   u8 ("自动（VST 优先）"), true, sourceMode == BeatSource::Auto);
        src.addItem (beatSourceVst,    u8 ("仅 VST（UDP）"),    true, sourceMode == BeatSource::Vst);
        src.addItem (beatSourceSystem, u8 ("仅系统音源"),       true, sourceMode == BeatSource::System);
        src.addItem (beatSourceOff,    u8 ("关闭（待机）"),     true, sourceMode == BeatSource::Off);

        src.addSeparator();

        juce::String active;
        switch (activeSource)
        {
            case BeatSource::Vst:    active = u8 ("VST");     break;
            case BeatSource::System: active = u8 ("系统音源"); break;
            case BeatSource::Auto:   active = u8 ("自动");     break;
            case BeatSource::Off:    active = u8 ("待机");     break;
        }
        src.addItem (-1, u8 ("当前：") + active, false, false);

        menu.addSubMenu (u8 ("节拍源"), src);
    }

    menu.addSeparator();

    // ---- character management list (Fruity Dance "skin" selector style) --
    {
        juce::PopupMenu chars;
        chars.addItem (proceduralId, u8 ("程序小人（内置）"), true, spriteChar == nullptr);

        const int n = characters.getNumEntries();
        if (n > 0)
            chars.addSeparator();

        for (int i = 0; i < n && (charFirst + i) <= charLast; ++i)
        {
            const auto& e = characters.getEntry (i);
            const bool has = characters.entryHasTxt (i);
            chars.addItem (charFirst + i,
                           has ? e.displayName : e.displayName + u8 ("  (缺 .txt)"),
                           has,
                           isSpriteActive (e));
        }

        chars.addSeparator();
        chars.addItem (importDirId, u8 ("从精灵表文件夹导入…"), true, false);
        chars.addItem (addFileId,   u8 ("添加单个精灵表…"),     true, false);
        chars.addSeparator();
        chars.addItem (openDirId,  u8 ("打开人物文件夹"), true, false);
        chars.addItem (refreshId,  u8 ("刷新列表"),       true, false);

        menu.addSubMenu (u8 ("人物"), chars);
    }

    // ---- dance rows -------------------------------------------------------
    {
        juce::PopupMenu dances;

        if (spriteChar == nullptr)
        {
            dances.addItem (10, u8 ("自动轮换"), true, patternLock < 0);
            dances.addItem (11, u8 ("摆动"),     true, patternLock == 0);
            dances.addItem (12, u8 ("挥手"),     true, patternLock == 1);
            dances.addItem (13, u8 ("踏步"),     true, patternLock == 2);
        }
        else
        {
            dances.addItem (rowAuto, u8 ("自动轮换"), true, rowLock < 0);

            const int danceRows = spriteChar->getDanceRowCount();
            for (int i = 0; i < danceRows && (rowFirst + i) <= rowLast; ++i)
            {
                const auto rowName = spriteChar->getDanceRowName (i).trim();
                dances.addItem (rowFirst + i,
                                rowName.isEmpty()
                                    ? u8 ("（未命名行 ") + juce::String (i + 1) + u8 ("）")
                                    : rowName,
                                true, rowLock == i);
            }

            dances.addSeparator();

            juce::PopupMenu anim;

            auto formatSpeed = [] (double v) -> juce::String
            {
                juce::String s (v, 2);
                while (s.endsWithChar ('0') && s.containsChar ('.'))
                    s = s.dropLastCharacters (1);
                if (s.endsWithChar ('.'))
                    s = s.dropLastCharacters (1);
                return u8 ("×") + s;
            };

            anim.addItem (-1, u8 ("当前 ") + formatSpeed (speedMultiplier), false, false);
            anim.addSeparator();

            static const double presets[] { 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0 };
            static const char*  labels[]  { "×0.25", "×0.5", "×0.75", "×1",
                                           "×1.25", "×1.5", "×2", "×3", "×4" };

            for (int i = 0; i < 9; ++i)
                anim.addItem (speedPresetFirst + i, u8 (labels[i]), true,
                              std::abs (speedMultiplier - presets[i]) < 1.0e-6);

            anim.addSeparator();
            anim.addItem (speedMinus, u8 ("− 0.05"), speedMultiplier > 0.25, false);
            anim.addItem (speedPlus,  u8 ("+ 0.05"), speedMultiplier < 4.0,  false);
            anim.addSeparator();
            anim.addItem (mirrorId, u8 ("水平镜像"), true, mirror);
            anim.addItem (blendId,  u8 ("帧间混合"), true, blend);

            dances.addSubMenu (u8 ("动画设置"), anim);
        }

        menu.addSubMenu (u8 ("舞步"), dances);
    }

    // ---- character size ---------------------------------------------------
    {
        juce::PopupMenu sizes;
        sizes.addItem (-1, u8 ("当前 ") + juce::String (sizePercent) + "%", false, false);
        sizes.addSeparator();

        static const int presets[] { 50, 75, 100, 125, 150, 200, 300 };
        for (int i = 0; i < 7; ++i)
            sizes.addItem (sizeFirst + i, juce::String (presets[i]) + "%", true,
                           sizePercent == presets[i]);

        sizes.addSeparator();
        sizes.addItem (-1, u8 ("按住 Shift 拖拽可自由缩放"), false, false);

        menu.addSubMenu (u8 ("角色大小"), sizes);
    }

    menu.addSeparator();
    menu.addItem (3, u8 ("回到屏幕角落"), true, false);
    menu.addItem (4, u8 ("退出 Overlay"), true, false);
}

void OverlayWindow::handleMenuResult (int id)
{
    switch (id)
    {
        case 1:  clickThrough = ! clickThrough; applyNativeStyle(); break;
        case 2:  alwaysOnTop  = ! alwaysOnTop;  applyNativeStyle(); break;
        case 10: patternLock = -1; break;
        case 11: patternLock = 0;  break;
        case 12: patternLock = 1;  break;
        case 13: patternLock = 2;  break;
        case 3:  snapToCorner(); break;
        case 4:
            if (auto* app = juce::JUCEApplication::getInstance())
                app->systemRequestedQuit();
            break;

        case proceduralId:  selectProceduralCharacter(); break;
        case importDirId:   promptForDirectory(); break;
        case addFileId:     promptForFile(); break;
        case openDirId:     characters.getCharactersDir().startAsProcess(); break;
        case refreshId:     characters.rescan(); break;

        case rowAuto:   rowLock = -1; break;

        case beatSourceAuto:   sourceMode = BeatSource::Auto;   applySourceMode(); break;
        case beatSourceVst:    sourceMode = BeatSource::Vst;    applySourceMode(); break;
        case beatSourceSystem: sourceMode = BeatSource::System; applySourceMode(); break;
        case beatSourceOff:    sourceMode = BeatSource::Off;    applySourceMode(); break;

        case speedMinus: speedMultiplier = juce::jlimit (0.25, 4.0, speedMultiplier - 0.05); break;
        case speedPlus:  speedMultiplier = juce::jlimit (0.25, 4.0, speedMultiplier + 0.05); break;
        case mirrorId:  mirror = ! mirror; break;
        case blendId:   blend  = ! blend;  break;

        default:
            if (id >= charFirst && id <= charLast)
            {
                selectSpriteCharacter (id - charFirst);
            }
            else if (id >= rowFirst && id <= rowLast)
            {
                rowLock = id - rowFirst;
            }
            else if (id >= speedPresetFirst && id <= speedPresetLast)
            {
                static const double presets[] { 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0 };
                speedMultiplier = presets[id - speedPresetFirst];
            }
            else if (id >= sizeFirst && id <= sizeLast)
            {
                static const int presets[] { 50, 75, 100, 125, 150, 200, 300 };
                sizePercent = presets[id - sizeFirst];
                applyCharacterSize();
            }
            break;
    }

    refreshSpriteSettings();
    saveToProperties();
}

//==============================================================================
void OverlayWindow::selectProceduralCharacter()
{
    spriteChar.reset();
    activePath.clear();
    applyCharacterSize();
}

void OverlayWindow::selectSpriteCharacter (int entryIndex)
{
    auto sheet = characters.loadSheet (entryIndex);
    if (! sheet.ok)
    {
        juce::Logger::writeToLog ("[Character] failed to load '"
                                  + characters.getEntry (entryIndex).displayName
                                  + "': " + sheet.error);
        selectProceduralCharacter();
        return;
    }

    activePath = sheet.sourceFile.getFullPathName();
    spriteChar = std::make_unique<SpriteCharacter> (std::move (sheet));
    applyCharacterSize();
}

void OverlayWindow::applyCharacterSize()
{
    const double user = juce::jlimit (0.25, 4.0, sizePercent / 100.0);

    int nw = 0, nh = 0;

    if (spriteChar == nullptr)
    {
        displayAspect = 240.0 / 360.0;
        nw = juce::jmax (16, juce::roundToInt (240.0 * user));
        nh = juce::jmax (16, juce::roundToInt (360.0 * user));
    }
    else
    {
        int cw = 0, ch = 0;
        spriteChar->getCellSize (cw, ch);

        displayAspect = (double) cw / (double) ch;

        const double targetH = 320.0, capW = 340.0;
        double s = juce::jmin (targetH / (double) ch, capW / (double) cw);
        s = juce::jlimit (1.0, 4.0, s);

        nw = juce::jmax (16, juce::roundToInt ((double) cw * s * user));
        nh = juce::jmax (16, juce::roundToInt ((double) ch * s * user));
    }

    // Clamp as one scale factor, not width and height separately, otherwise a
    // large size on a small screen would squash the character.
    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto u = display->userArea;
        const double maxW = juce::jmax (16.0, (double) u.getWidth()  - 20.0);
        const double maxH = juce::jmax (16.0, (double) u.getHeight() - 40.0);

        double fit = 1.0;
        if ((double) nw > maxW) fit = juce::jmin (fit, maxW / (double) nw);
        if ((double) nh > maxH) fit = juce::jmin (fit, maxH / (double) nh);

        nw = juce::jmax (16, juce::roundToInt ((double) nw * fit));
        nh = juce::jmax (16, juce::roundToInt ((double) nh * fit));
    }

    setSize (nw, nh);

    if (isOnDesktop())
    {
        juce::Rectangle<int> keep { getX(), getY(), nw, nh };
        if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            const auto u = display->userArea;
            if (! u.contains (keep))
                snapToCorner();
        }
    }
}

void OverlayWindow::snapToCorner()
{
    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        setTopLeftPosition (display->userArea.getRight()  - getWidth()  - 40,
                            display->userArea.getBottom() - getHeight() - 40);
}

void OverlayWindow::refreshSpriteSettings()
{
    procChar.setPatternLock (patternLock);

    if (spriteChar == nullptr)
        return;

    spriteChar->setSpeed   (speedMultiplier);
    spriteChar->setMirror  (mirror);
    spriteChar->setBlend   (blend);
    spriteChar->setRowLock (rowLock);
}

bool OverlayWindow::isSpriteActive (const CharacterManager::Entry& e) const
{
    return spriteChar != nullptr && e.imageFile == juce::File (activePath);
}

void OverlayWindow::addRootFromChoice (const juce::File& dir)
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

    juce::Logger::writeToLog ("[Character] added root folder: " + dir.getFullPathName()
                              + " -> now " + juce::String (characters.getNumEntries())
                              + " sheet(s) accessible");
}

void OverlayWindow::handleDirectoryResult (const juce::File& dir)
{
    if (dir.isDirectory())
        addRootFromChoice (dir);
}

void OverlayWindow::handleFileResult (const juce::File& file)
{
    if (file.existsAsFile())
    {
        juce::Logger::writeToLog ("[Character] sheet chosen file: "
                                  + file.getFullPathName());
        addRootFromChoice (file.getParentDirectory());
    }
}

void OverlayWindow::promptForDirectory()
{
    chooser = std::make_unique<juce::FileChooser> (
        u8 ("选择包含精灵表的文件夹（例如 FL Studio 的 Fruity Dance\\Artwork）"),
        characters.getCharactersDir());

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [safeThis = juce::Component::SafePointer<OverlayWindow> (this)] (const juce::FileChooser& fc)
                          {
                              if (safeThis != nullptr)
                                  safeThis->handleDirectoryResult (fc.getResult());
                          });
}

void OverlayWindow::promptForFile()
{
    chooser = std::make_unique<juce::FileChooser> (
        u8 ("选择精灵表图片（自动读取其所在的同名 .txt）；该文件所在文件夹会加入人物目录"),
        characters.getCharactersDir(),
        "*.png;*.gif;*.jpg;*.jpeg");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safeThis = juce::Component::SafePointer<OverlayWindow> (this)] (const juce::FileChooser& fc)
                          {
                              if (safeThis != nullptr)
                                  safeThis->handleFileResult (fc.getResult());
                          });
}

//==============================================================================
void OverlayWindow::saveToProperties()
{
    settings.setValue ("characterPath", activePath);
    settings.setValue ("extraRoots", characters.getExtraRootsAsString());
    settings.setValue ("rowLock", rowLock);
    settings.setValue ("speedMultiplier", speedMultiplier);
    settings.setValue ("sizePercent", sizePercent);
    settings.setValue ("beatSource", (int) sourceMode);
    settings.setValue ("mirror", mirror ? 1 : 0);
    settings.setValue ("blend", blend ? 1 : 0);
    settings.setValue ("alwaysOnTop", alwaysOnTop ? 1 : 0);
    settings.setValue ("windowX", getX());
    settings.setValue ("windowY", getY());
    // click-through is intentionally NOT persisted: a saved "on" would lock
    // the window away from both drag and right-click.
}

} // namespace dt
