#!/usr/bin/env python3
"""
check.py - プロジェクト全体の cppcheck を一括実行するスクリプト
使用法:
    python check.py [オプション]

オプション:
    --jobs N        並列実行数 (デフォルト: CPU コア数)
    --level LEVEL   チェックレベル: normal / all (デフォルト: normal)
    --xml           結果を XML 形式で出力 (check_result.xml に保存)
    --fix           対応可能な警告を自動修正 (実験的)
    --suppress FILE 抑制リストファイルを指定 (デフォルト: .cppcheck_suppress)
    --build-dir DIR compile_commands.json のあるビルドディレクトリを指定
"""

import argparse
import multiprocessing
import os
import shutil
import subprocess
import sys
from pathlib import Path

RESET  = "\033[0m"
RED    = "\033[31m"
YELLOW = "\033[33m"
GREEN  = "\033[32m"
CYAN   = "\033[36m"
BOLD   = "\033[1m"

def find_cpp_files(root: Path) -> list[Path]:
    exclude_dirs = {".git", ".buildtmp", "build", "dist", ".cache", "clap", "vst3sdk"}
    result = []
    for path in root.rglob("*.cpp"):
        if any(p in exclude_dirs for p in path.parts):
            continue
        # Qt 自動生成ファイルを除外
        if path.name.startswith("moc_") or path.name.startswith("qrc_"):
            continue
        result.append(path)
    return sorted(result)

def find_include_dirs(root: Path) -> list[Path]:
    dirs = set()
    for path in root.rglob("*.hpp"):
        if any(p in {".git", ".buildtmp", "build", "clap", "vst3sdk"} for p in path.parts):
            continue
        dirs.add(path.parent)
    return sorted(dirs)

def load_suppress_file(path: Path) -> list[str]:
    if not path.exists():
        return []
    lines = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            lines.append(line)
    return lines

def build_command(
    files: list[Path],
    include_dirs: list[Path],
    suppress_list: list[str],
    args: argparse.Namespace,
    root: Path,
) -> list[str]:
    cmd = ["cppcheck"]

    # 並列数
    cmd += [f"-j{args.jobs}"]

    # C++ 標準
    cmd += ["--std=c++23"]

    # チェックレベル
    if args.level == "all":
        cmd += ["--enable=all"]
    else:
        cmd += ["--enable=warning,performance,portability,information,style"]

    # インクルードディレクトリ
    for d in include_dirs:
        cmd += [f"-I{d}"]

    # Qt / FFmpeg / LuaJIT の最低限の定義 (誤検知抑制)
    defines = [
        "Q_OBJECT=", "Q_INVOKABLE=", "Q_PROPERTY(...)=", "Q_SIGNALS=protected",
        "Q_SLOTS=", "Q_EMIT=", "slots=", "signals=protected:",
        "RINA_HAS_CLAP=1", "RINA_HAS_VST3=1",
    ]
    for d in defines:
        cmd += [f"-D{d}"]

    # 抑制
    for s in suppress_list:
        cmd += [f"--suppress={s}"]
    # Qt 生成コードに起因する誤検知を共通抑制
    cmd += [
        "--suppress=missingIncludeSystem",
        "--suppress=unmatchedSuppression",
        "--suppress=unknownMacro",
    ]

    # compile_commands.json があれば使う
    if args.build_dir:
        bd = Path(args.build_dir)
        cc = bd / "compile_commands.json"
        if cc.exists():
            cmd += [f"--project={cc}"]
        else:
            print(f"{YELLOW}警告: {cc} が見つかりません。ファイルリストで実行します。{RESET}")
            cmd += [str(f) for f in files]
    else:
        cmd += [str(f) for f in files]

    # XML 出力
    if args.xml:
        cmd += ["--xml", "--xml-version=2"]

    # テンプレート (通常テキスト出力時)
    if not args.xml:
        cmd += ["--template={file}:{line}: [{severity}] {id}: {message}"]

    return cmd

def print_summary(returncode: int, xml_path: Path | None) -> None:
    print()
    if returncode == 0:
        print(f"{BOLD}{GREEN}✔  cppcheck 完了: 問題なし{RESET}")
    else:
        print(f"{BOLD}{RED}✘  cppcheck 完了: 警告・エラーあり (終了コード {returncode}){RESET}")
    if xml_path and xml_path.exists():
        print(f"{CYAN}   XML レポート: {xml_path}{RESET}")

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Rina プロジェクト cppcheck 一括実行"
    )
    parser.add_argument(
        "--jobs", type=int,
        default=multiprocessing.cpu_count(),
        help="並列実行数 (デフォルト: CPU コア数)"
    )
    parser.add_argument(
        "--level", choices=["normal", "all"], default="normal",
        help="チェックレベル"
    )
    parser.add_argument(
        "--xml", action="store_true",
        help="XML 形式で出力"
    )
    parser.add_argument(
        "--suppress", default=".cppcheck_suppress",
        metavar="FILE",
        help="抑制リストファイル (デフォルト: .cppcheck_suppress)"
    )
    parser.add_argument(
        "--build-dir", default=None,
        metavar="DIR",
        help="compile_commands.json があるビルドディレクトリ"
    )
    args = parser.parse_args()

    # cppcheck の存在確認
    if not shutil.which("cppcheck"):
        print(f"{RED}エラー: cppcheck が見つかりません。{RESET}")
        print("  CachyOS: sudo pacman -S cppcheck")
        return 1

    root = Path(__file__).parent.resolve()

    print(f"{BOLD}{CYAN}=== Rina cppcheck ==={RESET}")
    print(f"  ルート      : {root}")
    print(f"  並列数      : {args.jobs}")
    print(f"  レベル      : {args.level}")

    # ファイル収集
    cpp_files    = find_cpp_files(root)
    include_dirs = find_include_dirs(root)
    suppress     = load_suppress_file(root / args.suppress)

    print(f"  .cpp ファイル: {len(cpp_files)} 件")
    print(f"  インクルード : {len(include_dirs)} ディレクトリ")
    if suppress:
        print(f"  抑制ルール  : {len(suppress)} 件")
    print()

    # コマンド構築
    cmd = build_command(cpp_files, include_dirs, suppress, args, root)

    if args.xml:
        xml_path = root / "check_result.xml"
        print(f"{CYAN}実行中 (XML → {xml_path})...{RESET}")
        with open(xml_path, "w", encoding="utf-8") as f:
            result = subprocess.run(cmd, stderr=f, cwd=root)
        print_summary(result.returncode, xml_path)
        return result.returncode
    else:
        print(f"{CYAN}実行中...{RESET}")
        result = subprocess.run(cmd, cwd=root)
        print_summary(result.returncode, None)
        return result.returncode

if __name__ == "__main__":
    sys.exit(main())
