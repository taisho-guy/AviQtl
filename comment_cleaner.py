#!/usr/bin/env python3
"""
AviQtl プロジェクト コメント装飾クリーナー
対象: .cpp / .hpp / .h / .qml
動作:
  - 装飾記号（-=*~+#_| および Unicode 罫線）を除去し本文を保持
  - 本文が空になった場合のみ行ごと削除
  - 連続する空行を 1 行に圧縮
"""

import os, re

# ASCII 装飾記号 + Unicode Box Drawing (U+2500–U+257F) + 全角ダッシュ等
DEC_RE = re.compile(
    r'[-=*~+#_|]{3,}'
    r'|[\u2500-\u257F]{3,}'
    r'|[・•◆◇■□▲▼●○]{3,}'
)


def clean_comments_in_file(filepath: str) -> bool:
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except UnicodeDecodeError:
        return False

    token_pattern = re.compile(
        r'("(?:\\[\s\S]|[^"])*"'
        r"|'(?:\\[\s\S]|[^'])*'"
        r'|//[^\n]*'
        r'|/\*[\s\S]*?\*/)'
    )

    def replacer(match):
        token = match.group(1)

        if token.startswith('//'):
            if DEC_RE.search(token):
                cleaned = DEC_RE.sub('', token[2:]).strip()
                if not cleaned:
                    return '%%DELETE_ME%%'
                return '// ' + cleaned
            return token

        if token.startswith('/*'):
            if DEC_RE.search(token):
                cleaned = DEC_RE.sub('', token[2:-2]).strip()
                if not cleaned:
                    return '%%DELETE_ME%%'
                if '\n' in cleaned:
                    return '/*\n ' + cleaned + '\n*/'
                return '/* ' + cleaned + ' */'
            return token

        return token

    new_content = token_pattern.sub(replacer, content)

    final_lines = []
    for line in new_content.split('\n'):
        if '%%DELETE_ME%%' in line:
            line = line.replace('%%DELETE_ME%%', '').rstrip()
            if not line.strip():
                continue
        final_lines.append(line)

    # 3行以上連続する空行を 1行に圧縮
    result_lines = []
    blank_count = 0
    for line in final_lines:
        if line.strip() == '':
            blank_count += 1
            if blank_count <= 1:
                result_lines.append(line)
        else:
            blank_count = 0
            result_lines.append(line)

    final_content = '\n'.join(result_lines)

    if content != final_content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(final_content)
        return True
    return False


def main():
    root_dir = os.path.abspath(os.getcwd())
    target_exts = {'.cpp', '.hpp', '.h', '.qml'}
    exclude_dirs = {'.git', 'build', '.build_tmp', '.qt', 'AviQtl_autogen', '__pycache__', '.cache'}

    cleaned_files = []
    print("\n\033[34m === コメント装飾クリーナー === \033[0m\n")

    for dirpath, dirnames, filenames in os.walk(root_dir):
        dirnames[:] = [d for d in dirnames if d not in exclude_dirs]
        for fname in filenames:
            if os.path.splitext(fname)[1].lower() in target_exts:
                fpath = os.path.join(dirpath, fname)
                if clean_comments_in_file(fpath):
                    rel = os.path.relpath(fpath, root_dir)
                    cleaned_files.append(rel)
                    print(f"  → {rel}")

    if cleaned_files:
        print(f"\n\033[32m ✓ {len(cleaned_files)} 個のファイルを処理しました \033[0m\n")
    else:
        print("\n\033[33m 除去対象は見つかりませんでした \033[0m\n")


if __name__ == '__main__':
    main()
