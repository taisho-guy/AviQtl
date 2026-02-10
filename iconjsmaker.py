import re
import os

# パスの定義（プロジェクト構造に合わせる）
CSS_PATH = 'ui/resources/remixicon.css'
OUT_PATH = 'ui/qml/common/Icons.js'

if not os.path.exists(CSS_PATH):
    print(f"Error: {CSS_PATH} が見つかりません。ファイルを配置してください。")
    exit(1)

with open(CSS_PATH, 'r', encoding='utf-8') as f:
    css_content = f.read()

# 正規表現で抽出
pattern = r'\.ri-([\w-]+):before\s*\{\s*content:\s*"\\(\w+)"'
matches = re.findall(pattern, css_content)

os.makedirs(os.path.dirname(OUT_PATH), exist_ok=True)
with open(OUT_PATH, 'w', encoding='utf-8') as f:
    f.write(".pragma library\n\nconst RI = {\n")
    for name, code in matches:
        js_name = name.replace('-', '_')
        f.write(f'    "{js_name}": "\\u{code}",\n')
    f.write("};\n")

print(f"Success: {len(matches)} 個のアイコンを {OUT_PATH} に書き出しました。")