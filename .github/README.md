<div align="center">
    <img src="../assets/splash.svg" alt="" width="256"/>
    <h1>Rina is not AviUtl</h1>
    <p>AviUtlを踏襲し凌駕する次世代動画編集ソフト</p>
</div>

> [!IMPORTANT]
> ## このリポジトリは [Codeberg](https://codeberg.org/taisho-guy/Rina) のミラーです。
> 開発はCodebergで活発に行われており、**イシューの報告やプルリクエストの送信はすべてCodebergでのみ受け付けています**。
> フィードバックや貢献を検討されている方は、ぜひ Codeberg のリポジトリをご利用ください。

## Rinaとは

Rinaは、AviUtl 1.10 & ExEdit 0.92の操作感を踏襲しつつ、AviUtlを凌駕する性能を持つ動画編集ソフトを実装するプロジェクトです。

**詳細は[Rinaのお部屋](https://rina.taisho-guy.org)をご確認ください。**

## フィードバック・バグの報告

[イシューを作成](https://codeberg.org/taisho-guy/Rina/issues/new)して下さい。

些細なことでも、開発に大きく役立ちます。
 
## 貢献

本プロジェクトへプルリクエストやその他の貢献を送信した場合、あなたは自分の貢献物を[GNU Affero General Public License Version 3](https://www.gnu.org/licenses/agpl-3.0.txt)の下で本プロジェクトに提供することに同意したものとみなします。あなたの貢献物の著作権はあなたに残りますが、受理されたコードは[GNU Affero General Public License Version 3](https://www.gnu.org/licenses/agpl-3.0.txt)の条件に従って利用・改変・再配布されます。
 
- Rinaをフォークし、フォーク済みのリポジトリを`clone`します。
- `cd Rina`
- `git checkout -b fix/some-change`
- 変更を行います。
- `git add .`
- `git commit -m "変更内容"`
- `git push -u origin fix/some-change`
- [Codeberg](https://codeberg.org/taisho-guy/Rina/pulls)でプルリクエストを作成します。

## ビルド手順

Linuxユーザーであれば`BUILD.py`1つで簡単にビルドできます。

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

> [!IMPORTANT]
> - Windows、macOS上でのビルド検証は行っておりません。
> - Distrobox内でビルドを行います。ホストの環境でビルドしたい場合は`BUILD.py`はご利用になれません。

## ライセンス

Rinaは[GNU Affero General Public License Version 3](https://www.gnu.org/licenses/agpl-3.0.txt)に基づいて公開されています。