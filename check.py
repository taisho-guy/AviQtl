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
    --full          究極の解析モード (全ツール最高レベル + 自動修正)
    --clazy-level L Clazy のチェックレベル (1-3, デフォルト: 1)
    --import-path P QML のインポートパスを指定 (複数可)
    --skip-qml      QML のチェックをスキップ
    --skip-cppcheck Cppcheck をスキップ
    --skip-clazy    Clazy をスキップ
    --skip-tidy     Clang-Tidy をスキップ
    --skip-format   コード整形をスキップ
"""

import argparse
import concurrent.futures
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

def find_qml_files(root: Path) -> list[Path]:
    exclude_dirs = {".git", ".buildtmp", "build", "dist", ".cache"}
    result = []
    for path in root.rglob("*.qml"):
        if any(p in exclude_dirs for p in path.parts):
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

def run_formatting(root: Path) -> None:
    print(f"{BOLD}{YELLOW}--- Code Formatting ---{RESET}")
    
    # C++ / HPP files
    cpp_hpp_patterns = ["*.cpp", "*.hpp"]
    exclude_dirs = {".git", ".buildtmp", "build", "dist", ".cache", "clap", "vst3sdk"}
    cpp_hpp_files = []
    for pattern in cpp_hpp_patterns:
        for path in root.rglob(pattern):
            if any(p in exclude_dirs for p in path.parts):
                continue
            if path.name.startswith(("moc_", "qrc_")):
                continue
            cpp_hpp_files.append(path)
    
    if cpp_hpp_files and shutil.which("clang-format"):
        print(f"  C++/HPP ({len(cpp_hpp_files)} 件) を整形中...")
        subprocess.run(["clang-format", "-i"] + [str(f) for f in cpp_hpp_files], cwd=root)
    elif not shutil.which("clang-format"):
        print(f"{RED}警告: clang-format が見つかりません。{RESET}")

    # QML files
    qml_files = find_qml_files(root)
    if qml_files and shutil.which("qmlformat"):
        print(f"  QML ({len(qml_files)} 件) を整形中...")
        # 一括で渡して実行
        subprocess.run(["qmlformat", "-i"] + [str(f) for f in qml_files], cwd=root)
    elif not shutil.which("qmlformat"):
        print(f"{RED}警告: qmlformat が見つかりません。{RESET}")
    
    print(f"{GREEN}  整形完了{RESET}\n")

def run_cppcheck(
    files: list[Path],
    include_dirs: list[Path],
    suppress_list: list[str],
    args: argparse.Namespace,
    root: Path
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

    print(f"{BOLD}{YELLOW}--- Cppcheck ---{RESET}")
    result = subprocess.run(cmd, cwd=root)
    return result.returncode

def run_qmllint(files: list[Path], args: argparse.Namespace, root: Path) -> int:
    if not files:
        return 0
    print(f"{BOLD}{YELLOW}--- qmllint (including QQmlSA) ---{RESET}")
    if not shutil.which("qmllint"):
        print(f"{RED}警告: qmllint が見つかりません。スキップします。{RESET}")
        return 0
    
    cmd = ["qmllint"]
    
    # インポートパスの設定
    if args.import_path:
        for p in args.import_path:
            cmd += ["-I", p]
    
    if args.build_dir:
        # ビルドディレクトリ配下のQML関連パスを自動追加
        for sub in ["qml", "Rina"]:
            qml_path = Path(args.build_dir) / sub
            if qml_path.exists():
                cmd += ["-I", str(qml_path)]

    if args.full:
        cmd.append("--compiler-warnings")

    cmd += [str(f) for f in files]
    result = subprocess.run(cmd, cwd=root)
    return result.returncode

def run_clazy(files: list[Path], args: argparse.Namespace, root: Path) -> int:
    print(f"{BOLD}{YELLOW}--- Clazy (Qt Anti-Patterns) ---{RESET}")
    executable = shutil.which("clazy-standalone") or shutil.which("clazy")
    if not executable:
        print(f"{RED}警告: clazy が見つかりません。スキップします。{RESET}")
        return 0

    if not args.build_dir:
        print(f"{RED}エラー: Clazy の実行には --build-dir が必要です。{RESET}")
        return 1

    cc_json = Path(args.build_dir) / "compile_commands.json"
    if not cc_json.exists():
        print(f"{RED}エラー: {cc_json} が見つかりません。{RESET}")
        return 1

    # チェックレベルの設定
    clazy_checks = [f"level{i}" for i in range(1, args.clazy_level + 1)]
    if args.full:
        clazy_checks = ["level1", "level2", "level3"]
    
    checks_str = ",".join(clazy_checks + ["qt6-header-fixes", "qstring-allocations", "manual", "copy-on-write"])
    cmd = [executable, f"-p={args.build_dir}", f"-checks={checks_str}"] + [str(f) for f in files]
    
    # 非常に重いため、並列実行は OS のスケジューラに任せるか、必要に応じて分割が必要
    result = subprocess.run(cmd, cwd=root)
    return result.returncode

def _invoke_tidy(file_path: str, build_dir: str, checks: str, fix: bool) -> int:
    """個別ファイルに対して clang-tidy を実行 (並列処理用)"""
    cmd = ["clang-tidy", f"-p={build_dir}", f"--checks={checks}", "--quiet"]
    if fix:
        cmd.append("--fix-errors") # --full 時はより強力な修正を試みる
    cmd.append(file_path)
    
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.stdout:
        print(res.stdout)
    return res.returncode

def run_clang_tidy(files: list[Path], args: argparse.Namespace, root: Path) -> int:
    print(f"{BOLD}{YELLOW}--- Clang-Tidy (General & Static Analyzer) ---{RESET}")
    if not shutil.which("clang-tidy"):
        print(f"{RED}警告: clang-tidy が見つかりません。スキップします。{RESET}")
        return 0

    if not args.build_dir:
        print(f"{RED}エラー: Clang-Tidy の実行には --build-dir が必要です。{RESET}")
        return 1

    # Qt 向けのチェックも含める
    if args.full:
        checks = "bugprone-*,performance-*,portability-*,modernize-*,clang-analyzer-*,qt-*,readability-*,cppcoreguidelines-*,cert-*"
    else:
        checks = "bugprone-*,performance-*,portability-*,modernize-*,clang-analyzer-*,qt-*"
    
    print(f"  {args.jobs} スレッドで並列実行中...")
    errors = 0
    try:
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
            futures = [
                executor.submit(_invoke_tidy, str(f), args.build_dir, checks, args.fix)
                for f in files
            ]
            for future in concurrent.futures.as_completed(futures):
                if future.result() != 0:
                    errors += 1
        return 0 if errors == 0 else 1
    except Exception as e:
        print(f"{RED}Clang-Tidy 実行中にエラーが発生しました: {e}{RESET}")
        return 1



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
    parser.add_argument(
        "--full", action="store_true", help="究極の解析モード (全ツール最高レベル + 自動修正)"
    )
    parser.add_argument(
        "--clazy-level", type=int, choices=[1, 2, 3], default=1,
        help="Clazy のチェックレベル (1-3)"
    )
    parser.add_argument(
        "--import-path", action="append",
        metavar="PATH",
        help="QML のインポートパス (複数指定可)"
    )
    parser.add_argument(
        "--skip-qml", action="store_true", help="QML のチェックをスキップ"
    )
    parser.add_argument(
        "--skip-cppcheck", action="store_true", help="Cppcheck をスキップ"
    )
    parser.add_argument(
        "--skip-clazy", action="store_true", help="Clazy をスキップ"
    )
    parser.add_argument(
        "--skip-tidy", action="store_true", help="Clang-Tidy をスキップ"
    )
    parser.add_argument(
        "--skip-format", action="store_true", help="コード整形をスキップ"
    )
    parser.add_argument(
        "--fix", action="store_true", help="対応可能な警告を自動修正"
    )

    args = parser.parse_args()

    # --full モード時のオーバーライド
    if args.full:
        args.level = "all"
        args.clazy_level = 3
        args.fix = True

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

    # 0. Formatting (統合されたプロセス)
    if not args.skip_format:
        run_formatting(root)

    # ファイル収集
    cpp_files    = find_cpp_files(root)
    qml_files    = find_qml_files(root)
    include_dirs = find_include_dirs(root)
    suppress     = load_suppress_file(root / args.suppress)

    print(f"  .cpp ファイル: {len(cpp_files)} 件")
    print(f"  .qml ファイル: {len(qml_files)} 件")
    print(f"  インクルード : {len(include_dirs)} ディレクトリ")
    if suppress:
        print(f"  抑制ルール  : {len(suppress)} 件")
    print()

    total_errors = 0

    # 1 & 2. QML Lint (QQmlSA)
    if not args.skip_qml:
        total_errors += run_qmllint(qml_files, args, root)

    # 3. Clazy
    if not args.skip_clazy:
        total_errors += run_clazy(cpp_files, args, root)

    # 4. Clang-Tidy (Includes Static Analyzer)
    if not args.skip_tidy:
        total_errors += run_clang_tidy(cpp_files, args, root)

    # Cppcheck
    if not args.skip_cppcheck:
        # 古い実装との互換性：XML出力は Cppcheck のみ対応
        if args.xml:
            print(f"{CYAN}Cppcheck を XML モードで実行します...{RESET}")
        total_errors += run_cppcheck(cpp_files, include_dirs, suppress, args, root)

    print_summary(total_errors, root / "check_result.xml" if args.xml else None)
    
    return 1 if total_errors > 0 else 0

if __name__ == "__main__":
    sys.exit(main())
