#!/bin/bash
# Usage: ./export.sh [output_filename]

OUTPUT_FILE="${1:-project_export.txt}"
PROJECT_ROOT=$(pwd)
SCRIPT_NAME=$(basename "$0")

# Extensions to include
EXTENSIONS="cpp|hpp|h|c|qml|cmake|sh|txt|md|py|json|qrc|ui|glsl|vert|frag"

# Initialize output file
echo "# Project Source Code Export" > "$OUTPUT_FILE"
echo "# Generated on: $(date)" >> "$OUTPUT_FILE"
echo "# Root: $PROJECT_ROOT" >> "$OUTPUT_FILE"
echo "#" >> "$OUTPUT_FILE"

# Generate Tree Structure
echo "================================================================================" >> "$OUTPUT_FILE"
echo "PROJECT STRUCTURE" >> "$OUTPUT_FILE"
echo "================================================================================" >> "$OUTPUT_FILE"
if command -v tree &> /dev/null; then
    tree -I "build|dist|node_modules|__pycache__|.git|.idea|.vscode|$OUTPUT_FILE" >> "$OUTPUT_FILE"
else
    # Fallback tree generation
    find . -maxdepth 4 -not -path '*/.*' -not -path './build*' | sort | sed 's/[^/]*\//  /g' >> "$OUTPUT_FILE"
fi
echo "" >> "$OUTPUT_FILE"

echo "Scanning directory: $PROJECT_ROOT"

# Function to process files
process_file() {
    local file="$1"
    local rel_path="${file#./}"
    
    # Skip the export file itself and the script
    if [[ "$rel_path" == "$OUTPUT_FILE" || "$rel_path" == "$SCRIPT_NAME" ]]; then return; fi
    
    # Check extension
    if [[ "$file" =~ \.($EXTENSIONS)$ || "$(basename "$file")" == "CMakeLists.txt" ]]; then
        # Skip binary files
        if grep -qI -m 1 "" "$file"; then
            echo "Exporting: $rel_path"
            echo "================================================================================" >> "$OUTPUT_FILE"
            echo "FILE: $rel_path" >> "$OUTPUT_FILE"
            echo "================================================================================" >> "$OUTPUT_FILE"
            echo "" >> "$OUTPUT_FILE"
            cat "$file" >> "$OUTPUT_FILE"
            echo "" >> "$OUTPUT_FILE"
            echo "" >> "$OUTPUT_FILE"
        fi
    fi
}

# Use git ls-files if available to respect .gitignore, otherwise find
if [ -d ".git" ] && command -v git &> /dev/null; then
    echo "Using git ls-files..."
    git ls-files | while read -r file; do
        process_file "./$file"
    done
    # Also check for untracked files
    git ls-files --others --exclude-standard | while read -r file; do
        process_file "./$file"
    done
else
    echo "Using find..."
    find . -type f -not -path '*/.*' -not -path './build*' | sort | while read -r file; do
        process_file "$file"
    done
fi

echo "Export complete. Output saved to: $OUTPUT_FILE"
