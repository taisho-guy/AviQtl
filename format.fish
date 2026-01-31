#!/usr/bin/env fish

set -l root_dir (pwd)

# Color codes for output
set -l COLOR_GREEN "\033[32m"
set -l COLOR_BLUE "\033[34m"
set -l COLOR_YELLOW "\033[33m"
set -l COLOR_RESET "\033[0m"

echo -e " $COLOR_BLUE === Rina Code Formatter === $COLOR_RESET \n"

# ===== C++/HPP files =====
set -l cpp_files (find $root_dir -type f \( -name "*.cpp" -o -name "*.hpp" \) \
    -not -path "*/build/*" \
    -not -path "*/.build_tmp/*" \
    -not -path "*/.git/*")

if test (count $cpp_files) -gt 0
    echo -e " $COLOR_YELLOW Formatting" (count $cpp_files) "C++/HPP files... $COLOR_RESET "
    for file in $cpp_files
        echo "  → $file"
        clang-format -i $file
    end
    echo -e " $COLOR_GREEN ✓ C++ formatting complete! $COLOR_RESET \n"
else
    echo "No C++/HPP files found.\n"
end

# ===== QML files =====
set -l qml_files (find $root_dir -type f -name "*.qml" \
    -not -path "*/build/*" \
    -not -path "*/.build_tmp/*" \
    -not -path "*/.git/*")

if test (count $qml_files) -gt 0
    echo -e " $COLOR_YELLOW Formatting" (count $qml_files) "QML files... $COLOR_RESET "
    for file in $qml_files
        echo "  → $file"
        qmlformat -i $file
    end
    echo -e " $COLOR_GREEN ✓ QML formatting complete! $COLOR_RESET \n"
else
    echo "No QML files found.\n"
end

echo -e " $COLOR_GREEN === All formatting complete! === $COLOR_RESET "
