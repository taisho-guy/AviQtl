#!/usr/bin/env python3
import os
import json
import argparse
from datetime import datetime
import subprocess

EXCLUDE_FILES = {
    ".DS_Store", "Thumbs.db", "package-lock.json", "yarn.lock", "Icons.js", ".gitignore"
}

def is_text_file(filepath):
    try:
        with open(filepath, 'rb') as f:
            chunk = f.read(1024)
            return b'\x00' not in chunk
    except Exception:
        return False

def get_git_files(abs_root):
    try:
        # --cached: tracked files, --others: untracked, --exclude-standard: respect .gitignore
        # -z: handle filenames with spaces safely
        output = subprocess.check_output(
            ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"],
            cwd=abs_root,
            stderr=subprocess.DEVNULL
        ).decode('utf-8').split('\0')
        return [f for f in output if f]
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None

def generate_tree(file_list):
    tree_str = []
    # Sort for consistent tree display
    for path in sorted(file_list):
        parts = path.split(os.sep)
        indent = "  " * (len(parts) - 1)
        if len(parts) > 1:
            tree_str.append(f"{indent}├── {parts[-1]}")
        else:
            tree_str.append(f"{parts[-1]}")
    return "\n".join(tree_str)

def should_process(filepath, output_file):
    filename = os.path.basename(filepath)
    if filename.startswith("project_context") and filename.endswith(".json"):
        return False
    if os.path.abspath(filepath) == os.path.abspath(output_file):
        return False
    if os.path.basename(filepath) == "export.py":
        return False
    if filename in EXCLUDE_FILES:
        return False
    return is_text_file(filepath)

def get_root_dir():
    try:
        # Get the top-level directory of the git repository
        root = subprocess.check_output(
            ["git", "rev-parse", "--show-toplevel"],
            stderr=subprocess.DEVNULL
        ).decode('utf-8').strip()
        return root
    except (subprocess.CalledProcessError, FileNotFoundError):
        return os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

def generate_export(output_file="project_context.json", root_dir="."):
    """
    Traverses the directory and writes structured content to a JSON file.
    """
    abs_root = os.path.abspath(root_dir)
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    
    print(f"🔍 Scanning structure...")
    git_files = get_git_files(abs_root)
    if git_files is None:
        print("❌ Not a git repository or git not found. Please run within a git project.")
        return

    # Filter only relevant files for the tree and meta
    processed_files = [f for f in git_files if should_process(os.path.join(abs_root, f), output_file)]
    tree_structure = generate_tree(processed_files)

    project_data = {
        "meta": {
            "project_name": os.path.basename(abs_root),
            "date": timestamp,
            "generated_by": "export.py",
            "structure": tree_structure,
            "summary": {}
        },
        "files": []
    }

    extension_counts = {}

    print(f"📄 Reading files...")
    try:
        for rel_path in processed_files:
            filepath = os.path.join(abs_root, rel_path)
            ext = os.path.splitext(rel_path)[1].lower()
            try:
                with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
                    content = f.read()

                extension_counts[ext] = extension_counts.get(ext, 0) + 1
                project_data["files"].append({
                    "path": rel_path,
                    "size": len(content),
                    "extension": ext,
                    "content": content
                })
            except Exception as e:
                print(f"⚠️ Skipping {rel_path}: {e}")

        project_data["meta"]["summary"] = {"file_counts": extension_counts, "total_files": len(project_data["files"])}

        with open(output_file, 'w', encoding='utf-8') as out:
            json.dump(project_data, out, indent=2, ensure_ascii=False)

        print(f"✅ Export completed: {output_file}")

    except IOError as e:
        print(f"❌ Error writing output file: {e}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Export project source code to a single text file.")
    parser.add_argument("-o", "--output", help="Output filename")
    parser.add_argument("-d", "--dir", help="Root directory to scan (defaults to git root)")
    
    args = parser.parse_args()
    root_dir = args.dir if args.dir else get_root_dir()

    output_file = args.output
    if not output_file:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        output_file = f"project_context_{timestamp}.json"

    generate_export(output_file, root_dir)
