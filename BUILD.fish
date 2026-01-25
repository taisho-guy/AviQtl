#!/usr/bin/env fish

# 設定
set SOURCE_DIR (pwd)
set TEMP_BUILD_DIR "$SOURCE_DIR/.build_tmp"
set OUTPUT_DIR "$SOURCE_DIR/build"
set EXECUTABLE_NAME "Rina"
set BUILD_TYPE "Release"

echo "=== Rina ビルドプロセス開始 ($BUILD_TYPE) ==="

# 1. 一時ディレクトリの準備
if test -d "$TEMP_BUILD_DIR"
    echo "既存の一時ビルドディレクトリを削除中..."
    rm -rf "$TEMP_BUILD_DIR"
end
mkdir -p "$TEMP_BUILD_DIR"

# 2. CMake設定とコンパイル
cd "$TEMP_BUILD_DIR"
echo "CMake (Ninja) を使用して構成中..."
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=$BUILD_TYPE "$SOURCE_DIR"

if test $status -ne 0
    echo "エラー: CMakeの設定に失敗しました。"
    cd "$SOURCE_DIR"
    exit 1
end

echo "コンパイル中..."
ninja

if test $status -ne 0
    echo "エラー: ビルドに失敗しました。"
    cd "$SOURCE_DIR"
    exit 1
end

# 3. 成果物のデプロイ
echo "成果物を $OUTPUT_DIR に展開中..."

if test -d "$OUTPUT_DIR"
    rm -rf "$OUTPUT_DIR"
end
mkdir -p "$OUTPUT_DIR"

# 実行可能ファイルのコピー
cp "$EXECUTABLE_NAME" "$OUTPUT_DIR/"

# エフェクトとオブジェクトのコピー
echo "リソースファイルをコピー中..."
mkdir -p "$OUTPUT_DIR/effects"

# エフェクト (Filters)
if test -d "$SOURCE_DIR/ui/qml/effects"
    cp -r "$SOURCE_DIR/ui/qml/effects/"* "$OUTPUT_DIR/effects/"
else
    echo "警告: ui/qml/effects ディレクトリが見つかりません。"
end

# オブジェクト (Generators) -> build/objects (並列配置)
mkdir -p "$OUTPUT_DIR/objects"
if test -d "$SOURCE_DIR/ui/qml/objects"
    cp -r "$SOURCE_DIR/ui/qml/objects/"* "$OUTPUT_DIR/objects/"
else
    echo "警告: ui/qml/objects ディレクトリが見つかりません。"
end

# 4. クリーンアップ
echo "一時ファイルを削除中..."
cd "$SOURCE_DIR"
rm -rf "$TEMP_BUILD_DIR"

echo "=== ビルド完了 ==="
echo "成果物は $OUTPUT_DIR にあります。"
echo "カスタムエフェクトは $OUTPUT_DIR/effects に追加可能です。"
echo "カスタムオブジェクトは $OUTPUT_DIR/objects に追加可能です。"
