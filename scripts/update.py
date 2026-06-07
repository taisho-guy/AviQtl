#!/usr/bin/env python3
import os
import subprocess

def update_bgfx_dependencies():
    # bgfx.cmake ポートをメインの依存関係として扱う
    url = "https://github.com/bkaradzic/bgfx.cmake.git"
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
    repo_path = os.path.join(root_dir, "bgfx.cmake")

    if not os.path.exists(repo_path):
        print(f"Cloning bgfx.cmake into {root_dir}...")
        # bgfx, bimg, bx をサブモジュールとして含むため --recursive が必須
        subprocess.run(["git", "clone", "--recursive", url], cwd=root_dir, check=True)
    else:
        print(f"Updating bgfx.cmake in {repo_path}...")
        subprocess.run(["git", "pull"], cwd=repo_path, check=True)
        subprocess.run(["git", "submodule", "update", "--init", "--recursive"], cwd=repo_path, check=True)

    # 古い個別ディレクトリがある場合は警告（または手動で削除してください）
    for old in ["bx", "bimg", "bgfx"]:
        if os.path.exists(os.path.join(root_dir, old)) and not os.path.exists(os.path.join(root_dir, old, ".git")):
            print(f"Note: Old directory '{old}' detected. bgfx.cmake uses its own subfolders.")

if __name__ == "__main__":
    try:
        update_bgfx_dependencies()
        print("All repositories are up to date.")
    except subprocess.CalledProcessError as e:
        print(f"An error occurred while updating repositories: {e}")