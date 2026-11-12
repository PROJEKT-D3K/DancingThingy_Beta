# DanceThingy

**FL Studio 専用ではない、どの DAW でも使える Fruity Dance 風デスクトップコンパニオン。**

[English](README.md) · [中文](README.zh-CN.md)

![Version](https://img.shields.io/badge/version-0.4.0-green)
![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11%20x64-0078D6)
![License](https://img.shields.io/badge/license-GPL--3.0-blue)

DanceThingy は Image-Line の Fruity Dance のアイデアを、独立しながら連携する 2 つの部品として再構成したものです。

- **DanceOverlay** — 背景ウィンドウを持たない、透過・常時最前面のデスクトップキャラクター。クリック透過は任意。ほかのすべてのウィンドウの上で踊ります。
- **DanceThingy (VST3)** — どの DAW のトラックにも挿せるパススルー・エフェクト。音声には一切触れず、ホストのトランスポートを読んで、ローカル UDP で拍をオーバーレイへ送ります。

## 2 つの部品、3 つの使い方

| 構成 | 得られるもの |
|---|---|
| Overlay + VST3 | ホストの小節・拍位置に追従するため、動きが小節の頭に合い、ズレが蓄積しません。 |
| Overlay のみ | WASAPI ループバックでシステム出力を聴き、オンセット検出からテンポを推定。DAW・ブラウザ・メディアプレーヤーなど任意の音源で動きますが、タイミングは近似です。 |
| VST3 のみ | プラグイン画面でトランスポート状態を確認できます。踊らせるには同じ PC 上で Overlay が必要です。 |

## 機能

- Win32 レイヤードウィンドウのピクセル単位アルファで、完全透過。背景の矩形は出ません。
- **複数キャラクターを同時に表示**できます。それぞれにシート・ダンス行・速度・サイズ・ミラー/ブレンド・位置を個別に指定でき、同じシートを 2 体に使うこともできます。
- 常時最前面。クリック透過は任意（透明ピクセル部分もクリックを通過させます）。
- ドラッグで移動。**Shift** を押しながらドラッグすると、縦横比を固定したまま連続リサイズ。
- 操作範囲は素材のバウンディングボックスに合わせて切り出されるため、キャラクター上のどこでもドラッグ・右クリックできます。**操作範囲を表示**でその矩形を描けます。
- **設定ウィンドウ**（右クリック → `設定…`、または `--settings` 付きで起動）：ビートソース、UDP ポート、VST タイムアウト、再試行回数、リフレッシュレート、キャラクター一覧。
- **日本語 / English / 中文** の 3 言語 UI。設定ウィンドウでも VST エディターでも即時に切り替わり、両プロセスで同じ設定を共有します。
- ビートソースは 4 種類：**自動（VST 優先）**、**VST のみ（UDP）**、**システム音声のみ**、**オフ（待機）**。
- ネイティブ WASAPI ループバック + オンセット検出 + BPM 推定。既定の再生デバイスに追従し、ループバックを拒否された場合はほかのアクティブな render エンドポイントを自動で走査。
- 速度 **×0.25–×4** の 9 段階に加え、**±0.05** の微調整。速度変更時はフレーム位相をその場で基準化し直すため、映像が飛びません。
- キャラクターサイズ **50–300%** の 7 段階、または Shift ドラッグで **25–400%** まで連続変更。
- Fruity Dance 互換のスプライトシート：画像 1 枚、**幅は必ず 8 セル**、行数は任意。同名の `.txt` に行名を列挙。
- 素材ファイル不要の内蔵プロシージャルキャラクター。
- FL Studio の Artwork フォルダをその場でインポート（コピーなし）、単体のシート追加、画像をキャラクターへドロップして追加。
- 設定は `%APPDATA%\DanceOverlay\settings.xml`、UI 言語は `%APPDATA%\DanceOverlay\language.txt`、ログは `%APPDATA%\DanceOverlay\overlay.log`。
- トレイアイコン、左右ミラー、フレーム間ブレンド、ダンス行のロック。

## 現状

- **v0.4.0** — 複数キャラクター、設定ウィンドウ、日中英 UI、UDP ポートの変更に対応。Windows 専用。
- メニュー操作、Shift リサイズの感触、速度変更の滑らかさなど、実行時の UX はまだ人手で受け入れ確認していません。
- システム音声モードはヒューリスティックです。数拍のウォームアップが必要で、約 2.2 秒の無音で待機に戻ります。
- ホストによってはエフェクトにトランスポート情報を公開しません。その場合はシステム音声モードを使ってください。
- Windows オーディオサービスの既知の不具合：ループバック初期化が `0x800706CC`（`RPC_S_DUPLICATE_ENDPOINT`）で失敗する場合、管理者権限でオーディオサービスを再起動してください。[トラブルシューティング](#トラブルシューティング)を参照。

## 動作環境

- Windows 10/11 x64
- Visual Studio 2022 Build Tools（C++ ワークロード）
- CMake ≥ 3.22
- Git（初回 configure 時に FetchContent が JUCE 8.0.4 を取得します）

ツールチェーンをまとめて導入する例（管理者 PowerShell/CMD）：

```cmd
choco install visualstudio2022buildtools --packageParameters "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended" -y
choco install cmake -y
```

`tools\install-toolchain.cmd` はこの 2 コマンドをまとめたスクリプトです。

## ビルド

初回 configure で JUCE（100 MB 超）をダウンロードし、初回ビルドでそれを丸ごとコンパイルします。正常な動作です。

```cmd
git clone https://github.com/<your-account>/DanceThingy.git
cd DanceThingy
cmake --preset default
cmake --build --preset default
```

成果物：

- `build\DanceOverlay_artefacts\Release\DanceOverlay.exe` — オーバーレイ
- `build\DanceThingy_artefacts\Release\VST3\DanceThingy.vst3` — プラグイン

既存の Overlay が起動中なら先に終了してください。MSVC はロックされた `.exe` を上書きできません。

```cmd
taskkill /IM DanceOverlay.exe
```

## インストール

1. `DanceThingy.vst3` を `C:\Program Files\Common Files\VST3\` へコピー（管理者権限）。`tools\deploy-vst3.cmd` がビルドディレクトリから実行します。
2. `DanceOverlay.exe` は任意の場所に置いて起動するだけ。下記のインストーラを使えば、この配置とアンインストール登録を自動化できます。

### インストーラのビルド

`installer\build-installer.cmd` は NSIS 3.x で `dist\DanceThingy-<version>-Setup.exe` を生成します。両方のコンポーネントをインストールし、アンインストーラを登録し、VC++ 2015-2022 ランタイムがない環境では同梱のランタイムをサイレントインストールします。`makensis.exe` が `E:\NSIS` にない場合は `NSIS_HOME` を設定してください。

```cmd
installer\build-installer.cmd
```

`test` を付けると `%TEMP%` 以下に入るユーザーレベルのサンドボックス版を生成します（検証用で、配布用ではありません）。

## 使い方

### Overlay

- 起動直後は待機してその場で呼吸します。
- キャラクターをドラッグすると移動。**Shift** を押しながらドラッグするとリサイズ。キャラクターの右クリックは**そのキャラクター**のメニュー、トレイアイコンは全体メニューです。
- キャラクターのメニュー：シート、ダンス行、アニメーション設定（速度・ミラー・ブレンド）、サイズ、**キャラクターを追加**、**このキャラクターを削除**（最後の 1 体では無効）、**設定…**、クリック透過、最前面に表示、画面隅へ移動、終了。
- **設定…**（または `DanceOverlay.exe --settings`）で設定ウィンドウが開きます：
  - **言語** — 中文 / English / 日本語。即時反映され、VST エディターも追従します。
  - **ビートソース** — 下記の 4 種類。
  - **UDP ポート** — 1024–65535、既定 53711。変更は即座に再バインドされ、プラグインは 1 秒以内に追従します。バインドに失敗した場合は元のポートへ戻し、その旨を表示します。
  - **VST タイムアウト**、**システム音声の再試行回数**、**リフレッシュレート**（30/60/120 Hz）。
  - **操作範囲を表示** — 操作できる矩形を描画します。
  - **キャラクター一覧** — 追加と削除。
- ビートソース：
  - `自動（VST 優先）` — 既定。タイムアウト内の VST ハートビートがあればホストの拍を使用。なければシステム音声、それもなければ待機。
  - `VST のみ（UDP）` — フォールバックしません。ハートビートがなければ待機のまま。
  - `システム音声のみ` — UDP を完全に無視。DAW にプラグインを挿したまま（停止中でも 15 ms ごとにパケットを送ります）システム音声が隠されません。
  - `オフ（待機）` — キャプチャを一切行いません。
- 複数キャラクターは同じトランスポートで踊りますが、行・速度・サイズ・ミラー/ブレンド・位置は個別です。ドラッグ、リサイズ、メニューもキャラクター単位です。
- **クリック透過**：マウスイベントがすべてのキャラクターを通過します。戻す手段はトレイアイコン。締め出されないよう、この状態は意図的に保存しません。

### DAW 側

任意のトラックに DanceThingy を通常のエフェクトとして挿して再生するだけ。プラグインは約 15 ms ごとに UDP パケットを送ります。複数インスタンスでも問題なく、オーバーレイは最新のハートビートを採用します。再生を止めると、キャラクターは現在のフレームで停止します。

### スプライトシート

形式は Fruity Dance の公式マニュアルに従い、本プロジェクト独自の拡張を 1 つ加えています。

- 1 キャラクターにつき画像 1 枚（`PNG`、`JPEG`、`GIF`。JUCE に BMP デコーダはないため、BMP は PNG に変換してください）。
- シートの幅は必ず **8 セル**。行数は任意で、各行が 8 フレームのアニメーション。セルサイズは任意ですが、シート全体で統一されている必要があります。
- 画像と同じ名前の `.txt` に、行順で 1 行 1 名を列挙。照合は大文字小文字を区別せず、UTF-8 BOM も許容します。
- 行名の規約：
  - `Held` — キャラクターをドラッグしている間に表示（実時間ループ）。
  - `Waiting` — **本プロジェクトの拡張**。トランスポートが再生中でないときの待機ループ。自動ローテーションとダンス行メニューからは除外されます。この行がないシートは、再生外では現在のフレームで停止します。
  - それ以外の行 — 再生中のダンスステップ。2 小節ごとに切り替え。
- 再生基準：**1 小節あたり 8 フレーム**（半拍に 1 フレーム）。フレーム位置はトランスポートから導出されるため、ズレが蓄積しません。
- スキャン対象：`%APPDATA%\DanceOverlay\Characters\` と、インポートした各フォルダ。FL Studio の `...\Plugins\Fruity\Generators\Fruity Dance\Artwork` をインポートしても、ユーザー自身の素材をその場で読むだけで、コピーは行いません。

`tools\make-demo-sheet.ps1` で、著作権上の問題がない `Demo.png` + `Demo.txt`（Demo / Waiting / Held）を生成できます。

## UDP プロトコル

ループバックのみ、`127.0.0.1:<ポート>`（既定 **53711**、設定で変更可）、プレーンテキスト：

```
DTB1;<valid>;<playing>;<bpm>;<ppq>;<barStartPpq>;<timestampMs>
```

例：`DTB1;1;1;92.000000;156.375000;156.000000;1843200`

オーバーレイは `127.0.0.1` からのパケットだけを受け付けます。DAW なしで試すには `tools\send-beats.ps1 -bpm 120 -seconds 8 [-port 53711]`、監視には `nc -u -l 53711` を使えます。

## アーキテクチャ

```
Shared/BeatClock.h              TransportSnapshot / BeatState / 拍位相の導出（純粋な POD）
Shared/BeatProtocol.h           UDP テキストフレームのエンコード/デコード + ポート・タイムアウト既定値
Shared/AppConfig.h              プロセス間設定：settings.xml の読み取り、言語ファイルの読み書き
Shared/Choreography.h           プロシージャルキャラクターのダンスパターン表
Shared/DancerRenderer.h         プロシージャルキャラクターの描画
Shared/L10n.h                   日中英の UI 文言テーブル + UTF-8 リテラルヘルパー
Source/PluginProcessor.*        パススルー処理 + 送信スレッド（オーディオスレッドはネットワークに触れない）
Source/PluginEditor.*           トランスポート状態パネル + 言語切り替え
Overlay/OverlaySession.*        共有サービス：UDP、ループバック、拍の調停、トレイ、設定、言語
Overlay/DancerWindow.*          1 キャラクター 1 ウィンドウ：素材、ドラッグ/リサイズ、専用メニュー
Overlay/DancerState.h           キャラクター単位の状態 + JSON シリアライズ
Overlay/SettingsWindow.*        設定パネル
Overlay/CharacterManager.h      スプライトシートの検出・解析
Overlay/CharacterRenderer.h     描画インターフェース（プロシージャル + スプライト）
Overlay/TrayIcon.h              トレイ入口
Overlay/LoopbackListener.*      オンセット検出 + BPM 推定
Overlay/WasapiLoopbackCapture.* ネイティブ WASAPI ループバック（COM はすべて MTA スレッド上）
installer/                      NSIS インストーラのスクリプトとビルドスクリプト
tools/                          ツールチェーン導入、デプロイ、プローブ、拍注入、デモシート
```

拍の計算：`beatPhase = ppq mod 1`、バウンス高 `|sin(pi * phase)|`、スプライトのフレーム `(beatInBar + beatPhase) * 2 * speed`。

ヒットテスト：Windows のレイヤードウィンドウは**ピクセル単位**でマウスを受けるか決めるため、アルファ 0 のピクセルは下のウィンドウへ渡ります。そこで各キャラクターのウィンドウを素材の結合バウンディングボックスに切り出し、その中の空ピクセルのアルファを 1（不可視）に上げて、矩形全体をつかめるようにしています。

## ツール

| スクリプト | 用途 |
|---|---|
| `tools/install-toolchain.cmd` | Chocolatey で VS2022 Build Tools + CMake を導入 |
| `tools/deploy-vst3.cmd` | ビルドした VST3 をシステムの VST3 フォルダへコピー（管理者権限） |
| `tools/send-beats.ps1` | 合成拍を注入（`-bpm`、`-seconds`、`-port`） |
| `tools/make-demo-sheet.ps1` | 著作権フリーのデモスプライトシートを生成 |
| `tools/build-probe.cmd` | 単体 WASAPI プローブをビルド（`tools/wasapi-probe.exe` を出力） |
| `tools/probe-and-grab.ps1` | プロセスから Overlay ウィンドウを特定してスクリーンショット |
| `tools/grab-corner.ps1` | 画面右下をスクリーンショット |

## トラブルシューティング

- **キャラクターが動かない。** `%APPDATA%\DanceOverlay\overlay.log` に `[Beat] active source -> VST` または `[WASAPI] capturing ...` があるか確認。ビートソースが `VST のみ` や `オフ` になっていないかも確認してください。
- **ホストのトランスポートが読めない。** ホストによってはエフェクトに playhead を公開しません。パネルにその旨が出るので、システム音声モードへ切り替えてください。
- **システム音声モードが反応しない。** BPM 推定には数拍のウォームアップが必要で、2.2 秒を超える無音で待機に戻ります。ビートソースが `VST のみ` でないことを確認してください。
- **`Initialize(loopback) failed: HRESULT 0x800706CC`。** `RPC_S_DUPLICATE_ENDPOINT` で、Windows のオーディオエンドポイント登録が固まっています。プラグインの不具合ではありません（ループバックなしの通常の共有モードストリームでも失敗します）。管理者権限で実行してください。

  ```
  Restart-Service Audiosrv
  ```

  改善しなければ再起動し、その後ビートソースを選び直してください。`tools\build-probe.cmd` で `tools\wasapi-probe.exe` をビルドすれば独立に再現できます。

- **セキュリティソフトがマイクへのアクセスを求めてくる。** WASAPI ループバックは内部的にキャプチャストリームなので、Kaspersky などは録音とみなします。`DanceOverlay.exe` を許可するか、`VST のみ` / `オフ` を使ってください。
- **ビルドが LNK1104 で失敗する。** Overlay が起動中で `.exe` をロックしています。`taskkill /IM DanceOverlay.exe` を実行してください。

## 著作権と第三者素材

- プロシージャルキャラクターとデモスプライトシートはコードで生成しており、再配布できます。
- **Image-Line の FL-chan や Fruity Dance 同梱素材を再配布しないでください。** FL Artwork フォルダのインポートはユーザー自身のファイルをその場で読むだけで、本リポジトリやビルド成果物に Image-Line の素材は一切含まれません。
- Fruity Dance および FL Studio は Image-Line の商標です。本プロジェクトは独立したオマージュであり、Image-Line との提携や承認関係はありません。

## ライセンス

GPL-3.0。[LICENSE](LICENSE) を参照してください。

JUCE 8 は GPLv3 と商用ライセンスのデュアルライセンスです。本プロジェクトは GPLv3 の選択肢を使用します。
