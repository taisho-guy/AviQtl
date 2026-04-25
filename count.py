#!/usr/bin/env python3
"""AviQtl プロジェクト ソースコード行数カウンター"""

import sys
from pathlib import Path
from collections import defaultdict

# カウント対象の拡張子とその表示名
TARGET_EXTENSIONS: dict[str, str] = {
    ".cpp":  "C++  ソース",
    ".hpp":  "C++  ヘッダ",
    ".qml":  "QML",
    ".js":   "JavaScript",
    ".py":   "Python",
    ".glsl": "GLSL シェーダ",
    ".frag": "GLSL フラグメント",
    ".vert": "GLSL 頂点",
}

# CMakeLists.txt も個別に対象とする
CMAKE_FILENAME = "CMakeLists.txt"

# スキャン除外ディレクトリ
EXCLUDE_DIRS: frozenset[str] = frozenset({
    ".git", ".build_tmp", "build", "dist",
    "clap", "vst3sdk", ".cache", "__pycache__",
})


def is_excluded(path: Path) -> bool:
    """パスの各部分に除外ディレクトリが含まれるか判定する"""
    return any(part in EXCLUDE_DIRS for part in path.parts)


def count_lines(path: Path) -> tuple[int, int, int]:
    """
    ファイルの行数を返す。
    戻り値: (総行数, コード行数, 空行数)
    """
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return 0, 0, 0

    total = len(lines)
    blank = sum(1 for line in lines if not line.strip())
    code = total - blank
    return total, code, blank


def collect_files(root: Path) -> list[Path]:
    """ルート以下から対象ファイルを再帰収集する"""
    result: list[Path] = []
    for path in root.rglob("*"):
        if path.is_dir():
            continue
        rel = path.relative_to(root)
        if is_excluded(rel):
            continue
        if path.suffix in TARGET_EXTENSIONS:
            result.append(path)
        elif path.name == CMAKE_FILENAME:
            result.append(path)
    return sorted(result)


def format_table_row(label: str, files: int, total: int, code: int, blank: int) -> str:
    return f"  {label:<22} {files:>5}ファイル  {total:>7}行  {code:>7}行  {blank:>7}行"


def main() -> None:
    root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).parent.resolve()

    print(f"\n{'=' * 70}")
    print(f"  AviQtl プロジェクト ソースコード行数レポート")
    print(f"  スキャン対象: {root}")
    print(f"{'=' * 70}")

    files = collect_files(root)

    # 拡張子ごとに集計
    stats: dict[str, dict] = defaultdict(lambda: {"files": 0, "total": 0, "code": 0, "blank": 0})

    for path in files:
        key = path.name if path.name == CMAKE_FILENAME else path.suffix
        total, code, blank = count_lines(path)
        stats[key]["files"] += 1
        stats[key]["total"] += total
        stats[key]["code"]  += code
        stats[key]["blank"] += blank

    # 表示順序
    display_order = list(TARGET_EXTENSIONS.keys()) + [CMAKE_FILENAME]

    print(f"\n  {'種別':<22} {'ファイル数':>10}  {'総行数':>10}  {'コード行':>10}  {'空行':>10}")
    print(f"  {'-' * 66}")

    grand_files = grand_total = grand_code = grand_blank = 0

    for key in display_order:
        if key not in stats:
            continue
        s = stats[key]
        label = TARGET_EXTENSIONS.get(key, key)
        print(format_table_row(label, s["files"], s["total"], s["code"], s["blank"]))
        grand_files += s["files"]
        grand_total += s["total"]
        grand_code  += s["code"]
        grand_blank += s["blank"]

    print(f"  {'-' * 66}")
    print(format_table_row("合計", grand_files, grand_total, grand_code, grand_blank))
    print(f"{'=' * 70}\n")

    # コード行が多い上位10ファイルを表示
    file_ranks: list[tuple[int, int, Path]] = []
    for path in files:
        total, code, blank = count_lines(path)
        file_ranks.append((code, total, path))

    file_ranks.sort(reverse=True)
    print("  コード行数 上位10ファイル:")
    print(f"  {'-' * 66}")
    for i, (code, total, path) in enumerate(file_ranks[:10], 1):
        rel = path.relative_to(root)
        print(f"  {i:>2}. {str(rel):<50} {code:>6}行")
    print()


if __name__ == "__main__":
    main()
