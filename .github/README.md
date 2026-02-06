> [!IMPORTANT]
> 本リポジトリはミラーです。イシューやプルリクエスト等は[Codebergリポジトリ](https://codeberg.org/taisho-guy/Rina)へお願いします。

# Rina - Rina is not AviUtl.

RinaはAviUtl 1.10（ExEdit 0.92）の代替を目指す実験プロジェクトです。

### [Rinaのお部屋](https://taisho-guy.codeberg.page/Rina) ・ [Wiki](https://codeberg.org/taisho-guy/Rina/wiki)

## ダウンロード

- [Windows(x86_64)・macOS(ARM64)](https://github.com/taisho-guy/Rina/releases/latest)

- [Linux(x86_64)](https://codeberg.org/taisho-guy/Rina/releases)

> [!NOTE]
>その他の環境の方は、以下のビルド手順に従うか、Issueを立ててください。

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