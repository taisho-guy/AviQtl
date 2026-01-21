# Rina - Rina is not AviUtl.

RinaはAviUtl1.10とExEdit 0.92の代替を目指す実験プロジェクトです。

## ビルド手順

```
# 依存関係をインストールします
paru -S --needed fish git cmake ninja qt6-base lua-jit

# リポジトリをクローンします
git clone https://codeberg.org/taisho-guy/Rina.git

# ビルドします
cd Rina
fish ./build.fish
```