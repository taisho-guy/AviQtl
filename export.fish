#!/usr/bin/env fish
# Usage: ./export.fish [output_filename]

# --- Args ---
set OUTPUT_FILE "project_export.txt"
if set -q argv[1]
    set OUTPUT_FILE $argv[1]
end

set PROJECT_ROOT (pwd)
set SCRIPT_NAME (path basename (status filename))

# Extensions to include (regex alternation)
set EXTENSIONS "cpp|hpp|h|c|qml|cmake|sh|txt|md|py|json|qrc|ui|glsl|vert|frag"

# --- Initialize output file ---
echo "# Project Source Code Export" > "$OUTPUT_FILE"
echo "# Generated on: "(date) >> "$OUTPUT_FILE"
echo "# Root: $PROJECT_ROOT" >> "$OUTPUT_FILE"
echo "#" >> "$OUTPUT_FILE"

# --- Project structure ---
echo "================================================================================" >> "$OUTPUT_FILE"
echo "PROJECT STRUCTURE" >> "$OUTPUT_FILE"
echo "================================================================================" >> "$OUTPUT_FILE"

if type -q tree
    tree -I "build|dist|node_modules|__pycache__|.git|.idea|.vscode|$OUTPUT_FILE" >> "$OUTPUT_FILE"
else
    # Fallback tree generation
    find . -maxdepth 4 -not -path '*/.*' -not -path './build*' \
        | sort \
        | sed 's/[^/]*\// /g' >> "$OUTPUT_FILE"
end

echo "" >> "$OUTPUT_FILE"
echo "Scanning directory: $PROJECT_ROOT"

# --- File processor ---
function process_file --argument-names file
    set rel_path (string replace -r '^\\./' '' -- "$file")
    set base (path basename "$file")

    # Skip the export file itself and the script
    if test "$rel_path" = "$OUTPUT_FILE"; or test "$rel_path" = "$SCRIPT_NAME"
        return
    end

    # Check extension or CMakeLists.txt
    if string match -rq -- "\\.($EXTENSIONS)\$" "$file"; or test "$base" = "CMakeLists.txt"
        # Skip binary files (grep -I: treat binary as no-match)
        if grep -qI -m 1 "" -- "$file"
            echo "Exporting: $rel_path"

            echo "================================================================================" >> "$OUTPUT_FILE"
            echo "FILE: $rel_path" >> "$OUTPUT_FILE"
            echo "================================================================================" >> "$OUTPUT_FILE"
            echo "" >> "$OUTPUT_FILE"

            cat "$file" >> "$OUTPUT_FILE"
            echo "" >> "$OUTPUT_FILE"
            echo "" >> "$OUTPUT_FILE"
        end
    end
end

# --- Enumerate files ---
if test -d ".git"; and type -q git
    echo "Using git ls-files..."

    git ls-files | while read -l file
        process_file "./$file"
    end

    # Also check for untracked files
    git ls-files --others --exclude-standard | while read -l file
        process_file "./$file"
    end
else
    echo "Using find..."

    find . -type f -not -path '*/.*' -not -path './build*' \
        | sort | while read -l file
            process_file "$file"
        end
end

echo "Export complete. Output saved to: $OUTPUT_FILE"
