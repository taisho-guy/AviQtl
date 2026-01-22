#!/usr/bin/env fish

# 設定
set SOURCE_DIR (pwd)
set TEMP_BUILD_DIR "$SOURCE_DIR/.build_tmp"
set OUTPUT_DIR "$SOURCE_DIR/build"
set EXECUTABLE_NAME "Rina"
set BUILD_TYPE "Release"

echo "=== Building Rina ($BUILD_TYPE) ==="

# 1. 一時ビルドディレクトリの準備
if test -d $TEMP_BUILD_DIR
    rm -rf $TEMP_BUILD_DIR
end
mkdir -p $TEMP_BUILD_DIR

# 2. CMake設定 & ビルド (一時ディレクトリで実行)
cd $TEMP_BUILD_DIR
echo "Configuring with CMake (Ninja) in temp dir..."
cmake -GNinja -DCMAKE_BUILD_TYPE=$BUILD_TYPE $SOURCE_DIR
if test $status -ne 0
    echo "=== Configuration Failed ==="
    cd $SOURCE_DIR
    exit 1
end

echo "Compiling..."
ninja
if test $status -ne 0
    echo "=== Build Failed ==="
    cd $SOURCE_DIR
    exit 1
end

# 3. 成果物の抽出
echo "Install/Extracting artifacts to $OUTPUT_DIR..."

# 出力ディレクトリのリセット（クリーンインストール）
if test -d $OUTPUT_DIR
    rm -rf $OUTPUT_DIR
end
mkdir -p $OUTPUT_DIR

# 実行可能ファイルのコピー
cp $EXECUTABLE_NAME $OUTPUT_DIR/

# effects ディレクトリの作成
mkdir -p "$OUTPUT_DIR/effects"

# サンプルエフェクトがあればコピー (オプション)
# if test -d "$SOURCE_DIR/examples/effects"
#     cp -r "$SOURCE_DIR/examples/effects/"* "$OUTPUT_DIR/effects/"
# end

# assetsなど実行に必要なリソースがあればここにコピー処理を追加
# cp -r "$SOURCE_DIR/assets" "$OUTPUT_DIR/"

# 4. クリーンアップ
echo "Cleaning up temporary build files..."
cd $SOURCE_DIR
rm -rf $TEMP_BUILD_DIR

echo "=== Build Complete ==="
echo "Artifacts are located in: $OUTPUT_DIR"
echo "You can place custom effects in: $OUTPUT_DIR/effects"
