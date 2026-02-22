#!/usr/bin/env fish

set -l root_dir (pwd)

set -l COLOR_GREEN "\033[32m"
set -l COLOR_BLUE "\033[34m"
set -l COLOR_YELLOW "\033[33m"
set -l COLOR_RESET "\033[0m"

echo -e " $COLOR_BLUE === コード整形 === $COLOR_RESET \n"

# C++/HPP
set -l cpp_files (find $root_dir -type f \( -name "*.cpp" -o -name "*.hpp" \) \
    -not -path "*/build/*" \
    -not -path "*/.build_tmp/*" \
    -not -path "*/.git/*")

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
set -l qml_files (find $root_dir -type f -name "*.qml" \
    -not -path "*/build/*" \
    -not -path "*/.build_tmp/*" \
    -not -path "*/.git/*")

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
