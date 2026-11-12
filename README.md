# DanceThingy

**A Fruity Dance–style desktop companion for any DAW — not just FL Studio.**

[中文](README.zh-CN.md) · [日本語](README.ja.md)

![Version](https://img.shields.io/badge/version-0.4.0-green)
![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11%20x64-0078D6)
![License](https://img.shields.io/badge/license-GPL--3.0-blue)

DanceThingy reproduces the idea behind Image-Line's Fruity Dance as two independent, cooperating pieces:

- **DanceOverlay** — a transparent, always-on-top desktop character with no background window. Click-through is optional. She sits above every other window and dances.
- **DanceThingy (VST3)** — a passthrough effect you can drop on any DAW track. It never touches the audio; it reads the host transport and broadcasts beats to the overlay over local UDP.

## Why two pieces?

| Setup | What you get |
|---|---|
| Overlay + VST3 | Choreography follows the host's bar/beat position, so moves land on the downbeat and never drift. |
| Overlay only | The overlay listens to the system output through WASAPI loopback and estimates tempo from onsets. Works with any source (DAW, browser, media player), but timing is approximate. |
| VST3 only | The plugin panel shows transport status. Dancing requires the overlay on the same machine. |

## Features

- Transparent per-pixel-alpha Win32 layered window; no background box.
- **Any number of dancers at once**, each with its own sheet, dance row, speed, size, mirror/blend state and position. The same sheet can be used twice.
- Always-on-top, with optional click-through (transparent pixels also let clicks pass).
- Drag to move; hold **Shift** and drag to resize continuously with the aspect ratio locked.
- The interactive area is cropped to the artwork's bounding box, so dragging and right-clicking work anywhere on the character; **Show grab area** outlines it.
- **Settings window** (right-click → `Settings…`, or start with `--settings`): beat source, UDP port, VST timeout, retry count, refresh rate, character list.
- **Chinese / English / Japanese UI**, switchable live from the settings window or the plugin editor; the choice is shared by both processes.
- Four beat sources: **Auto (VST first)**, **VST only**, **System audio only**, **Off**.
- Native WASAPI loopback capture with onset detection and BPM estimation. Follows the default output device and falls back to other active render endpoints when loopback is refused.
- Speed **×0.25–×4** in nine presets, plus **±0.05** fine steps. The frame phase is re-based when speed changes, so the animation does not jump.
- Character size **50–300%** in seven presets, or **25–400%** continuously via Shift-drag.
- Fruity Dance–compatible sprite sheets: one image, exactly 8 columns, arbitrary rows, plus a same-named `.txt` listing row names.
- Built-in procedural character that needs no assets.
- Import FL Studio's Artwork folder in place (zero-copy), add a single sheet, or drag a sheet onto the character.
- Settings persist to `%APPDATA%\DanceOverlay\settings.xml`; the UI language lives in `%APPDATA%\DanceOverlay\language.txt`; log at `%APPDATA%\DanceOverlay\overlay.log`.
- Tray icon, horizontal mirror, inter-frame blending, and dance-row locking.

## Status

- **v0.4.0** — several characters at once, a settings window, zh/en/ja UI and a configurable UDP port. Windows only.
- Manual UX acceptance (menu clicks, Shift-resize feel, speed smoothness) is still pending.
- System-audio mode is heuristic: it needs a few beats to warm up and returns to idle after roughly 2.2 seconds of silence.
- Some hosts do not expose transport information to effect plugins. Use system-audio mode there.
- Known Windows audio-service issue: if loopback initialization fails with `0x800706CC` (`RPC_S_DUPLICATE_ENDPOINT`), restart the audio service as administrator. See [Troubleshooting](#troubleshooting).

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 Build Tools with the C++ workload
- CMake ≥ 3.22
- Git (FetchContent pulls JUCE 8.0.4 on the first configure)

Toolchain one-liner (elevated PowerShell/CMD):

```cmd
choco install visualstudio2022buildtools --packageParameters "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended" -y
choco install cmake -y
```

`tools\install-toolchain.cmd` wraps these two commands.

## Build

The first configure downloads JUCE (100+ MB) and the first build compiles it. That is expected.

```cmd
git clone https://github.com/<your-account>/DanceThingy.git
cd DanceThingy
cmake --preset default
cmake --build --preset default
```

Artifacts:

- `build\DanceOverlay_artefacts\Release\DanceOverlay.exe` — the overlay
- `build\DanceThingy_artefacts\Release\VST3\DanceThingy.vst3` — the plugin

If a previous overlay is still running, close it first; MSVC cannot overwrite a locked `.exe`:

```cmd
taskkill /IM DanceOverlay.exe
```

## Install

1. Copy `DanceThingy.vst3` to `C:\Program Files\Common Files\VST3\` (administrator). `tools\deploy-vst3.cmd` does this from the build tree.
2. Put `DanceOverlay.exe` anywhere and run it. The optional installer below automates this and registers an uninstaller.

### Build the installer

`installer\build-installer.cmd` produces `dist\DanceThingy-<version>-Setup.exe` with NSIS 3.x. It installs both components, registers an uninstaller, and bundles the VC++ 2015-2022 runtime for machines that do not have it. Set `NSIS_HOME` if `makensis.exe` is not in `E:\NSIS`.

```cmd
installer\build-installer.cmd
```

Add `test` for a user-level sandbox build that installs under `%TEMP%` (verification only, not for distribution).

## Usage

### Overlay

- It launches idle and breathes on the spot.
- Drag a character to move it; hold **Shift** and drag it to resize. Right-click a character for **that character's** menu, or the tray icon for the global one.
- Character menu: which sheet, dance row, animation settings (speed, mirror, blend), character size, **Add another character**, **Remove this character** (disabled while only one is left), **Settings…**, click-through, always-on-top, back to corner, quit.
- **Settings…** (or `DanceOverlay.exe --settings`) opens the settings window:
  - **Language** — 中文 / English / 日本語, applied immediately everywhere, including the VST editor.
  - **Beat source** — the four modes below.
  - **UDP port** — 1024–65535, default 53711. Rebinding is immediate and the VST follows within a second. A failed bind falls back to the previous port and says so.
  - **VST timeout**, **system audio retries**, **refresh rate** (30/60/120 Hz).
  - **Show grab area** — draws the interactive rectangle so it is obvious where the character can be grabbed.
  - **Character list** — add or remove dancers.
- Beat source modes:
  - `Auto (VST first)` — default. Uses VST heartbeats younger than the timeout; otherwise system audio; otherwise idle.
  - `VST only (UDP)` — never falls back. No heartbeat means idle.
  - `System audio only` — ignores UDP entirely, so a paused plugin cannot shadow the loopback signal with its 15 ms heartbeat.
  - `Off` — no capture at all.
- Several characters share the transport but keep their own row, speed, size, mirror/blend and position. Dragging, resizing and the menu are per character.
- **Click-through**: mouse events pass through every character. The tray icon remains the way back. This is deliberately not persisted, so you cannot lock yourself out.

### DAW side

Insert DanceThingy as a normal effect on any track, then play. The plugin sends one UDP packet every ~15 ms. Multiple instances are fine; the overlay trusts the freshest heartbeat. When playback stops, the character freezes on the current frame.

### Sprite sheets

The format follows the Fruity Dance manual, plus one extension:

- One image per character (`PNG`, `JPEG`, or `GIF`; JUCE has no BMP decoder, so convert BMP to PNG).
- The sheet is exactly **8 cells wide**. Rows are arbitrary; every row is an 8-frame animation. Cell size is arbitrary but must be uniform across the sheet.
- A same-named `.txt` next to the image lists row names, one per line, in row order. Matching is case-insensitive and tolerates a UTF-8 BOM.
- Row conventions:
  - `Held` — shown while the character is dragged (wall-clock loop).
  - `Waiting` — **this project's extension**. Idle loop whenever the transport is not playing. It is excluded from auto-rotation and from the dance-row menu. A sheet without it simply freezes when idle.
  - Every other row — a dance step, cycled every two bars while playing.
- Playback basis: **8 frames per bar** (one frame per half beat). The frame position is derived from the transport, so it cannot drift.
- Scan roots: `%APPDATA%\DanceOverlay\Characters\` plus any imported folders. Importing FL Studio's `...\Plugins\Fruity\Generators\Fruity Dance\Artwork` reads your own sheets in place; nothing is copied.

`tools\make-demo-sheet.ps1` generates a copyright-free `Demo.png` + `Demo.txt` (Demo / Waiting / Held).

## UDP protocol

Loopback only, `127.0.0.1:<port>` (default **53711**, configurable in Settings), plain text:

```
DTB1;<valid>;<playing>;<bpm>;<ppq>;<barStartPpq>;<timestampMs>
```

Example: `DTB1;1;1;92.000000;156.375000;156.000000;1843200`

The overlay accepts packets only from `127.0.0.1`. To test without a DAW, use `tools\send-beats.ps1 -bpm 120 -seconds 8 [-port 53711]`, or listen with `nc -u -l 53711`.

## Architecture

```
Shared/BeatClock.h              TransportSnapshot / BeatState / phase derivation (plain POD)
Shared/BeatProtocol.h           UDP text frame encode/decode + port/timeout defaults
Shared/AppConfig.h              Cross-process config: settings.xml reads, language file
Shared/Choreography.h           Dance pattern table (procedural character)
Shared/DancerRenderer.h         Procedural character renderer
Shared/L10n.h                   UI strings in zh/en/ja, plus the UTF-8 literal helper
Source/PluginProcessor.*        Passthrough processor + sender thread (no network on audio thread)
Source/PluginEditor.*           Transport status panel + language switch
Overlay/OverlaySession.*        Shared services: UDP, loopback, beat arbitration, tray, settings, language
Overlay/DancerWindow.*          One character per window: artwork, drag/resize, its own menu
Overlay/DancerState.h           Per-character state + JSON (de)serialisation
Overlay/SettingsWindow.*        Settings panel
Overlay/CharacterManager.h      Sprite sheet discovery/parsing
Overlay/CharacterRenderer.h     Renderer interface; procedural + sprite implementations
Overlay/TrayIcon.h              Tray entry point
Overlay/LoopbackListener.*      Onset detection + BPM estimation
Overlay/WasapiLoopbackCapture.* Native WASAPI loopback capture (all COM on an MTA thread)
installer/                      NSIS installer script and build script
tools/                          Toolchain installer, deploy script, probes, beat injector, demo sheet
```

Beat math: `beatPhase = ppq mod 1`; bounce height `|sin(pi * phase)|`; sprite frame `(beatInBar + beatPhase) * 2 * speed`.

Hit testing: a Windows layered window receives the mouse per pixel, so any pixel with alpha 0 belongs to whatever is underneath. Each character window is therefore cropped to the union bounding box of its artwork, and the empty pixels inside that box are lifted to alpha 1 (invisible) so the whole box is grabbable.

## Tools

| Script | Purpose |
|---|---|
| `tools/install-toolchain.cmd` | Install VS2022 Build Tools + CMake via Chocolatey |
| `tools/deploy-vst3.cmd` | Copy the built VST3 bundle to the system VST3 folder (run elevated) |
| `tools/send-beats.ps1` | Inject synthetic beats (`-bpm`, `-seconds`, `-port`) |
| `tools/make-demo-sheet.ps1` | Generate a copyright-free demo sprite sheet |
| `tools/build-probe.cmd` | Build the standalone WASAPI probe (`tools/wasapi-probe.exe`) |
| `tools/probe-and-grab.ps1` | Locate the overlay window by process and screenshot it |
| `tools/grab-corner.ps1` | Screenshot the bottom-right corner |

## Troubleshooting

- **The character never moves.** Check `%APPDATA%\DanceOverlay\overlay.log` for `[Beat] active source -> VST` or `[WASAPI] capturing ...`. Confirm the beat source is not `VST only` or `Off`.
- **No host transport.** Some hosts hide the playhead from effect plugins. The panel says so; switch to system-audio mode.
- **System-audio mode is unresponsive.** BPM estimation needs a few beats, and silence longer than 2.2 seconds returns to idle. Make sure the source is not `VST only`.
- **`Initialize(loopback) failed: HRESULT 0x800706CC`.** This is `RPC_S_DUPLICATE_ENDPOINT`: the Windows audio endpoint registration is stuck. It is not a plugin bug — even a plain shared-mode stream fails. Run as administrator:

  ```
  Restart-Service Audiosrv
  ```

  Reboot if that does not help, then re-select the beat source. Build `tools\wasapi-probe.exe` with `tools\build-probe.cmd` and run it to reproduce the failure independently.

- **An antivirus asks for microphone access.** WASAPI loopback is implemented as a capture stream, so security suites (Kaspersky, for example) can treat it as recording. Allow `DanceOverlay.exe`, or use `VST only` / `Off`.
- **Build fails with LNK1104.** The overlay is still running and locking its `.exe`: `taskkill /IM DanceOverlay.exe`.

## Copyright and third-party assets

- The procedural character and the demo sprite sheet are generated by code and may be redistributed.
- **Do not redistribute Image-Line's FL-chan or any bundled Fruity Dance artwork.** Importing the FL Artwork folder reads the user's own files in place; no Image-Line asset is ever included in this repository or in build artifacts.
- Fruity Dance and FL Studio are trademarks of Image-Line. This project is an independent homage and is not affiliated with or endorsed by Image-Line.

## License

GPL-3.0. See [LICENSE](LICENSE).

JUCE 8 is dual-licensed under GPLv3 and a commercial license; this project uses the GPLv3 option.
