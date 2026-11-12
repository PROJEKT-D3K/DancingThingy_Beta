# DanceThingy

**一个不依赖 FL Studio 的 Fruity Dance 同类物：任意 DAW 都能用的桌面伴舞小人。**

[English](README.md) · [日本語](README.ja.md)

![Version](https://img.shields.io/badge/version-0.3.0-green)
![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11%20x64-0078D6)
![License](https://img.shields.io/badge/license-GPL--3.0-blue)

DanceThingy 把 Image-Line Fruity Dance 的思路拆成两个彼此独立、又能配合的部件：

- **DanceOverlay** —— 透明、置顶、无背景窗口的桌面悬浮小人，可选点击穿透，浮在任何窗口之上跳舞。
- **DanceThingy（VST3）** —— 挂在任意 DAW 轨道上的直通效果器。它不改动音频，只读取宿主 transport，再通过本机 UDP 把拍子广播给悬浮窗。

## 两种部件，三种用法

| 组合 | 效果 |
|---|---|
| Overlay + VST3 | 动作跟随宿主的拍/小节位置，踩在小节第 1 拍上，且永不漂移。 |
| 只要 Overlay | 通过 WASAPI 回环自听系统输出，用起音检测估速起舞。任意音源都行（DAW、浏览器、播放器），但踩拍是近似值。 |
| 只要 VST3 | 插件面板能看 transport 状态；要跳舞仍需同一台电脑上运行 Overlay。 |

## 功能

- Win32 分层窗口逐像素 alpha，真正透明，没有背景方块。
- 置顶显示；可选点击穿透（透明像素区也会让点击透过）。
- 拖动移动；按住 **Shift** 拖拽可连续缩放，宽高比自动锁定。
- 四档节拍源：**自动（VST 优先）**、**仅 VST（UDP）**、**仅系统音源**、**关闭（待机）**。
- 原生 WASAPI 回环采集 + 起音检测 + BPM 估计；默认跟随默认播放设备，默认端点拒绝回环时自动扫描其它 active render 端点。
- 速度 **×0.25–×4** 九档，另有 **±0.05** 微调；调速时帧位就地重基准，画面不跳帧。
- 角色大小 **50–300%** 七档；Shift 拖拽可在 **25–400%** 之间连续缩放。
- 兼容 Fruity Dance 精灵表：一张图、固定 8 列、任意行数，配同名 `.txt` 列出每行名称。
- 内置程序小人，不需要任何素材文件。
- 可原地导入 FL Studio 的 Artwork 文件夹（零拷贝）、单独添加精灵表，或把图直接拖到小人身上。
- 设置持久化到 `%APPDATA%\DanceOverlay\settings.xml`；日志在 `%APPDATA%\DanceOverlay\overlay.log`。
- 托盘图标、水平镜像、帧间混合、舞步行锁定。

## 当前状态

- **v0.3.0** —— 功能完整的原型，仅支持 Windows。
- 菜单点击、Shift 缩放手感、调速顺滑度等运行时体验仍待人工验收。
- 系统音源模式是启发式的：需要几拍热身，静音约 2.2 秒后回到待机。
- 部分宿主不对效果器暴露 transport 信息，这类宿主请改用系统音源模式。
- 已知 Windows 音频服务故障：回环初始化报 `0x800706CC`（`RPC_S_DUPLICATE_ENDPOINT`）时，以管理员身份重启音频服务；见[故障排查](#故障排查)。

## 环境要求

- Windows 10/11 x64
- Visual Studio 2022 Build Tools，含 C++ 工作负载
- CMake ≥ 3.22
- Git（首次 configure 时 FetchContent 会拉取 JUCE 8.0.4）

一键补齐工具链（管理员 PowerShell/CMD）：

```cmd
choco install visualstudio2022buildtools --packageParameters "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended" -y
choco install cmake -y
```

`tools\install-toolchain.cmd` 就是这两条命令的脚本。

## 构建

首次 configure 会下载 JUCE（百余 MB），首次构建会全量编译，属正常耗时。

```cmd
git clone https://github.com/<your-account>/DanceThingy.git
cd DanceThingy
cmake --preset default
cmake --build --preset default
```

产物：

- `build\DanceOverlay_artefacts\Release\DanceOverlay.exe` —— 悬浮窗
- `build\DanceThingy_artefacts\Release\VST3\DanceThingy.vst3` —— 插件

如果旧 Overlay 还在运行，先关掉；MSVC 无法覆盖被锁住的 `.exe`：

```cmd
taskkill /IM DanceOverlay.exe
```

## 安装

1. 把 `DanceThingy.vst3` 拷到 `C:\Program Files\Common Files\VST3\`（需要管理员）。`tools\deploy-vst3.cmd` 会从构建目录完成这一步。
2. `DanceOverlay.exe` 放在任意位置双击运行。下面的安装包会自动完成这一步，并注册卸载项。

### 构建安装包

`installer\build-installer.cmd` 用 NSIS 3.x 生成 `dist\DanceThingy-<version>-Setup.exe`。安装包会同时安装两个部件、注册卸载项，并在系统缺少 VC++ 2015-2022 运行库时静默安装随包携带的运行库。`makensis.exe` 不在 `E:\NSIS` 时，用 `NSIS_HOME` 指定。

```cmd
installer\build-installer.cmd
```

加 `test` 参数可生成用户级沙箱版，安装到 `%TEMP%`，仅用于验证，不用于分发。

## 使用

### Overlay

- 启动后小人待机呼吸。
- 点住小人本体可拖动；右键小人或托盘图标出菜单。
- **节拍源**：
  - `自动（VST 优先）` —— 默认。VST 心跳在 500 ms 内就用宿主拍子，否则落到系统自听，再否则待机。
  - `仅 VST（UDP）` —— 只认宿主，**不回落**；没有心跳就定格待机。
  - `仅系统音源` —— **完全忽略 UDP**。DAW 里挂着插件（哪怕暂停，它也每 15 ms 发一包）不会压住自听。
  - `关闭（待机）` —— 不做任何采集。
- **人物**：内置程序小人、扫描到的每张精灵表、从文件夹导入、添加单个精灵表、打开人物文件夹、刷新列表。
- **舞步**：自动轮换或锁定某一行；动画设置里有速度、水平镜像、帧间混合。
- **角色大小**：50/75/100/125/150/200/300%；Shift 拖拽可在 25–400% 之间连续缩放。
- **点击穿透**：鼠标穿过小人，托盘图标仍是唤回菜单的入口。此状态刻意不持久化，避免把自己锁在窗外。
- **回到屏幕角落** / **退出 Overlay**。

### DAW 侧

在任意轨道挂 DanceThingy（普通效果器），播放工程即可。插件每 ~15 ms 发一个 UDP 包；多个实例都挂着也没关系，悬浮窗只认最新心跳。停止播放后小人定格当前帧。

### 精灵表格式

格式出自 Fruity Dance 官方手册，另加一条本插件约定：

- 一个角色一张图（`PNG`、`JPEG` 或 `GIF`；JUCE 没有 BMP 解码器，BMP 请转存 PNG）。
- 图固定 **8 格宽**，行数任意；每行是一个 8 帧序列。格子尺寸任意，但整表必须统一。
- 图片旁的同名 `.txt` 按行顺序列出每行名称。匹配大小写不敏感，容忍 UTF-8 BOM。
- 行名约定：
  - `Held` —— 点住拖动小人时显示的姿势（墙钟循环）。
  - `Waiting` —— **本插件的扩展**。未播放时的待机循环；播放时自动轮换与舞步菜单都会跳过它。没有这一行时，播放之外定格当前帧。
  - 其余行 —— 播放时的舞步，按 2 小节边界轮换。
- 播放基准：**8 帧 / 小节**（每半拍一帧）。帧位由 transport 推导，因此不会漂移。
- 扫描根目录：`%APPDATA%\DanceOverlay\Characters\`，以及任何已导入的文件夹。导入 FL Studio 的 `...\Plugins\Fruity\Generators\Fruity Dance\Artwork` 属于就地读取你自己的素材，不做任何拷贝。

`tools\make-demo-sheet.ps1` 可生成无版权风险的 `Demo.png` + `Demo.txt`（Demo / Waiting / Held）。

## UDP 协议

仅本机回环，`127.0.0.1:53711`，纯文本帧：

```
DTB1;<valid>;<playing>;<bpm>;<ppq>;<barStartPpq>;<timestampMs>
```

例：`DTB1;1;1;92.000000;156.375000;156.000000;1843200`

悬浮窗只接受来自 `127.0.0.1` 的包。没有 DAW 时可用 `tools\send-beats.ps1 -bpm 120 -seconds 8` 灌拍，或用 `nc -u -l 53711` 观察。

## 架构速记

```
Shared/BeatClock.h              TransportSnapshot / BeatState / 拍相位推导（纯 POD）
Shared/BeatProtocol.h           UDP 文本帧编解码 + 端口/超时常量
Shared/Choreography.h           程序小人的舞步编排表
Shared/DancerRenderer.h         程序小人渲染
Shared/L10n.h                   JUCE 字符串的 UTF-8 字面量助手
Source/PluginProcessor.*        直通处理器 + 发送线程（音频线程零网络访问）
Source/PluginEditor.*           transport 状态面板
Overlay/OverlayWindow.*         JUCE 组件 + Win32 分层窗口 + 菜单/拖放
Overlay/CharacterManager.h      精灵表扫描/解析
Overlay/CharacterRenderer.h     渲染接口：程序小人 + 精灵表实现
Overlay/TrayIcon.h              托盘入口
Overlay/LoopbackListener.*      起音检测 + BPM 估计
Overlay/WasapiLoopbackCapture.* 原生 WASAPI 回环采集（全部 COM 调用在 MTA 线程）
installer/                      NSIS 安装脚本与构建脚本
tools/                          工具链安装、部署、探针、灌拍、示例精灵表
```

踩拍核心：`beatPhase = ppq mod 1`；弹跳高度 `|sin(pi * phase)|`；精灵表取帧位 `(beatInBar + beatPhase) * 2 * speed`。

## 工具脚本

| 脚本 | 用途 |
|---|---|
| `tools/install-toolchain.cmd` | 通过 Chocolatey 安装 VS2022 Build Tools + CMake |
| `tools/deploy-vst3.cmd` | 把构建出的 VST3 拷到系统 VST3 目录（需管理员） |
| `tools/send-beats.ps1` | 向 `127.0.0.1:53711` 注入合成节拍 |
| `tools/make-demo-sheet.ps1` | 生成无版权风险的示例精灵表 |
| `tools/build-probe.cmd` | 构建独立 WASAPI 探针（输出 `tools/wasapi-probe.exe`） |
| `tools/probe-and-grab.ps1` | 按进程定位 Overlay 窗口并截屏 |
| `tools/grab-corner.ps1` | 截取屏幕右下角 |

## 故障排查

- **小人一直不动。** 看 `%APPDATA%\DanceOverlay\overlay.log` 里有没有 `[Beat] active source -> VST` 或 `[WASAPI] capturing ...`；确认节拍源不是 `仅 VST` 或 `关闭`。
- **读不到宿主 transport。** 部分宿主对效果器隐藏 playhead；面板会显示提示，改用系统音源模式即可。
- **系统音源模式没反应。** BPM 估计需要几拍热身；静音超过 2.2 秒会回到待机。先确认节拍源不是 `仅 VST`。
- **`Initialize(loopback) failed: HRESULT 0x800706CC`。** 这是 `RPC_S_DUPLICATE_ENDPOINT`，Windows 音频端点注册卡住了，**不是插件 bug**——连不带回环的普通共享流都会失败。以管理员身份执行：

  ```
  Restart-Service Audiosrv
  ```

  无效再重启系统，然后重新选择节拍源。用 `tools\build-probe.cmd` 构建 `tools\wasapi-probe.exe` 后可独立复现。

- **构建报 LNK1104。** Overlay 还在运行并锁住了 `.exe`：`taskkill /IM DanceOverlay.exe`。

## 版权红线

- 程序小人与示例精灵表都由代码生成，可分发。
- **不要分发 Image-Line 的 FL-chan 及任何 FL 自带精灵表。** 导入 FL Artwork 属于就地读取用户自己的资产；本仓库与构建产物中永远不包含 Image-Line 素材。
- Fruity Dance 与 FL Studio 是 Image-Line 的商标。本项目是独立致敬之作，与 Image-Line 无隶属或背书关系。

## 许可证

GPL-3.0，见 [LICENSE](LICENSE)。

JUCE 8 采用 GPLv3 与商业授权双许可；本项目使用 GPLv3 选项。
