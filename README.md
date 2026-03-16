<div align="center">
    <img src="./assets/icon.svg" alt="" width="192" align="center" />
    <h1 align="center">Rina - Rina is not AviUtl</h1>
</div>

### AviUtl 1.10（ExEdit 0.92）以上のユーザー体験をExEdit 0.xライクな操作感で実装するプロジェクトです。

- [Rinaのお部屋](https://taisho-guy.codeberg.page/Rina)
- [Wiki](https://codeberg.org/taisho-guy/Rina/wiki)

## ダウンロード

- [Linux(x86_64)](https://codeberg.org/taisho-guy/Rina/releases)

  - 依存関係を同梱しておりません。Qt6全般、LuaJIT、Vulkan実装（Mesa等）、FFmpeg、Carlaをインストールしてからご利用ください。
  - 最新のランタイム（glibc等）が利用できないディストリビューションでは動作しない場合がございます。

- Windows版/macOS版は現在提供しておりません。ビルドできるかもしれませんが......

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

> [!WARNING]
> MSYS2やmacOS、Visual Studio上でのビルド検証は行っておりません。
> ビルドが通らない場合はIssueを立てたり、`CMakeLists.txt`や`BUILD.py`を改良してプルリクエストを送信して頂けると有り難いです。

## サードパーティー製の埋め込みリソース

- [Remix Icon](https://github.com/Remix-Design/RemixIcon) ([Remix Icon License v1.0](https://raw.githubusercontent.com/Remix-Design/RemixIcon/refs/heads/master/License))

## ライセンス

Rinaは[GNU AFFERO GENERAL PUBLIC LICENSE Version 3](https://www.gnu.org/licenses/agpl-3.0.txt)に基づいて公開されています。