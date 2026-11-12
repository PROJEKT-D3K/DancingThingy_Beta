#pragma once

#include <juce_core/juce_core.h>

#include <initializer_list>

namespace dt
{
// Narrow literals compiled with /utf-8 carry UTF-8 bytes, but juce::String's
// char* constructor deliberately treats 8-bit data as ASCII/CP1252 (see
// juce_String.cpp). Wrap user-visible literals like this instead:
//   dt::u8 ("点击穿透")
//
// u8() stays the helper for log lines and other non-UI text; everything the
// user reads goes through tr()/trf() below so it follows the selected language.
inline juce::String u8 (const char* s)
{
    return juce::String (juce::CharPointer_UTF8 (s));
}

//------------------------------------------------------------------------------
// UI language. The tag persisted on disk is "zh" / "en" / "ja".
enum class Language { Zh = 0, En = 1, Ja = 2 };

inline const char* languageTag (Language l) noexcept
{
    switch (l)
    {
        case Language::En: return "en";
        case Language::Ja: return "ja";
        case Language::Zh: break;
    }
    return "zh";
}

// The overlay and the VST are separate processes and both may write the tag, so
// it lives in its own small file instead of settings.xml (PropertiesFile
// rewrites that file whole, which would clobber a concurrent write).
inline Language& currentLanguage()
{
    static Language lang = Language::Zh;
    return lang;
}

inline Language getLanguage()            { return currentLanguage(); }
inline void     setLanguage (Language l) { currentLanguage() = l; }

// Unreadable tag -> keep what we have; first run -> follow the OS.
inline Language languageFromTag (const juce::String& tag)
{
    const auto t = tag.trim().toLowerCase();
    if (t.startsWith ("zh")) return Language::Zh;
    if (t.startsWith ("ja")) return Language::Ja;
    if (t.startsWith ("en")) return Language::En;
    return getLanguage();
}

inline Language systemLanguage()
{
    const auto l = juce::SystemStats::getUserLanguage().toLowerCase();
    if (l.startsWith ("zh")) return Language::Zh;
    if (l.startsWith ("ja")) return Language::Ja;
    if (l.startsWith ("en")) return Language::En;
    return Language::Zh;
}

//------------------------------------------------------------------------------
// Text ids, grouped by area.
enum class Text
{
    // common
    close, remove, add, current, none,

    // character window menu
    menuClickThrough, menuAlwaysOnTop, menuBeatSource, menuCharacter, menuDance,
    menuAnimation, menuSize, menuAddCharacter, menuFromSheets, menuRemoveCharacter,
    menuSettings, menuSnapCorner, menuQuitOverlay,

    // beat source
    beatAutoVst, beatVstOnly, beatSystemOnly, beatOff,
    beatVst, beatSystem, beatAuto, beatIdle,

    // character list
    charProcedural, charMissingTxt, charUnnamed, charImportFolder, charAddSheet,
    charOpenFolder, charRefresh,

    // dance + animation + size
    danceAutoRotate, danceSway, danceWave, danceStep, danceUnnamedRow, danceCurrent,
    animMirror, animBlend, sizeCurrent, sizeShiftHint,

    // tray
    trayCharacters, trayAddCharacter, trayTooltip,

    // settings window
    settingsTitle, settingsLanguage, settingsUdpPort, settingsUdpListening,
    settingsUdpBindFailed, settingsUdpHint, settingsVstTimeout, settingsSysRetries,
    settingsRefreshRate, settingsShowGrabArea, settingsCharacters,
    settingsNoCharacters, settingsAddCharacter,

    // file chooser
    chooserFolderTitle, chooserSheetTitle,

    // plugin editor
    pluginWaiting, pluginHint, pluginNoTransport, pluginNoTransportDetail,
    pluginPlaying, pluginPaused, pluginBeat, pluginBar, pluginLanguage, pluginPort
};

struct TextEntry
{
    const char* zh;
    const char* en;
    const char* ja;
};

// One exhaustive switch: an id added without a translation is obvious here.
constexpr TextEntry textEntryOf (Text t)
{
    switch (t)
    {
        case Text::close:            return { "关闭", "Close", "閉じる" };
        case Text::remove:           return { "移除", "Remove", "削除" };
        case Text::add:              return { "添加", "Add", "追加" };
        case Text::current:          return { "当前", "Current", "現在" };
        case Text::none:             return { "（无）", "(none)", "（なし）" };

        case Text::menuClickThrough:    return { "点击穿透", "Click-through", "クリック透過" };
        case Text::menuAlwaysOnTop:     return { "置顶显示", "Always on top", "最前面に表示" };
        case Text::menuBeatSource:      return { "节拍源", "Beat source", "ビートソース" };
        case Text::menuCharacter:       return { "人物", "Character", "キャラクター" };
        case Text::menuDance:           return { "舞步", "Dance", "ダンス" };
        case Text::menuAnimation:       return { "动画设置", "Animation", "アニメーション" };
        case Text::menuSize:            return { "角色大小", "Character size", "キャラクターサイズ" };
        case Text::menuAddCharacter:    return { "再加一个角色", "Add another character", "キャラクターを追加" };
        case Text::menuFromSheets:      return { "从人物表选…", "From character sheets…", "シートから選択…" };
        case Text::menuRemoveCharacter: return { "移除这个角色", "Remove this character", "このキャラクターを削除" };
        case Text::menuSettings:        return { "设置…", "Settings…", "設定…" };
        case Text::menuSnapCorner:      return { "回到屏幕角落", "Snap to corner", "画面の隅へ移動" };
        case Text::menuQuitOverlay:     return { "退出 Overlay", "Quit overlay", "Overlay を終了" };

        case Text::beatAutoVst:    return { "自动（VST 优先）", "Automatic (VST preferred)", "自動（VST 優先）" };
        case Text::beatVstOnly:    return { "仅 VST（UDP）", "VST only (UDP)", "VST のみ（UDP）" };
        case Text::beatSystemOnly: return { "仅系统音源", "System audio only", "システム音声のみ" };
        case Text::beatOff:        return { "关闭（待机）", "Off (idle)", "オフ（待機）" };
        case Text::beatVst:        return { "VST", "VST", "VST" };
        case Text::beatSystem:     return { "系统音源", "System audio", "システム音声" };
        case Text::beatAuto:       return { "自动", "Auto", "自動" };
        case Text::beatIdle:       return { "待机", "Idle", "待機" };

        case Text::charProcedural:   return { "程序小人（内置）", "Built-in figure", "内蔵キャラクター" };
        case Text::charMissingTxt:   return { "（缺 .txt）", "(missing .txt)", "（.txt なし）" };
        case Text::charUnnamed:      return { "角色 %1", "Character %1", "キャラクター %1" };
        case Text::charImportFolder: return { "从精灵表文件夹导入…", "Import sprite folder…", "スプライトのフォルダーを読み込む…" };
        case Text::charAddSheet:     return { "添加单个精灵表…", "Add a sprite sheet…", "スプライトシートを追加…" };
        case Text::charOpenFolder:   return { "打开人物文件夹", "Open character folder", "キャラクターフォルダーを開く" };
        case Text::charRefresh:      return { "刷新列表", "Refresh list", "リストを更新" };

        case Text::danceAutoRotate: return { "自动轮换", "Auto rotate", "自動切り替え" };
        case Text::danceSway:       return { "摆动", "Sway", "スウェイ" };
        case Text::danceWave:       return { "挥手", "Wave", "手振り" };
        case Text::danceStep:       return { "踏步", "Step", "ステップ" };
        case Text::danceUnnamedRow: return { "（未命名行 %1）", "(unnamed row %1)", "（名称未設定の行 %1）" };
        case Text::danceCurrent:    return { "当前 ×%1", "Current ×%1", "現在 ×%1" };
        case Text::animMirror:      return { "水平镜像", "Mirror horizontally", "左右反転" };
        case Text::animBlend:       return { "帧间混合", "Frame blending", "フレーム補間" };
        case Text::sizeCurrent:     return { "当前 %1%", "Current %1%", "現在 %1%" };
        case Text::sizeShiftHint:   return { "按住 Shift 拖拽可自由缩放", "Shift+drag to resize freely", "Shift+ドラッグで自由に拡大縮小" };

        case Text::trayCharacters:   return { "角色", "Characters", "キャラクター" };
        case Text::trayAddCharacter: return { "新增角色…", "Add character…", "キャラクターを追加…" };
        case Text::trayTooltip:      return { "DanceOverlay", "DanceOverlay", "DanceOverlay" };

        case Text::settingsTitle:         return { "DanceThingy 设置", "DanceThingy Settings", "DanceThingy の設定" };
        case Text::settingsLanguage:      return { "语言", "Language", "言語" };
        case Text::settingsUdpPort:       return { "UDP 端口", "UDP port", "UDP ポート" };
        case Text::settingsUdpListening:  return { "正在监听端口 %1", "Listening on port %1", "ポート %1 で待ち受け中" };
        case Text::settingsUdpBindFailed: return { "端口 %1 绑定失败", "Could not bind port %1", "ポート %1 をバインドできません" };
        case Text::settingsUdpHint:       return { "VST 插件会读取同一份设置，把心跳发到这个端口。",
                                                   "The VST plugin reads the same settings and sends its heartbeat here.",
                                                   "VST プラグインも同じ設定を読み、ここへハートビートを送ります。" };
        case Text::settingsVstTimeout:    return { "VST 心跳超时（ms）", "VST timeout (ms)", "VST タイムアウト（ms）" };
        case Text::settingsSysRetries:    return { "系统音源重试次数", "System audio retries", "システム音声の再試行回数" };
        case Text::settingsRefreshRate:   return { "刷新率", "Refresh rate", "リフレッシュレート" };
        case Text::settingsShowGrabArea:  return { "显示抓取范围", "Show grab area", "操作範囲を表示" };
        case Text::settingsCharacters:    return { "角色列表", "Character list", "キャラクター一覧" };
        case Text::settingsNoCharacters:  return { "还没有角色。用“新增角色…”添加一个。",
                                                   "No characters yet. Use \"Add character…\" to create one.",
                                                   "キャラクターがありません。「キャラクターを追加…」から作成してください。" };
        case Text::settingsAddCharacter:  return { "新增角色…", "Add character…", "キャラクターを追加…" };

        case Text::chooserFolderTitle: return { "选择包含精灵表的文件夹（例如 FL Studio 的 Fruity Dance\\Artwork）",
                                                "Choose a folder with sprite sheets (e.g. FL Studio's Fruity Dance\\Artwork)",
                                                "スプライトシートのフォルダーを選択（例：FL Studio の Fruity Dance\\Artwork）" };
        case Text::chooserSheetTitle:  return { "选择精灵表图片（自动读取其所在的同名 .txt）",
                                                "Choose a sprite sheet image (its same-named .txt is read too)",
                                                "スプライトシート画像を選択（同名の .txt も読み込みます）" };

        case Text::pluginWaiting:           return { "等待宿主 transport 信息…", "Waiting for the host transport…", "ホストのトランスポート情報を待機しています…" };
        case Text::pluginHint:              return { "DanceOverlay 悬浮窗启动后，小人就会踩着这里的拍子跳舞。\n UDP -> 127.0.0.1:%1（15 ms 心跳）",
                                                    "Start the DanceOverlay window and the characters dance to this transport.\n UDP -> 127.0.0.1:%1 (15 ms heartbeat)",
                                                    "DanceOverlay を起動すると、キャラクターがこのテンポで踊ります。\n UDP -> 127.0.0.1:%1（15 ms ハートビート）" };
        case Text::pluginNoTransport:       return { "宿主未提供 transport 信息", "The host exposes no transport", "ホストがトランスポート情報を提供していません" };
        case Text::pluginNoTransportDetail: return { "有些宿主对普通效果器隐藏播放头；悬浮窗会退回自听模式。",
                                                     "Some hosts hide the playhead from plain effects; the overlay falls back to self-listen.",
                                                     "一部のホストは通常のエフェクトに再生位置を公開しません。オーバーレイは自己 listen に切り替わります。" };
        case Text::pluginPlaying:           return { "● 播放中 - 正在广播节拍", "● Playing - broadcasting beats", "● 再生中 - ビートを送信中" };
        case Text::pluginPaused:            return { "○ 已暂停 - 心跳仍在对时", "○ Paused - heartbeat keeps time", "○ 一時停止 - ハートビートで同期中" };
        case Text::pluginBeat:              return { "拍位", "beat", "拍" };
        case Text::pluginBar:               return { "小节", "bar", "小節" };
        case Text::pluginLanguage:          return { "语言", "Language", "言語" };
        case Text::pluginPort:              return { "端口 %1", "Port %1", "ポート %1" };
    }

    return { "", "", "" };
}

inline const char* textOf (Text t, Language lang)
{
    const auto e = textEntryOf (t);

    switch (lang)
    {
        case Language::En: return e.en;
        case Language::Ja: return e.ja;
        case Language::Zh: break;
    }

    return e.zh;
}

inline juce::String tr (Text t)
{
    return u8 (textOf (t, getLanguage()));
}

// Substitutes %1, %2, ... from the back, so a value that itself contains "%1"
// is not substituted twice.
inline juce::String trf (Text t, std::initializer_list<juce::String> args)
{
    auto s = tr (t);

    int index = (int) args.size();
    for (auto it = args.end(); it != args.begin();)
    {
        --it;
        s = s.replace ("%" + juce::String (index), *it);
        --index;
    }

    return s;
}

} // namespace dt
