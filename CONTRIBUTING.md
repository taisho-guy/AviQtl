# AviQtl コントリビューションガイド

AviQtlプロジェクトへようこそ。本プロジェクトは「AviUtlを踏襲し、凌駕する」ことを目標とした次世代動画編集エンジンです。
本プロジェクトは、**データ指向設計 (DOD)** と **ECS (Entity Component System)** を軸としており、これらへの深い理解が求められます。

## 0. 基本原則
- **Performance First**: フレームレートに影響を与えるパスでは、常にキャッシュ効率を考慮してください。
- **Modern Only**: Arch Linux相当の最新環境をターゲットとし、古いコンパイラやOSへの互換性は考慮しません。
- **Safety**: メモリ安全性と脆弱性の排除を徹底してください。

## 1. 開発環境の構築
本プロジェクトは Arch Linux 環境でのビルドを推奨します。

### 必須要件

- Linux
  - Python
  - Distrobox
  - Podman
  - Git
  - PySide6

- Windows
  - MSYS2
  - 


```bash
# クローンとビルド例
git clone [https://github.com/project-aviqtl/AviQtl.git](https://github.com/project-aviqtl/AviQtl.git)
cd AviQtl
python3 BUILD.py
```

## 2. コーディング規約
AviQtlのコードベースは以下の規約に従います。詳細は `format.fish` を参照してください。

### アーキテクチャ制約
- **SoA (Structure of Arrays)**: コンポーネントはデータ局所性を高めるため、SoA形式で管理します。
- **No Heavy Objects in Loop**: `engine/timeline/` 内のループ中で `std::string` や重いオブジェクトを生成しないでください。
- **String Handling**: 文字列は `StringTable` を介してID管理するか、`std::string_view` を使用してください。

### スタイル
- **命名**: クラス名は `PascalCase`、変数・関数名は `snake_case`。
- **コメント**: 日本語で簡潔に記述。装飾記号（`====`等）は禁止。
- **インクルード**: `#pragma once` を使用。

## 3. プルリクエスト (PR) の流れ
1. **Issueの作成**: 大規模な変更や新機能の場合は、まずIssueで設計方針を議論してください。
2. **ブランチ作成**: `feature/` または `fix/` プレフィックスを使用してください。
3. **セルフチェック**: 
   - `python3 check.py` を実行し、静的解析をパスすること。
   - `format.fish` を実行し、コード整形済みであること。
4. **PR提出**: 説明文には「なぜその設計（データ構造）を選んだか」を記述してください。

## 4. UI/QML に関する注意
- UIロジックは `ui/src/` の C++ サービス層に記述し、QMLはビューに徹してください。
- シェーダー（`.frag`, `.comp`）の変更は、必ず `engine/` 側の定数定義（`ssbo_layout.hpp`等）との整合性を確認してください。

## 5. ライセンス
コントリビュートされたコードは、本プロジェクトの `LICENSE.md` (GPL-3.0-or-later等) に基づいて配布されることに同意したものとみなされます。