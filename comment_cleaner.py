#!/usr/bin/env python3
import os
import re

def clean_comments_in_file(filepath):
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except UnicodeDecodeError:
        return False

    # 文字列リテラルとコメントを正確に区別するための正規表現
    # 文字列リテラルにマッチした場合はそのまま、コメントにマッチした場合は置換関数に回す
    token_pattern = re.compile(
        r'("(?:\\[\s\S]|[^"])*"'             # ダブルクォート文字列
        r'|\'(?:\\[\s\S]|[^\'])*\''          # シングルクォート文字列
        r'|//[^\n]*'                         # 1行コメント
        r'|/\*[\s\S]*?\*/)'                  # ブロックコメント
    )
    
    # 3文字以上連続する装飾記号にマッチ（-, =, *, ~, +, #, _）
    dec_re = re.compile(r'[-=*~+#_]{3,}')

    def replacer(match):
        token = match.group(1)
        if token.startswith('//'):
            # 1行コメント内の装飾記号を除去
            if dec_re.search(token):
                cleaned = dec_re.sub('', token[2:]).strip()
                if not cleaned:
                    return '%%DELETE_ME%%' # 中身が空になったら削除フラグ
                else:
                    return '// ' + cleaned
        elif token.startswith('/*'):
            # ブロックコメント内の装飾記号を除去
            if dec_re.search(token):
                cleaned = dec_re.sub('', token[2:-2]).strip()
                if not cleaned:
                    return '%%DELETE_ME%%'
                else:
                    # 複数行と1行でフォーマットを変える
                    return '/*\n ' + cleaned + '\n*/' if '\n' in cleaned else '/* ' + cleaned + ' */'
        return token # 文字列リテラル等の場合はそのまま

    new_content = token_pattern.sub(replacer, content)

    # 削除フラグの立った空のコメント行を物理的に消し去る
    lines = new_content.split('\n')
    final_lines = []
    for line in lines:
        if '%%DELETE_ME%%' in line:
            line = line.replace('%%DELETE_ME%%', '').rstrip()
            if not line.strip(): # 行が完全に空（あるいは空白のみ）になったらスキップ
                continue
        final_lines.append(line)

    final_content = '\n'.join(final_lines)
    
    # ファイルに変更があった場合のみ上書き保存
    if content != final_content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(final_content)
        return True
    return False

def main():
    root_dir = os.path.abspath(os.getcwd())
    target_exts = {'.cpp', '.hpp', '.h', '.qml'}
    
    # format.fish と同様の除外ディレクトリ
    exclude_dirs = {'.git', 'build', '.build_tmp', '.qt', 'Rina_autogen'}

    cleaned_files = []

    print("\n\033[34m === コードコメントのクリーニング === \033[0m\n")

    for dirpath, dirnames, filenames in os.walk(root_dir):
        # 除外ディレクトリに該当する場合は探索リストから外す
        dirnames[:] = [d for d in dirnames if d not in exclude_dirs]
        
        for file in filenames:
            ext = os.path.splitext(file)[1].lower()
            if ext in target_exts:
                filepath = os.path.join(dirpath, file)
                if clean_comments_in_file(filepath):
                    # プロジェクトルートからの相対パスを表示
                    rel_path = os.path.relpath(filepath, root_dir)
                    cleaned_files.append(rel_path)
                    print(f"  → {rel_path}")

    if cleaned_files:
        print(f"\n\033[32m ✓ 合計 {len(cleaned_files)} 個のファイルのコメント装飾を除去しました！ \033[0m\n")
    else:
        print("\n\033[33m 除去対象のコメントは見つかりませんでした。\033[0m\n")

if __name__ == '__main__':
    main()