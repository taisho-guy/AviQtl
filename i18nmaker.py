import os

locales = [
    "en_US", "zh_CN", "zh_TW", "ko_KR", "fr_FR", "de_DE", "es_ES",
    "pt_BR", "ru_RU", "it_IT", "vi_VN", "th_TH", "pl_PL", "tr_TR",
    "ar_SA", "hi_IN", "id_ID", "uk_UA", "fil_PH", "sv_SE"
]

def initialize_ts_files():
    i18n_dir = "i18n"
    
    # ディレクトリがない場合は作成
    if not os.path.exists(i18n_dir):
        os.makedirs(i18n_dir)
        print(f"Created directory: {i18n_dir}")

    # 最小限の有効な .ts ファイル構造
    template = '<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE TS>\n<TS version="2.1" language="{0}"/>\n'

    count = 0
    for lang in locales:
        filepath = os.path.join(i18n_dir, f"Rina_{lang}.ts")
        
        if not os.path.exists(filepath):
            with open(filepath, "w", encoding="utf-8") as f:
                f.write(template.format(lang))
            print(f"Generated template: {filepath}")
            count += 1

    print(f"\n完了: {count} 個の新しい言語テンプレートを作成しました。")
    print("この後 'python BUILD.py' を実行すると、ソースから全テキストが抽出されます。")

if __name__ == "__main__":
    initialize_ts_files()