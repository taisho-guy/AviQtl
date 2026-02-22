# Rina - Rina is not AviUtl.

RinaはAviUtl 1.10（ExEdit 0.92）の代替を目指すプロジェクトです。

### [Rinaのお部屋](https://taisho-guy.codeberg.page/Rina) ・ [Wiki](https://codeberg.org/taisho-guy/Rina/wiki)

## 他の動画編集ソフトとの違い

| 項目 | [Rina](https://codeberg.org/taisho-guy/Rina/) | [ExEdit 1](https://spring-fragrance.mints.ne.jp/aviutl/) | [ExEdit 2](https://spring-fragrance.mints.ne.jp/aviutl/) | [Beutl](https://beutl.beditor.net/) | [YMM4](https://manjubox.net/ymm4/) | [DaVinci Resolve 無料版](https://www.blackmagicdesign.com/products/davinciresolve/) | [CapCut 無料版](https://www.capcut.com/) |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **OS** | Win/Mac/Linux | Windows | Windows | Win/Mac/Linux | Windows | Win/Mac/Linux | Win/Mac/Mobile |
| **ライセンス** | AGPL v3 | 不自由 | 不自由 | GPL v3 | 不自由 | 不自由 | 不自由 |
| **GPU** | 対応 (Vulkan) | 非対応 | 一部対応 | 対応 (OpenGL) | 対応 | 対応 | 対応 |
| **CPU** | 64bit | 32bit | 64bit | 64bit | 64bit | 64bit | 64bit |
| **UI** | レイヤー | レイヤー | レイヤー | レイヤー | タイムライン | ノード/タイムライン | トラック |
| **拡張** | 対応 | 対応 | 対応 | 対応 | 対応 | 制限 | 非対応 |
| **開発言語** | C++/QML | C++ | C++ | C# | C# | C++ | C++ |
| **速度** | 高速 | 低速 | 普通 | まぁ高速 | 普通 | 低速 | まぁ高速 |
| **動画の商用利用** | OK | OK | OK | OK | 制限 | OK | 制限 |

## ダウンロード

### [Linux(x86_64) / Windows(x86_64) / macOS(ARM64)](https://codeberg.org/taisho-guy/Rina/releases)

> [!NOTE]
> - Linux版は依存関係を同梱しておりません。Qt6, LuaJIT, Vulkan, FFmpegをインストールしてください。
> - Arch Linux上でビルドしているため、最新のランタイムが利用できないディストリビューション（Ubuntu等）では動作しない場合がございます。ローリングリリースのディストリビューション（CachyOS等）でご利用頂くことを推奨致します。

## ビルド手順(CachyOS推奨)

- 依存関係をインストールします

```
paru -S --needed fish git cmake ninja qt6 lua-jit vulkan-devel base-devel mold p7zip fftw wayland-protocols libffi ladspa lv2 lilv clap vst3sdk
```

- リポジトリをクローンします

```
git clone https://codeberg.org/taisho-guy/Rina.git
```

- ビルドします

```
cd Rina
python BUILD.py
```

- 実行します

```
./build/Rina
```

## サードパーティー製の埋め込みリソース

- [Remix Icon](https://github.com/Remix-Design/RemixIcon) (MIT License)

## ライセンス

Rinaは[GNU AFFERO GENERAL PUBLIC LICENSE Version 3](https://www.gnu.org/licenses/agpl-3.0.txt)に基づいて公開されています。