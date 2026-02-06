# Rina - Rina is not AviUtl.

RinaはAviUtl 1.10（ExEdit 0.92）の代替を目指す実験プロジェクトです。

# RinaはCodeberg上で開発されており、本リポジトリはミラーです。イシューやプルリクエスト等は[Codebergリポジトリ](https://codeberg.org/taisho-guy/Rina)へお願いします。

## ビルド手順

```
# 依存関係をインストールします
paru -S --needed fish git cmake ninja qt6-base lua-jit

# リポジトリをクローンします
git clone https://codeberg.org/taisho-guy/Rina.git

# ビルドします
cd Rina
fish ./BUILD.fish

# 実行します
./build/Rina
```

## ライセンス

Rinaは[GNU AFFERO GENERAL PUBLIC LICENSE Version 3](https://www.gnu.org/licenses/agpl-3.0.txt)に基づいて公開されています。