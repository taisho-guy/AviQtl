# Rina - Rina is not AviUtl.

RinaはAviUtl 1.10（ExEdit 0.92）の代替を目指す実験プロジェクトです。

## ビルド手順

```
# 依存関係をインストールします
paru -S --needed fish git cmake ninja qt6-base lua-jit

# リポジトリをクローンします
git clone https://codeberg.org/taisho-guy/Rina.git

# ビルドします
cd Rina
fish ./BUILD.fish
```