<p align="center">
  <img src="./assets/splash.svg" width="256">
</p>

<p align="center"><b>AviUtlを踏襲し凌駕する次世代動画編集ソフト</b></p>

## AviQtlとは

AviQtlは、AviUtl 1.10 & ExEdit 0.92の操作感を踏襲しつつ、AviUtlを凌駕する性能を持つ動画編集ソフトを実装するプロジェクトです。

**詳細は[AviQtlのお部屋](https://aviqtl.taisho-guy.org)をご確認ください。**

## フィードバック・バグの報告

[イシューを作成](https://codeberg.org/taisho-guy/AviQtl/issues/new)して下さい。

些細なことでも、開発に大きく役立ちます。
 
## 貢献

本プロジェクトへプルリクエストやその他の貢献を送信した場合、あなたは自分の貢献物を[GNU Affero General Public License](https://www.gnu.org/licenses/agpl-3.0.txt)の下で本プロジェクトに提供することに同意したものとみなします。あなたの貢献物の著作権はあなたに残りますが、受理されたコードは[GNU Affero General Public License](https://www.gnu.org/licenses/agpl-3.0.txt)の条件に従って利用・改変・再配布されます。
 
- AviQtlをフォークし、フォーク済みのリポジトリを`clone`します。
- `cd AviQtl`
- `git checkout -b fix/some-change`
- 変更を行います。
- `git add .`
- `git commit -m "変更内容"`
- `git push -u origin fix/some-change`
- [Codeberg](https://codeberg.org/taisho-guy/AviQtl/pulls)でプルリクエストを作成します。

## ビルド手順

Linuxユーザーであれば`BUILD.py`1つで簡単にビルドできます。

- 依存関係をインストールします

  - Pacman: `sudo pacman -S --needed distrobox podman python pyside6 git`
  - APT: `sudo apt install distrobox podman python3 python3-pyside6 git`
  - DNF: `sudo dnf install distrobox podman python3 python3-pyside6 git`
  - MSYS2: `pacman -S git mingw-w64-ucrt-x86_64-pyside6`

- リポジトリをクローンします

  - `git clone https://codeberg.org/taisho-guy/AviQtl.git`

- ビルドします

  - `cd AviQtl`
  - `python BUILD.py`

- 実行します

  - Linux: `./build/AviQtl`
  - MSYS2: `./build/AviQtl.exe`

> [!IMPORTANT]
> - Windows、macOS上でのビルド検証は行っておりません。
> - Distrobox内でビルドを行います。ホストの環境でビルドしたい場合は`BUILD.py`はご利用になれません。

## ライセンス

AviQtlは[GNU Affero General Public License](https://www.gnu.org/licenses/agpl-3.0.txt)に基づいて公開されています。

AviQtl内で使用されている[Remix Icon](https://remixicon.com/)は[Remix Icon License](https://raw.githubusercontent.com/Remix-Design/RemixIcon/refs/heads/master/License)に基づいて提供されています。