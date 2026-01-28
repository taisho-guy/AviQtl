#!/usr/bin/env fish

# 設定
set SOURCE_DIR (pwd)
# CMakeが生成する一時ビルドディレクトリ
set TEMP_BUILD_DIR "$SOURCE_DIR/.build_tmp"
# 最終的な成果物を置くディレクトリ
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

# CMakeの設定
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

# 3. 成果物の配置 (簡易版)
# CMakeのPOST_BUILDコマンドが既にリソースを .build_tmp 内にコピーしているはずなので、
# それらをまとめて最終出力ディレクトリに移動する。

echo "成果物を $OUTPUT_DIR に展開中..."
if test -d "$OUTPUT_DIR"
    rm -rf "$OUTPUT_DIR"
end
mkdir -p "$OUTPUT_DIR"

# バイナリのコピー
cp "$EXECUTABLE_NAME" "$OUTPUT_DIR/"

# CMakeが生成したリソースディレクトリがあればコピー
if test -d "effects"
    cp -r "effects" "$OUTPUT_DIR/"
end
if test -d "objects"
    cp -r "objects" "$OUTPUT_DIR/"
end

# 4. クリーンアップ
echo "一時ファイルを削除中..."
cd "$SOURCE_DIR"
rm -rf "$TEMP_BUILD_DIR"

echo "=== ビルド完了 ==="
echo "成果物は $OUTPUT_DIR にあります。"
