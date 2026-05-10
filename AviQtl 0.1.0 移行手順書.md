# AviQtl 0.1.0 移行手順書

仕様書: AviQtl_Spec_v0.1.0.md 準拠
各ステップは前のステップが完了してからのみ着手すること。
各フェーズ完了時にビルドとスモークテストを必ず実施すること。

---

## Phase 0: 環境基盤の確立 ✅ (完了済み)

### Step 0-1: Filamentバイナリの配置
- [x] `vendor/filament/` へ展開
- [x] `CMakeLists.txt` にFilament静的リンク設定を追加 (`CMakeLists_fixed.txt` 適用済み)

### Step 0-2: Qt Quick 3Dの完全除去
- [x] CMakeLists.txtから `Quick3D` を削除
- [x] QMLファイル群から `import QtQuick3D` を削除

---

## Phase 1: Filamentの最小描画確立

**目標**: Filamentがウィンドウ上に単色を描画できることを確認する。

### Step 1-1: FilamentCanvasの作成
- [x] `rendering/include/filament_canvas.hpp` を新規作成
- [x] `rendering/src/filament_canvas.cpp` を新規作成
- [x] Engine::create(VULKAN) → SwapChain → Renderer → Scene → View → Camera の初期化
- [x] `QQuickWindow::beforeRendering` に `renderFrame()` を接続
- [x] geometryChange で正投影カメラ(Z=0等倍)をセットアップ
- 確認: ビルドが通ること

### Step 1-2: CMakeLists.txtへのFilamentCanvas登録
- [x] `rendering/src/` がスキャン対象に含まれていることを確認
- [x] `main.cpp` の `qmlRegisterType` により QML に型登録されていることを確認

### Step 1-3: SceneRenderer.qmlの最小化
- [x] `ui/qml/SceneRenderer.qml` の内部実装を全削除
- [x] 代わりに `FilamentCanvas { anchors.fill: parent }` のみを配置
- [x] `CompositeView.qml` のクリップループ描画ロジックを全削除
- [x] 確認: 起動するとプレビュー領域がFilamentの単色(紺)で塗りつぶされること

---

## Phase 2: CoreBridgeとECS基盤の構築

**目標**: タイムラインの操作(シーク・再生・停止)がECSに反映される。

### Step 2-1: 旧同期機構の除去
- [ ] `ui/src/timeline/timeline_engine_synchronizer.cpp` の全ロジックをコメントアウト
- [ ] `ui/include/timeline_engine_synchronizer.hpp` の依存関係をClipModelから切り離す
- [ ] `TimelineController` から `TimelineEngineSynchronizer` への参照を除去

### Step 2-2: CoreBridgeの実装
- [ ] `ui/include/core_bridge.hpp` を新規作成 (QML_SINGLETON)
- [ ] `requestSeek(int frame)` / `requestPlay()` / `requestPause()` を実装
- [ ] ロックフリーコマンドキュー(`std::atomic`ベース)の実装
- [x] `main.cpp` でシングルトンをQMLエンジンに登録

### Step 2-3: ECSコンポーネント構造体の再定義
- [x] `engine/timeline/ecs.hpp` の `EffectModel*` ポインタを全廃
- [x] 仕様書第4章の通りにPODコンポーネント構造体を定義
  - `ActiveComponent`, `TransformComponent`, `KeyframeRefComponent`
  - `RenderableComponent`, `RenderBoundaryComponent`, `GroupTransformComponent`, `GlobalMatrixComponent`
- [x] SoA配列アロケータ(`DenseComponentMap`)をPOD専用に整理

### Step 2-4: CommandSystemの実装
- [ ] `engine/timeline/ecs_system.cpp` にCommandSystemを実装
- [ ] CoreBridgeのキューを毎フレーム消化してActiveComponentを更新する
- 確認: タイムラインのシークバーを動かすとECS側のCurrentFrameが変化すること

---

## Phase 3: DocumentModelとBake機構の構築

**目標**: プロジェクトのデータ正本とECSのBakeが動作する。

### Step 3-1: DocumentModelクラスの新規作成
- [ ] `core/include/document_model.hpp` を新規作成
- [ ] 既存の `ClipData` / `SceneData` / `Keyframe` 構造体をDocumentModel名前空間へ移管
- [ ] `EffectModel*` ポインタを `uint32_t effectId` + `EffectParams` (POD) に置き換え
- [ ] Undo/Redoスタック (`QUndoStack`) をDocumentModelが所有する

### Step 3-2: Bakeトリガーの実装
- [ ] DocumentModelの構造変化(クリップ追加・削除・エフェクト着脱)を検知するイベントを定義
- [ ] `SystemSettings.bakeStrategy` を読んでFull-Bake / On-Demandを切り替えるBakeコントローラを実装
- [ ] BakeコントローラがECSのエンティティ生成・破棄を呼び出す

### Step 3-3: プロジェクトシリアライザの更新
- [ ] `core/src/project_serializer.cpp` をDocumentModelの新構造体に合わせて書き直し
- 確認: プロジェクトの保存・読み込みが正常に動作すること

---

## Phase 4: Interpolation Engineの実装

**目標**: キーフレームのアニメーションがInterpolationSystemで動作する。

### Step 4-1: 補間エンジンのコアロジック実装
- [ ] `core/include/interpolation_engine.hpp` を新規作成
- [ ] linear: `value = start + (end - start) * t`
- [ ] bezier: cubic bezierの数値解法(Newton-Raphson法)を実装
- [ ] custom: JSON `expression` フィールドをASTにプリコンパイルするパーサを実装
  - 対応演算子: `+`, `-`, `*`, `/`, `pow`, `sin`, `cos`, `sqrt`, `abs`, `clamp`

### Step 4-2: InterpolationSystemの実装
- [ ] `engine/timeline/ecs_system.cpp` にInterpolationSystemを追加
- [ ] KeyframeRefComponentを持つ全エンティティに対し補間エンジンを呼び出す
- [ ] 結果をTransformComponentの各フィールドに書き込む
- 確認: キーフレームを打ったパラメータがフレーム変化で滑らかに補間されること

### Step 4-3: TransformSystemの実装
- [ ] GroupTransformComponentから子レイヤーへのTransform伝播を実装
- [ ] TransformComponent → GlobalMatrixComponent の行列計算(Z=0等倍正投影)を実装

---

## Phase 5: 描画パイプラインの再構築

**目標**: 動画・画像・矩形・テキストがFilamentで表示される。

### Step 5-1: マテリアルの準備
- [ ] `matc` で基本マテリアル(unlit + alpha blending)をコンパイルし `resources/materials/` へ配置
- [ ] CMakeのカスタムターゲットで `.mat` ファイルを自動生成するルールを追加

### Step 5-2: RenderableComponentとRenderSystemの実装
- [ ] FilamentのEntityManager・RenderableManager・TransformManagerをRenderSystemから操作
- [ ] RenderableComponent + GlobalMatrixComponentを持つエンティティをFilamentのSceneに登録
- [ ] RenderBoundaryComponentでRenderGraphのDAGを構築しRender-to-Textureを実装

### Step 5-3: 各ジェネレータの実装
- [ ] RectGenerator: Filamentの平面メッシュ(VertexBuffer + IndexBuffer)を生成
- [ ] ImageGenerator: stb_imageで読み込みFilamentのTextureとして登録
- [ ] VideoGenerator: FFmpegデコード → dmabuf → FilamentのTextureとして登録 (VideoAPIを使用)
- [ ] TextGenerator: Qt QuickオフスクリーンレンダリングによるQImage → FilamentのTexture

### Step 5-4: GroupControlの実装
- [ ] GroupTransformComponent付きエンティティのTransformを子エンティティに伝播させる処理をTransformSystemに統合

---

## Phase 6: エフェクトチェーンの再構築

**目標**: 既存のエフェクト(.frag)がFilamentマテリアルとして適用される。

### Step 6-1: エフェクトシェーダのmatc変換
- [ ] 既存の `.frag` ファイル(15種)をFilamentのマテリアル定義(`.mat`)に変換
  - `borderblur.frag` → `borderblur.mat`
  - 同様に全15エフェクトを変換
- [ ] CMakeビルドルールに `.frag` → `.mat` の自動変換を追加

### Step 6-2: エフェクトチェーンのC++実装
- [ ] `BaseEffect.qml` + `ObjectRenderer.qml` の描画ロジックを全廃
- [ ] EffectChainをC++で実装: エフェクトを連鎖的にRender-to-Textureで適用するパス
- [ ] `evalParam()` 相当をInterpolation Engineのoutputで代替

### Step 6-3: Lua APIのDocumentModel層への再バインド
- [ ] `scripting/lua_host.cpp` のバインディングをDocumentModelのAPIに限定
- [ ] ECSやFilamentへの直接アクセスを遮断

---

## Phase 7: 0.1.0 最終確認

### Step 7-1: リリース要件チェックリスト
- [ ] Filamentがウィンドウに描画できる
- [ ] タイムラインのシーク・再生・停止がECSに反映される
- [ ] 動画クリップ1本のプレビュー表示
- [ ] 画像クリップ1枚のプレビュー表示
- [ ] 矩形(Rect)クリップの描画
- [ ] テキストクリップの表示
- [ ] エフェクト(border_blur)の適用
- [ ] キーフレームアニメーションの動作
- [ ] Undo/Redoの動作
- [ ] aviqtlファイルの保存・読み込み

### Step 7-2: バージョン番号の更新
- [ ] `core/include/version.hpp` を `0.1.0` に更新
- [ ] CHANGELOG.md に移行内容を記載
- [ ] GitタグをPush: `git tag v0.1.0`
