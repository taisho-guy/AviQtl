<div align="center">
    <img src="./assets/icon.svg" alt="" width="128"/>
    <h1>Rina is not AviUtl</h1>
    <p>AviUtl以上のユーザー体験をExEdit0.xライクに実装するプロジェクト</p>
    <p>
        <a href="https://taisho-guy.codeberg.page/Rina"><img src="https://img.shields.io/badge/Home-Rina%E3%81%AE%E3%81%8A%E9%83%A8%E5%B1%8B-609926?style=flat-square" alt="Home"></a>
        <a href="https://codeberg.org/taisho-guy/Rina/wiki"><img src="https://img.shields.io/badge/Docs-Wiki-3e4348?style=flat-square" alt="Wiki"></a>
        <a href="https://codeberg.org/taisho-guy/Rina/releases"><img src="https://img.shields.io/gitea/v/release/taisho-guy/Rina?gitea_url=https%3A%2F%2Fcodeberg.org&style=flat-square&color=2185d0" alt="Release"></a>
        <a href="https://codeberg.org/taisho-guy/Rina/stars"><img src="https://img.shields.io/gitea/stars/taisho-guy/Rina?gitea_url=https%3A%2F%2Fcodeberg.org&style=flat-square&color=609926" alt="Stars"></a>
        <a href="https://codeberg.org/taisho-guy/Rina/issues"><img src="https://img.shields.io/badge/dynamic/json?url=https%3A%2F%2Fcodeberg.org%2Fapi%2Fv1%2Frepos%2Ftaisho-guy%2FRina&query=%24.open_issues_count&label=issues&style=flat-square&color=f2711c" alt="Issues"></a>
    </p>
</div>

## ダウンロード

- [Linux(x86_64)](https://codeberg.org/taisho-guy/Rina/releases)

  - 依存関係を同梱しておりません。Qt6全般、LuaJIT、Vulkan実装（Mesa等）、FFmpeg、Carlaをインストールしてからご利用ください。
  - 最新のランタイムが利用できないディストリビューションでは動作しない場合がございます。
  - [CachyOS](https://cachyos.org/)でのご利用を推奨しております。
  - Windows版/macOS版は現在提供しておりません。手動でビルドして下さい。

## Q & A

<details>

  <summary>開発経緯</summary>

  ### OSの壁と決定的敗北

  私が愛用する**CachyOS(Linux)**上でAviUtlを運用しようとして失敗したことがきっかけです。
  この失敗は「LinuxとWindowsを往復する」という**非効率的なワークフロー**を続けなければならないことを意味していました。
  
  **AviUtlのためだけにWindows環境を維持し続けなけること**は私にとって受け入れがたいものでした。

  ### 肥大化したエコシステム

  理由は違えど、AviUtlを「仕方なく」使い続けている方は少なくないはずです。
  長年の拡張によって肥大化した「ハウルの動く城」のようなエコシステムは、不満を抱えながらも手放すことができない存在となっています。ExEdit2が出た現在もAviUtl1.10が主流である理由も、この呪縛にあると考えています。

  ### プロジェクトの目標とミッション

  私は、[鹿児島県立甲南高等学校](https://edunet002.synapse-blog.jp/konan)の課題研究において、この現状を打破すべくAviUtlクローンの独自開発を決意しました。
  
  #### 個人的な目標

  Domino、VocalShifter、REAPER、AviUtlをはしごすることなく、Linux上のRinaのみで音MADを制作すること。
  
  #### Rinaのミッション

  AviUtlを「仕方なく」使っている人々を**全員**救済すること。

</details>

<details>

  <summary>名称の由来</summary>

  Rinaは、「**R**ina **i**s **n**ot **A**viUtl」の略語です。
  このように、略語自身の定義の中にその略語を含んでいる略語を**再帰的頭字語**といいます。
  この名称は、「AviUtlのユーザー体験を継承しつつ、中身は全く別物の現代的なシステムである」ということを示しています。

  尚、私が所属する弓道部の部員に、「リナ」という人がいます（Rinaの前身である「Noa」についても、「ノア」という部員がいます）。

</details>

<details>

  <summary>アイコンの由来</summary>

  ![Icon](./assets/icon.svg)
  
  Tatsh & NAOKIによる楽曲「RED ZONE」のアルバムカバーのフォントからインスピレーションを得ています。
  紫は、[鹿児島県立甲南高等学校](https://edunet002.synapse-blog.jp/konan)のブランドカラーです......が、特に思い入れがある訳ではないです。
  AviUtlのアイコンからは一切影響を受けておりません。

</details>

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

## サードパーティー製の埋め込みリソース

- [Remix Icon](https://github.com/Remix-Design/RemixIcon) ([Remix Icon License v1.0](https://raw.githubusercontent.com/Remix-Design/RemixIcon/refs/heads/master/License))

## ライセンス

Rinaは[GNU AFFERO GENERAL PUBLIC LICENSE Version 3](https://www.gnu.org/licenses/agpl-3.0.txt)に基づいて公開されています。