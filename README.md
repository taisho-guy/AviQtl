<div align="center">
    <img src="./assets/icon.svg" alt="" width="256"/>
    <h1>Rina is not AviUtl</h1>
    <p>AviUtlのまま、AviUtlを超える。安全・高速・自由な動画編集ソフト。</p>
</div>


>[!IMPORTANT]
> ## 私は2026年度、受験生になりますから、更新の頻度が低下します。2027年3月以降に本格的に開発を再開します。

## ダウンロード

- [Linux(x86_64)](https://codeberg.org/taisho-guy/Rina/releases)

> [!WARNING]
> ### ダウンロード前にご確認下さい
> - 現在RinaはLinux(x86_64)のみに対応しております。**Windows/macOSではご利用になれません**。
> - 最新のパッケージが使えないディストリビューション（Ubuntu等）では正しく動作しない可能性がございます。
> - [CachyOS](https://cachyos.org/)でのご利用を推奨しております。

>[!NOTE]
> ### ダウンロード・インストール方法
> - Qt6全般、LuaJIT、Vulkan実装（Mesa等）、FFmpeg、Carlaをインストールします。
> - `Rina-Linux-x86_64-v3.7z`をダウンロードします。
> - ダウンロードしたファイルを展開します。
> - `Rina`に実行権限を付与します。
> - `Rina`を実行します。

>[!TIP]
> ### フィードバック・バグの報告
> - [taisho-guy](https://www.instagram.com/taisho_guy/)宛にメッセージを送信して下さい。
> - 些細なことでも、開発に大きく役立ちます。
> 
> ### 貢献
> - 本プロジェクトへプルリクエストやその他の貢献を送信した場合、あなたは自分の貢献物を[GNU Affero General Public License Version 3](https://www.gnu.org/licenses/agpl-3.0.txt)の下で本プロジェクトに提供することに同意したものとみなします。あなたの貢献物の著作権はあなたに残りますが、受理されたコードは[GNU Affero General Public License Version 3](https://www.gnu.org/licenses/agpl-3.0.txt)の条件に従って利用・改変・再配布されます。
> 
>   - Rinaをフォークし、フォーク済みのリポジトリを`clone`します。
>   - `cd Rina`
>   - `git checkout -b fix/some-change`
>   - 変更を行います。
>   - `git add .`
>   - `git commit -m "変更内容"`
>   - `git push -u origin fix/some-change`
>   - [Codeberg](https://codeberg.org/taisho-guy/Rina/pulls)でプルリクエストを作成します。

## Q & A

<details>

  <summary>開発のきっかけは？</summary>

  ### OSの壁と決定的敗北

  私が愛用する**CachyOS**上でAviUtlを運用しようとして失敗したことがきっかけです。  
  **AviUtlのためだけにWindows環境を維持し続けること**は私にとって受け入れがたいものでした。

  ### 肥大化したエコシステム

  理由は違えど、AviUtlを「仕方なく」使い続けている方は少なくないはずです。
  長年の拡張によって肥大化した「ハウルの動く城」のようなエコシステムは、不満を抱えながらも手放すことができない存在となっています。

  ### プロジェクトの目標とミッション

  私は、[鹿児島県立甲南高等学校](https://edunet002.synapse-blog.jp/konan)の課題研究において、この現状を打破すべくRinaの独自開発を決意しました。
  
  #### 個人的な目標

  Domino、VocalShifter、REAPER、AviUtlをはしごすることなく、Linux上のRinaのみで音MADを制作すること。
  
  #### Rinaのミッション

  AviUtlを「仕方なく」使っている方々の最適解になること。

</details>

<details>

  <summary>名称の由来は？</summary>

  Rinaは、「**R**ina **i**s **n**ot **A**viUtl」の略語です。
  このように、略語自身の定義の中にその略語を含んでいる略語を**再帰的頭字語**といいます。
  この名称は、「AviUtlだけどAviUtlではない」ということを示しています。

  尚、私が所属する弓道部の部員に、「リナ」という人がいます（Rinaの前身である「Noa」についても、「ノア」という部員がいます）。

</details>

<details>

  <summary>アイコンの由来は？</summary>

  ![Icon](./assets/icon.svg)
  
  Tatsh & NAOKIによる楽曲「RED ZONE」のアルバムカバーのフォントからインスピレーションを得ています。
  紫は、[鹿児島県立甲南高等学校](https://edunet002.synapse-blog.jp/konan)のブランドカラーです。

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

  Rinaのコアはクロスプラットフォーム（C++23/Qt6）で設計されていますが、現在は私のメイン環境であるCachyOS向けに最適化しています。
  将来的にWindows/macOSへの対応も行う予定です。

</details>

<details>

  <summary>AviUtlのプラグインやスクリプトはそのまま使えますか？</summary>

  いいえ。仕組みが根本から異なるため互換性はありません。
  しかし、AviUtl以上に柔軟なプラグインシステムを持っています。

</details>

<details>

  <summary>開発を手伝いたいのですが、何から始めればいいですか？</summary>

  ありがとうございます！

  - Issueでのバグ報告や機能提案
  - コントリビュート（プルリクエスト等）
  - 下記のビルド手順を参照し、自身の環境でビルドする
  - [Rina Wiki](https://codeberg.org/taisho-guy/Rina/wiki)を拡充
  - SNS等のプラットフォームでRinaを宣伝
  - Rinaで編集した動画を公開

  等、様々な形で手伝っていただけます。
  よくわからない場合は[taisho-guy](https://www.instagram.com/taisho_guy)までその旨をお伝え下さい。
  あなたに最適なポジションを提案できるかもしれません。

</details>

<details>

  <summary>課題研究が終わった後も開発は続きますか？</summary>

  はい。このプロジェクトは私の「音MAD制作をLinuxで完結させる」という目標から始まっています。
  研究期間終了後も、私自身のメインエディタとして、そしてAviUtlの代替を待つ方々のために開発を継続します。

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

## 謝辞

| プロジェクト名 | ライセンス | 詳細 |
| --- | --- | --- |
| [Remix Icon](https://github.com/Remix-Design/RemixIcon) | [Remix Icon License v1.0](https://raw.githubusercontent.com/Remix-Design/RemixIcon/refs/heads/master/License) | Rina内のシンボルアイコンに採用されています。 |
| [Zrythm](https://www.zrythm.org) | [GNU Affero General Public License Version 3](https://www.gnu.org/licenses/agpl-3.0.txt) | 音声プラグインの対応において、実装を参考にしました。 |

Rinaは、この他にも様々なプロジェクトの上に成り立っています。この場を借りて、感謝を申し上げます。

## ライセンス

Rinaは[GNU Affero General Public License Version 3](https://www.gnu.org/licenses/agpl-3.0.txt)に基づいて公開されています。

### できること

- ソースコードの自由な使用・複製・改変・配布

- 商用利用
  - ただし条件あり
  - 私はデュアルライセンスの提供を考えておりません

### 義務・制限

- ソースコード公開
  - バイナリ配布する場合は対応するソースコードを提供しなければならない

- ネットワーク対話条項
  - 改変版をネットワーク経由でユーザーに提供する場合（SaaS・Web API含む）もソースコードを無償公開しなければならない

- ライセンスの継承
  - ソフトウェアを改変・組み合わせた派生物もAGPLv3でリリースしなければならない

- ライセンス文・著作権表示の保持
  - 改変・再配布時に元のライセンス文と著作権表示を維持する必要がある

- Tivoization禁止
  - ハードウェア側でユーザーによる改変を制限することを禁止

- DRM対策
  - デジタル制限管理への対抗措置が含まれる