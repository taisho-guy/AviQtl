#!/usr/bin/env fish

# 1. コンパイラ設定: Clangを明示的に指定
set -x CC clang
set -x CXX clang++

# 設定
set SOURCE_DIR (pwd)
# ビルド作業用ディレクトリ（インクリメンタルビルド用に保持）
set TEMP_BUILD_DIR "$SOURCE_DIR/.build_tmp"
# 最終成果物ディレクトリ（常にクリーンな状態にする）
set OUTPUT_DIR "$SOURCE_DIR/build"
set EXECUTABLE_NAME "Rina"
set BUILD_TYPE "Debug"

if contains -- --release $argv
    set BUILD_TYPE "Release"
end

echo "=== Rina ビルドプロセス開始 ($BUILD_TYPE) with Clang ==="

# ---------------------------------------------------------
# Phase 1: コンパイル (作業場 .build_tmp での処理)
# ---------------------------------------------------------

# 作業ディレクトリの作成
if not test -d "$TEMP_BUILD_DIR"
    mkdir -p "$TEMP_BUILD_DIR"
end

# CMake構成 (ビルドタイプ変更を反映するため常に実行)
echo "⚙️  CMake (Ninja) を構成中..."
cmake -B "$TEMP_BUILD_DIR" -G "Ninja" -DCMAKE_BUILD_TYPE=$BUILD_TYPE -S "$SOURCE_DIR"
if test $status -ne 0; echo "❌ CMake構成失敗"; exit 1; end

# コンパイル実行
echo "🔨 コンパイル中..."
cmake --build "$TEMP_BUILD_DIR"
if test $status -ne 0; echo "❌ ビルド失敗"; exit 1; end

# ---------------------------------------------------------
# Phase 2: デプロイ (ショーケース build への配置)
# ---------------------------------------------------------

echo "📦 成果物を $OUTPUT_DIR に配置中..."

# 1. 出力ディレクトリを完全にリセット (要件: 余計なファイルを置かない)
if test -d "$OUTPUT_DIR"
    rm -rf "$OUTPUT_DIR"
end
mkdir -p "$OUTPUT_DIR"

# 2. 必要なファイルのみをコピー (要件: Rina, objects, effects のみ)

# 実行バイナリ
if test -f "$TEMP_BUILD_DIR/$EXECUTABLE_NAME"
    cp "$TEMP_BUILD_DIR/$EXECUTABLE_NAME" "$OUTPUT_DIR/"
else
    echo "❌ 実行ファイルが見つかりません"
    exit 1
end

# エフェクトディレクトリ
if test -d "$SOURCE_DIR/ui/qml/effects"
    cp -r "$SOURCE_DIR/ui/qml/effects" "$OUTPUT_DIR/"
end

# オブジェクトディレクトリ
if test -d "$SOURCE_DIR/ui/qml/objects"
    cp -r "$SOURCE_DIR/ui/qml/objects" "$OUTPUT_DIR/"
end

echo "=== ✅ ビルド完了 ==="
echo "ディレクトリ構成:"
ls -F "$OUTPUT_DIR"
