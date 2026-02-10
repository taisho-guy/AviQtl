# Rina - Rina is not AviUtl.

RinaはAviUtl 1.10（ExEdit 0.92）の代替を目指すプロジェクトです。

### [Rinaのお部屋](https://taisho-guy.codeberg.page/Rina) ・ [Wiki](https://codeberg.org/taisho-guy/Rina/wiki)

## ダウンロード

### [Linux(x86_64)・Windows(x86_64)・macOS(ARM64)](https://codeberg.org/taisho-guy/Rina/releases)

> [!NOTE]
> - Linux版は依存関係を同梱しておりません。Qt6, LuaJIT, Vulkan, FFmpegをインストールしてください。
> - Arch Linux上でビルドしているため、最新のランタイムが利用できないディストリビューション（Ubuntu等）では動作しない場合がございます。ローリングリリースのディストリビューション（CachyOS等）でご利用頂くことを推奨致します。

## ビルド手順(CachyOS推奨)

- 依存関係をインストールします

```
paru -S --needed fish git cmake ninja qt6 lua-jit vulkan-devel base-devel mold p7zip  fftw wayland-protocols libffi
```

- リポジトリをクローンします

```
git clone https://codeberg.org/taisho-guy/Rina.git
```

- ビルドします

```
cd Rina
python BUILD.py --gui
```

- 実行します

```
./build/debug/Rina
```

## サードパーティー製の埋め込みリソース

- [Remix Icon](https://github.com/Remix-Design/RemixIcon) (MIT License)

## ライセンス

Rinaは[GNU AFFERO GENERAL PUBLIC LICENSE Version 3](https://www.gnu.org/licenses/agpl-3.0.txt)に基づいて公開されています。