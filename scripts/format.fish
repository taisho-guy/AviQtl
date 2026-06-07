#!/usr/bin/env fish
set -l script_dir (realpath (dirname (status filename)))
set -l root_dir (realpath "$script_dir/..")
cd "$root_dir"

set -l COLOR_GREEN "\033[32m"
set -l COLOR_BLUE "\033[34m"
set -l COLOR_YELLOW "\033[33m"
set -l COLOR_RESET "\033[0m"

echo -e " $COLOR_BLUE === コード整形 === $COLOR_RESET \n"

# Gitが無視していない C++/HPP/H/MM ファイルを抽出
set -l cpp_files (git ls-files --cached --others --exclude-standard "*.cpp" "*.hpp" "*.h" "*.mm")

if test (count $cpp_files) -gt 0
    echo -e " $COLOR_YELLOW" (count $cpp_files) "個の C++/HPP ファイルを整形中... $COLOR_RESET "
    for file in $cpp_files
        echo "  → $file"
        clang-format -i $file
    end
    echo -e " $COLOR_GREEN ✓ C++ の整形が完了しました！ $COLOR_RESET \n"
else
    echo "C++/HPP ファイルが見つかりませんでした。\n"
end

# QML
# Gitが無視していない QML ファイルを抽出
set -l qml_files (git ls-files --cached --others --exclude-standard "*.qml")

if test (count $qml_files) -gt 0
    echo -e " $COLOR_YELLOW" (count $qml_files) "個の QML ファイルを整形中... $COLOR_RESET "
    for file in $qml_files
        echo "  → $file"
        qmlformat -i $file
    end
    echo -e " $COLOR_GREEN ✓ QML の整形が完了しました！ $COLOR_RESET \n"
else
    echo "QML ファイルが見つかりませんでした。\n"
end

echo -e " $COLOR_GREEN === 全ての整形が完了しました！ === $COLOR_RESET "
