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

  <summary>開発のきっかけは？</summary>

  ### OSの壁と決定的敗北

  私が愛用する**CachyOS(Linux)**上でAviUtlを運用しようとして失敗したことがきっかけです。
  この失敗は「LinuxとWindowsを往復する」という**非効率的なワークフロー**を続けなければならないことを意味していました。
  
  **AviUtlのためだけにWindows環境を維持し続けること**は私にとって受け入れがたいものでした。

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

  <summary>名称の由来は？</summary>

  Rinaは、「**R**ina **i**s **n**ot **A**viUtl」の略語です。
  このように、略語自身の定義の中にその略語を含んでいる略語を**再帰的頭字語**といいます。
  この名称は、「AviUtlのユーザー体験を継承しつつ、中身は全く別物の現代的なシステムである」ということを示しています。

  尚、私が所属する弓道部の部員に、「リナ」という人がいます（Rinaの前身である「Noa」についても、「ノア」という部員がいます）。

</details>

<details>

  <summary>アイコンの由来は？</summary>

  ![Icon](./assets/icon.svg)
  
  Tatsh & NAOKIによる楽曲「RED ZONE」のアルバムカバーのフォントからインスピレーションを得ています。
  紫は、[鹿児島県立甲南高等学校](https://edunet002.synapse-blog.jp/konan)のブランドカラーです......が、特に思い入れがある訳ではないです。
  AviUtlのアイコンからは一切影響を受けておりません。

</details>

<details>

  <summary>なぜAviUtlがあるのにAviUtlのクローンを開発しているのですか？</summary>
  
  Rinaは「AviUtlの再発明」ではありません。AviUtlを強く意識していますが、その中身は全く異なります。

  | 鳥瞰図 | **Rina** | **AviUtl1** | **ExEdit2** |
  | --- | --- | --- | --- |
  | **基盤技術** | Qt Quick/Vulkan | Win32 API | Win32 API |
  | **並列処理モデル** | データ駆動型並列処理 | シングルスレッド | マルチスレッド |
  | **メモリ空間** | 64bit | 32bit（最大4GB/非効率的な共有メモリ退避） | 64bit |
  | **プレビュー描画** | Vulkan | GDI | Direct3D |
  | **音声エンジン** | Carla（VST, VST3, LADSPA, LV2等対応） | 貧弱 | 貧弱 |
  | **プラグイン方式** | LuaJIT/C++/QML/GLSL | Lua/C++ | LuaJIT/C++ |
  | **対応可能OS** | Linux, Windows, macOS等 | Windows | Windows |

  RinaはExEdit2も解決できない構造的弱点を根本的に覆します。

  1. ECSがもたらす「データ指向」の革命

      1. 従来のオブジェクト指向的なマルチスレッド（ExEdit2等）では、各クリップを「オブジェクト」として扱い、スレッドに割り振ります。

        - 各クリップ（オブジェクト）がメモリ上のあちこちに散らばっているため、CPUがデータを読み込むたびに待ち時間（レイテンシ）が発生します。

        - 複数のスレッドが同じオブジェクトを触らないよう「ロック（Mutex）」をかける必要があり、これが並列化の足を引っ張ります。

        - 特定の重いフィルタ処理が終わるまで他のスレッドが待たされる「不均衡」が起きやすいです。

      2. RinaのECSは、データを「コンポーネント」という単位で、メモリ上に 「隙間なく、連続して」 配置します。

        - CPUは隣り合ったデータをまとめて読み込む性質があるため、連続したメモリ配置は処理速度を劇的に向上させます。

        - 「座標計算システム」「描画システム」などが、全エンティティの特定のコンポーネントを一気に計算します。これは「全校生徒一人ひとりに指示を出す（OOP）」のではなく、「一斉に前へならえをさせる（ECS）」ような効率性です。
  
  2. メモリ管理の「近代化」と「一貫性」

      1. AviUtlは歴史が長いため、プラグインごとにメモリ管理の手法がバラバラで、これが「原因不明のクラッシュ」の温床になっていました。

      2. RinaではC++23のスマートポインタ（RAII）とアロケータ最適化を全面的に採用しています。

        - メモリリークが構造的に発生しにくく、巨大なプロジェクトを開いても、OSのメモリ管理と協調して動作します。

        - 「編集中に突然落ちて作業が消える」というリスクを、コードレベルで最小化しています。
  
  3. Qt6 / QML による「疎結合なUIアーキテクチャ」

      1. AviUtlのUI（Win32 GDI）は、描画処理とロジックが密結合しており、UIを操作すると描画が止まる、あるいはその逆が頻発します。

      2. RinaではQt Quick(QML)を採用することで、UIのスレッドとレンダリング（Vulkan）のスレッドを完全に分離しています。

        - どれだけ重いレンダリングを実行していても、タイムラインのスムーズなスクロールやボタン操作が妨げられません。

        - High-DPI（4Kモニター等）へのネイティブ対応も完璧であり、スケーリングでUIがぼやけることがありません。
  
  4. クロスプラットフォーム・コア（OSからの自立）

      1. ExEdit2であっても、依然としてWindows固有のAPI（Win32/D3D11）に依存しています。

      2. Rinaではコアロジックが特定のOSに依存しないプラットフォーム非依存設計になっています。

        - Linux上で真のパフォーマンスを発揮できるのはもちろん、別のOSでも、コアを書き換えることなく最大のパフォーマンスを得ることが出来ます。

        - 「特定のOSに縛られ続ける」という制作上のリスクを排除しています。

  他にもRinaが有利な点は多数あります。AviUtlの存在はRinaの存在を否定するには不十分です。

</details>

<details>

  <summary>Linux以外のOS（Windows等）でも動きますか？</summary>

  Rinaのコアはクロスプラットフォーム（C++23/Qt6）で設計されていますが、現在は私のメイン環境であるCachyOS (Linux) を最優先に最適化しています。
  将来的にWindows/macOSへの対応も行う予定ですが、まずはLinux上で最高の体験を提供することを目指しています。

</details>

<details>

  <summary>AviUtlのプラグインやスクリプトはそのまま使えますか？</summary>

  いいえ。仕組みが根本から異なるため互換性はありません。
  しかし、既存のスクリプト資産を参考に、より安全、高速、自由なプラグインシステムを構築中です。

</details>

<details>

  <summary>開発を手伝いたいのですが、何から始めればいいですか？</summary>

  ありがとうございます！
  Issueでのバグ報告や機能提案はもちろん、C++、Qt、GLSLに興味があるコントリビューターを歓迎しています。
  まずは下記のビルド手順を参照し、自身の環境でビルドすることから始めてみてください。

</details>

<details>

  <summary>課題研究が終わった後も開発は続きますか？</summary>

  はい。このプロジェクトは私の「音MAD制作をLinuxで完結させる」という目標から始まっています。
  研究期間終了後も、私自身のメインエディタとして、そしてAviUtlからの救済を待つ方々のために開発を継続します。

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