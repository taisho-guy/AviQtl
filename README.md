# Rina - Rina is not AviUtl.

### RinaはAviUtl 1.10（ExEdit 0.92）以上のユーザー体験をAviUtlの操作感で実現するプロジェクトです。

- [Rinaのお部屋](https://taisho-guy.codeberg.page/Rina)
- [Wiki](https://codeberg.org/taisho-guy/Rina/wiki)

## ダウンロード

- [Linux(x86_64)](https://codeberg.org/taisho-guy/Rina/releases)

  - Linux版は依存関係を同梱しておりません。Qt6, LuaJIT, Vulkan, FFmpegをインストールしてください。
  - Arch Linux上でビルドしているため、最新のランタイムが利用できないディストリビューションでは動作しない場合がございます。ローリングリリースのディストリビューションでご利用頂くことを推奨致します。

- [Windows(x86_64)](https://codeberg.org/taisho-guy/Rina/releases)

  - Windows版は依存関係を同梱しております。

- macOS版は現在提供しておりません。手動でビルドして下さい。

## ビルド手順 (Linux/MSYS2)

- 依存関係をインストールします

  - Pacman: `sudo pacman -S --needed distrobox podman python pyside6 git`
  - APT: `sudo apt install distrobox podman python3 python3-pyside6 git`
  - DNF: `sudo dnf install distrobox podman python3 python3-pyside6 git`
  - MSYS2: `pacman -S git mingw-w64-ucrt-x86_64-pyside6`

- リポジトリをクローンします

  - `git clone https://codeberg.org/taisho-guy/Rina.git`

- ビルドします

  - `cd Rina`
  - `python BUILD.py`

- 実行します

  - Linux: `./build/Rina`
  - MSYS2: `./build/Rina.exe`



## サードパーティー製の埋め込みリソース

- [Remix Icon](https://github.com/Remix-Design/RemixIcon) ([Remix Icon License v1.0](https://raw.githubusercontent.com/Remix-Design/RemixIcon/refs/heads/master/License))

## ライセンス

Rinaは[GNU AFFERO GENERAL PUBLIC LICENSE Version 3](https://www.gnu.org/licenses/agpl-3.0.txt)に基づいて公開されています。