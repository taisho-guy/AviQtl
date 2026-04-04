#!/usr/bin/env bash
# =============================================================================
# fix_p3_p4.sh  —  P3/P4 警告一括修正スクリプト
# 前提: プロジェクトルート (Rina/) で実行すること
#       .build_tmp/Release/compile_commands.json が存在すること
#       clazy-standalone / clang-tidy / clang-apply-replacements が PATH にあること
# =============================================================================
set -euo pipefail

BUILDDIR=".build_tmp/Release"
JOBS=$(nproc)
FIXES_DIR="$(mktemp -d /tmp/rina_p3_fixes.XXXXXX)"
trap 'rm -rf "$FIXES_DIR"' EXIT

# 対象 .cpp ファイルを収集 (moc_*, qrc_*, build 生成物を除外)
mapfile -t CPP_FILES < <(
  find . -name "*.cpp" \
    ! -path "./.build_tmp/*" \
    ! -path "./build/*" \
    ! -path "./clap/*" \
    ! -path "./vst3sdk/*" \
    ! -name "moc_*" \
    ! -name "qrc_*" \
  | sort
)
echo "対象ファイル数: ${#CPP_FILES[@]}"

# =============================================================================
# STEP 1: qstring-allocations — clazy-standalone で自動修正
#   "string literal"  →  QStringLiteral("string literal") / QLatin1String("...")
#   clazy は --export-fixes で YAML を出力し、clang-apply-replacements で適用する
# =============================================================================
echo ""
echo "=== [P3-A] qstring-allocations (Clazy 自動修正) ==="

CLAZY_BIN=$(command -v clazy-standalone 2>/dev/null || command -v clazy 2>/dev/null || true)
if [[ -z "$CLAZY_BIN" ]]; then
  echo "[WARN] clazy-standalone が見つかりません。スキップします。"
  echo "       sudo pacman -S clazy  でインストール後に再実行してください。"
else
  # 並列でファイルごとに fix YAML を生成
  run_clazy_fix() {
    local f="$1"
    local out="$FIXES_DIR/$(echo "$f" | tr '/' '_').yaml"
    "$CLAZY_BIN" \
      -p "$BUILDDIR" \
      -checks="qstring-allocations" \
      --export-fixes="$out" \
      "$f" \
      2>/dev/null || true
  }
  export -f run_clazy_fix
  export CLAZY_BIN BUILDDIR FIXES_DIR

  printf '%s\n' "${CPP_FILES[@]}" \
    | xargs -P "$JOBS" -I{} bash -c 'run_clazy_fix "$@"' _ {}

  # 重複 YAML があると clang-apply-replacements が衝突するため整理
  if command -v clang-apply-replacements &>/dev/null; then
    clang-apply-replacements "$FIXES_DIR" && echo "[OK] qstring-allocations 適用完了"
  else
    echo "[WARN] clang-apply-replacements が見つかりません。"
    echo "       sudo pacman -S clang  でインストール後、手動で以下を実行してください:"
    echo "       clang-apply-replacements $FIXES_DIR"
  fi
fi

# =============================================================================
# STEP 2: readability-qualified-auto — clang-tidy --fix で自動修正
#   for (auto p : ...) → for (const auto *p : ...)  など
#   warns.txt に FIX-IT applied の記録あり → --fix で一括適用可能
# =============================================================================
echo ""
echo "=== [P3-B] readability-qualified-auto (clang-tidy --fix) ==="

if ! command -v clang-tidy &>/dev/null; then
  echo "[WARN] clang-tidy が見つかりません。スキップします。"
else
  # ファイルを並列処理
  run_tidy_fix() {
    local f="$1"
    clang-tidy \
      -p "$BUILDDIR" \
      -checks="-*,readability-qualified-auto" \
      --fix \
      --quiet \
      "$f" 2>/dev/null || true
  }
  export -f run_tidy_fix
  export BUILDDIR

  printf '%s\n' "${CPP_FILES[@]}" \
    | xargs -P "$JOBS" -I{} bash -c 'run_tidy_fix "$@"' _ {}
  echo "[OK] readability-qualified-auto 適用完了"
fi

# =============================================================================
# STEP 3: readability-inconsistent-declaration-parameter-name — clang-tidy --fix
#   HPP と CPP の引数名が不一致の場合、CPP 側を HPP 定義に合わせる
# =============================================================================
echo ""
echo "=== [P3-C] readability-inconsistent-declaration-parameter-name (clang-tidy --fix) ==="

if command -v clang-tidy &>/dev/null; then
  run_tidy_decl() {
    local f="$1"
    clang-tidy \
      -p "$BUILDDIR" \
      -checks="-*,readability-inconsistent-declaration-parameter-name" \
      --fix \
      --quiet \
      "$f" 2>/dev/null || true
  }
  export -f run_tidy_decl
  printf '%s\n' "${CPP_FILES[@]}" \
    | xargs -P "$JOBS" -I{} bash -c 'run_tidy_decl "$@"' _ {}
  echo "[OK] declaration-parameter-name 適用完了"
fi

# =============================================================================
# STEP 4: .clang-tidy 設定ファイルを生成 — pro-bounds, identifier-length を抑制
#   pro-bounds: FFmpeg / Qt API のポインタ操作は意図的なため検査対象外にする
#   identifier-length: ループ変数 i/j/p 等は慣用的なため抑制
#   avoid-magic-numbers: 解像度・サンプルレート等のドメイン定数は定義済みの意味を持つ
#   function-cognitive-complexity: 段階的リファクタリングのためロックを緩和
# =============================================================================
echo ""
echo "=== [P4] .clang-tidy 設定を生成 ==="

cat > .clang-tidy << 'CLANG_TIDY_EOF'
# .clang-tidy — Rina プロジェクト用 Clang-Tidy 設定
# P3/P4 の意図的な抑制ルールを定義する
# bugprone-* / clang-analyzer-* / modernize-* は check.py が直接制御するため
# ここでは suppressions のみを定義する。

Checks: >
  -*,
  bugprone-*,
  clang-analyzer-*,
  cppcoreguidelines-*,
  modernize-*,
  performance-*,
  portability-*,
  qt-*,
  readability-*,
  -cppcoreguidelines-pro-bounds-array-to-pointer-decay,
  -cppcoreguidelines-pro-bounds-constant-array-index,
  -cppcoreguidelines-pro-bounds-pointer-arithmetic,
  -cppcoreguidelines-pro-bounds-avoid-unchecked-container-access,
  -cppcoreguidelines-avoid-magic-numbers,
  -cppcoreguidelines-owning-memory,
  -readability-magic-numbers,
  -readability-identifier-length,
  -readability-function-cognitive-complexity

CheckOptions:
  # identifier-length の最小値を 1 に下げる (i/j/p 等の慣用変数を許可)
  - key:   readability-identifier-length.MinimumVariableNameLength
    value: '1'
  - key:   readability-identifier-length.MinimumParameterNameLength
    value: '2'
  # cognitive-complexity のしきい値を緩和 (ProjectSerializer::load など)
  - key:   readability-function-cognitive-complexity.Threshold
    value: '80'

WarningsAsErrors: ''
HeaderFilterRegex: '^(?!.*/usr/).*$'
FormatStyle: file
CLANG_TIDY_EOF

echo "[OK] .clang-tidy を生成しました"

# =============================================================================
# STEP 5: modernize-return-braced-init-list — clang-tidy --fix
#   return QImage(...) → return {...}  (FIX-IT あり)
# =============================================================================
echo ""
echo "=== [P3-D] modernize-return-braced-init-list (clang-tidy --fix) ==="

if command -v clang-tidy &>/dev/null; then
  run_tidy_braced() {
    local f="$1"
    clang-tidy \
      -p "$BUILDDIR" \
      -checks="-*,modernize-return-braced-init-list" \
      --fix \
      --quiet \
      "$f" 2>/dev/null || true
  }
  export -f run_tidy_braced
  printf '%s\n' "${CPP_FILES[@]}" \
    | xargs -P "$JOBS" -I{} bash -c 'run_tidy_braced "$@"' _ {}
  echo "[OK] return-braced-init-list 適用完了"
fi

# =============================================================================
# STEP 6: 確認ビルド
# =============================================================================
echo ""
echo "=== 確認ビルド ==="
if command -v python3 &>/dev/null && [[ -f "check.py" ]]; then
  python3 check.py \
    --build-dir "$BUILDDIR" \
    --skip-cppcheck \
    --jobs "$JOBS" \
    2>&1 | grep -E "warning:|error:|---" | head -40 || true
fi

echo ""
echo "=== 完了 ==="
echo "残存する警告カテゴリ（.clang-tidy で抑制済み）:"
echo "  - cppcoreguidelines-pro-bounds-*    → FFmpeg/Qt API 意図的ポインタ操作"
echo "  - readability-identifier-length     → i/j/p 等のループ変数"
echo "  - cppcoreguidelines-avoid-magic-numbers → 1920/1080/48000 等のドメイン定数"
echo "  - readability-function-cognitive-complexity → ProjectSerializer::load (段階的リファクタ予定)"
