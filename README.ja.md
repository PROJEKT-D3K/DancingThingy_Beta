# DanceThingy

**FL Studio 専用ではない、どの DAW でも使える Fruity Dance 風デスクトップコンパニオン。**

[English](README.md) · [中文](README.zh-CN.md)

![Version](https://img.shields.io/badge/version-0.3.0-green)
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
- 常時最前面。クリック透過は任意（透明ピクセル部分もクリックを通過させます）。
- ドラッグで移動。**Shift** を押しながらドラッグすると、縦横比を固定したまま連続リサイズ。
- ビートソースは 4 種類：**自動（VST 優先）**、**VST のみ（UDP）**、**システム音声のみ**、**オフ（待機）**。
- ネイティブ WASAPI ループバック + オンセット検出 + BPM 推定。既定の再生デバイスに追従し、ループバックを拒否された場合はほかのアクティブな render エンドポイントを自動で走査。
- 速度 **×0.25–×4** の 9 段階に加え、**±0.05** の微調整。速度変更時はフレーム位相をその場で基準化し直すため、映像が飛びません。
- キャラクターサイズ **50–300%** の 7 段階、または Shift ドラッグで **25–400%** まで連続変更。
- Fruity Dance 互換のスプライトシート：画像 1 枚、**幅は必ず 8 セル**、行数は任意。同名の `.txt` に行名を列挙。
- 素材ファイル不要の内蔵プロシージャルキャラクター。
- FL Studio の Artwork フォルダをその場でインポート（コピーなし）、単体のシート追加、画像をキャラクターへドロップして追加。
- 設定は `%APPDATA%\DanceOverlay\settings.xml`、ログは `%APPDATA%\DanceOverlay\overlay.log`。
- トレイアイコン、左右ミラー、フレーム間ブレンド、ダンス行のロック。

## 現状

- **v0.3.0** — 機能は一通り揃ったプロトタイプ。Windows 専用。
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
2. `DanceOverlay.exe` は任意の場所に置いて起動するだけ。インストーラはなく、JUCE の設定ファイル以外にレジストリも書きません。

## 使い方

### Overlay

- 起動直後は待機してその場で呼吸します。
- キャラクターをドラッグすると移動。右クリック（キャラクターまたはトレイアイコン）でメニュー。
- **ビートソース**：
  - `自動（VST 優先）` — 既定。500 ms 以内の VST ハートビートがあればホストの拍を使用。なければシステム音声、それもなければ待機。
  - `VST のみ（UDP）` — フォールバックしません。ハートビートがなければ待機のまま。
  - `システム音声のみ` — UDP を完全に無視。DAW にプラグインを挿したまま（停止中でも 15 ms ごとにパケットを送ります）システム音声が隠されません。
  - `オフ（待機）` — キャプチャを一切行いません。
- **キャラクター**：内蔵プロシージャル、検出された各スプライトシート、フォルダインポート、単体シート追加、キャラクターフォルダを開く、再スキャン。
- **ダンス**：行の自動ローテーション、または 1 行に固定。アニメーション設定（速度、ミラー、ブレンド）。
- **キャラクターサイズ**：50/75/100/125/150/200/300%。Shift ドラッグで 25–400% まで連続変更。
- **クリック透過**：マウスイベントがキャラクターを通過します。戻す手段はトレイアイコン。締め出されないよう、この状態は意図的に保存しません。
- **画面隅へ戻す** / **終了**。

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

ループバックのみ、`127.0.0.1:53711`、プレーンテキスト：

```
DTB1;<valid>;<playing>;<bpm>;<ppq>;<barStartPpq>;<timestampMs>
```

例：`DTB1;1;1;92.000000;156.375000;156.000000;1843200`

オーバーレイは `127.0.0.1` からのパケットだけを受け付けます。DAW なしで試すには `tools\send-beats.ps1 -bpm 120 -seconds 8`、監視には `nc -u -l 53711` を使えます。

## アーキテクチャ

```
Shared/BeatClock.h              TransportSnapshot / BeatState / 拍位相の導出（純粋な POD）
Shared/BeatProtocol.h           UDP テキストフレームのエンコード/デコード + ポート・タイムアウト定数
Shared/Choreography.h           プロシージャルキャラクターのダンスパターン表
Shared/DancerRenderer.h         プロシージャルキャラクターの描画
Shared/L10n.h                   JUCE 文字列向け UTF-8 リテラルヘルパー
Source/PluginProcessor.*        パススルー処理 + 送信スレッド（オーディオスレッドはネットワークに触れない）
Source/PluginEditor.*           トランスポート状態パネル
Overlay/OverlayWindow.*         JUCE コンポーネント + Win32 レイヤードウィンドウ + メニュー/ドラッグ/ドロップ
Overlay/CharacterManager.h      スプライトシートの検出・解析
Overlay/CharacterRenderer.h     描画インターフェース（プロシージャル + スプライト）
Overlay/TrayIcon.h              トレイ入口
Overlay/LoopbackListener.*      オンセット検出 + BPM 推定
Overlay/WasapiLoopbackCapture.* ネイティブ WASAPI ループバック（COM はすべて MTA スレッド上）
tools/                          ツールチェーン導入、デプロイ、プローブ、拍注入、デモシート
```

拍の計算：`beatPhase = ppq mod 1`、バウンス高 `|sin(pi * phase)|`、スプライトのフレーム `(beatInBar + beatPhase) * 2 * speed`。

## ツール

| スクリプト | 用途 |
|---|---|
| `tools/install-toolchain.cmd` | Chocolatey で VS2022 Build Tools + CMake を導入 |
| `tools/deploy-vst3.cmd` | ビルドした VST3 をシステムの VST3 フォルダへコピー（管理者権限） |
| `tools/send-beats.ps1` | `127.0.0.1:53711` へ合成拍を注入 |
| `tools/make-demo-sheet.ps1` | 著作権フリーのデモスプライトシートを生成 |
| `tools/wasapi-probe.exe` | 共有/ループバックストリームを単体検証する WASAPI プローブ |
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

  改善しなければ再起動し、その後ビートソースを選び直してください。`tools\wasapi-probe.exe` で独立に再現できます。

- **ビルドが LNK1104 で失敗する。** Overlay が起動中で `.exe` をロックしています。`taskkill /IM DanceOverlay.exe` を実行してください。

## 著作権と第三者素材

- プロシージャルキャラクターとデモスプライトシートはコードで生成しており、再配布できます。
- **Image-Line の FL-chan や Fruity Dance 同梱素材を再配布しないでください。** FL Artwork フォルダのインポートはユーザー自身のファイルをその場で読むだけで、本リポジトリやビルド成果物に Image-Line の素材は一切含まれません。
- Fruity Dance および FL Studio は Image-Line の商標です。本プロジェクトは独立したオマージュであり、Image-Line との提携や承認関係はありません。

## ライセンス

GPL-3.0。[LICENSE](LICENSE) を参照してください。

JUCE 8 は GPLv3 と商用ライセンスのデュアルライセンスです。本プロジェクトは GPLv3 の選択肢を使用します。
