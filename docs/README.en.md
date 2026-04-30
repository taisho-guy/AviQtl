<p align="center">
  <img src="../assets/splash.svg" width="256">
</p>

<p align="center"><b>Next-generation video editing software that follows and surpasses AviUtl</b></p>

<p align="center"><a href="../README.md">日本語</a> | English | <a href="README.zh-Hans.md">简体中文</a> | <a href="README.zh-Hant.md">繁體中文</a> | <a href="README.ko.md">한국어</a> | <a href="README.es.md">Español</a> | <a href="README.fr.md">Français</a> | <a href="README.de.md">Deutsch</a></p>

## What is AviQtl?

AviQtl is a project to develop video editing software that maintains the workflow of **AviUtl 1.10** & **ExEdit 0.92** while delivering **performance that exceeds AviUtl**.
It supports **fast and powerful GPU-accelerated effects** and audio effects such as **VST3 and LV2**.
The software is designed to be platform-independent, capable of building and running on **Linux**, **Windows**, and **macOS**.

**For more details, please visit [AviQtl's Room](https://aviqtl.taisho-guy.org).**

## Q & A

<details>
<summary>What triggered the development?</summary>

### OS Barriers and Decisive Defeat
It started when I failed to run AviUtl on my favorite **CachyOS**. Maintaining a Windows environment solely for AviUtl was unacceptable.

### Bloated Ecosystem
Many users continue to use AviUtl "out of necessity." The ecosystem, which has grown over the years into something like "Howl's Moving Castle," is something people can't let go of despite their frustrations.

### Project Goals and Mission
I decided to develop AviQtl independently during my research at [Kagoshima Prefectural Konan High School](https://edunet002.synapse-blog.jp/konan/) to break this status quo.

- **Personal Goal:** To create "Oto-MAD" (audio-based MAD movies) on Linux using only AviQtl, without jumping between Domino, VocalShifter, REAPER, and AviUtl.
- **AviQtl's Mission:** To be the optimal solution for those using AviUtl "out of necessity."
</details>

<details>
<summary>Why develop an AviUtl clone?</summary>

AviQtl is not a "reinvention of AviUtl." While strongly inspired by AviUtl, its internals are completely different.

| Feature | AviQtl | AviUtl 1.10 | ExEdit 0.92 |
| :--- | :--- | :--- | :--- |
| Base Tech | Qt Quick / Vulkan | Win32 API | Win32 API |
| Parallelism | Data-driven (ECS) | Single-threaded | Multi-threaded |
| Memory Space | 64-bit | 32-bit (Max 4GB) | 64-bit |
| Preview | Vulkan | GDI | Direct3D |
| Audio Engine | Carla (VST3/LV2, etc.) | Standard only | Standard only |
| Plugin System | LuaJIT / C++ / QML / GLSL | Lua / C++ | LuaJIT / C++ |
| Supported OS | Linux, Windows, macOS, etc. | Windows | Windows |

AviQtl fundamentally solves structural weaknesses:
1. **Data-oriented via ECS (Entity Component System):** Maximizes CPU cache efficiency and speeds up processing of massive numbers of objects.
2. **Modern Memory Management:** Uses C++23 smart pointers to structurally minimize unexplained crashes.
3. **UI and Rendering Separation:** Timeline operations remain smooth even during heavy rendering, and the UI remains crisp in High-DPI environments.
</details>

<details>
<summary>Origin of the name and icon?</summary>

The name is a portmanteau of "AviUtl" and "Qt". 
The icon design merges the logos of Qt and AviUtl.

<p align="center">
  <img src="../assets/qt.svg" width="64" align="middle"> + <img src="../assets/aviutl.svg" width="64" align="middle"> = <img src="../assets/icon.svg" width="64" align="middle">
</p>
</details>

<details>
<summary>Does it work on Windows or macOS?</summary>
While the design is cross-platform, it is currently optimized for CachyOS. Official native support for Windows and macOS is planned for the future.
</details>

<details>
<summary>Can I use AviUtl plugins?</summary>
No. Due to structural differences, binary compatibility is not possible. However, since it uses LuaJIT, scripts are highly portable and allow for more flexible expansion.
</details>

## Download

> [!WARNING]
> - Currently, AviQtl only supports Linux (x86_64). **Binaries for Windows/macOS are not provided yet.**
> - Due to the requirement for the latest packages, it may not work on conservative distributions like Ubuntu.
> - **Using [CachyOS](https://cachyos.org/) is highly recommended.**

### Installation Steps
1. Install the following dependencies:
   - Qt6, LuaJIT, Vulkan implementation (Mesa, etc.), FFmpeg, Carla
2. Download `AviQtl-Linux-x86_64-v3.zip` from the Releases page.
3. Extract the file, grant execution permission to `AviQtl`, and run it.

## Build Instructions

Linux users can easily build using a single `BUILD.py`.

- Install dependencies:
  - Pacman: `sudo pacman -S --needed distrobox podman python pyside6 git`
  - APT: `sudo apt install distrobox podman python3 python3-pyside6 git`
  - DNF: `sudo dnf install distrobox podman python3 python3-pyside6 git`
  - MSYS2: `pacman -S git mingw-w64-ucrt-x86_64-pyside6`

- Clone the repository:
  - `git clone https://codeberg.org/taisho-guy/AviQtl.git`

- Build:
  - `cd AviQtl`
  - `python BUILD.py`

- Run:
  - Linux: `./build/AviQtl`
  - MSYS2: `./build/AviQtl.exe`

> [!IMPORTANT]
> - Build verification has not been performed on Windows or macOS.
> - Building takes place within Distrobox. If you wish to build in the host environment, `BUILD.py` cannot be used.

## Related Links

AviQtl stands on the shoulders of many wonderful projects.

| Project | License | Role |
| :--- | :--- | :--- |
| AviUtl | Non-free | Original Inspiration |
| Carla | GPLv2+ | Audio effect host (VST3/LV2, etc.) |
| FFmpeg | GPLv2+ | Video/Audio Decoding & Encoding |
| LuaJIT | MIT | High-performance script engine |
| Qt | GPLv3 | UI/UX Framework |
| Vulkan | Apache 2.0 | GPU Rendering / Effect Foundation |
| Zrythm | AGPLv3 | Reference for audio plugin implementation |
| Remix Icon | Remix Icon License | Symbol Icons |

## Feedback & Bug Reports

Please create an issue.
Even small feedback helps development significantly.

## Contribution

By submitting a pull request or other contribution to this project, you agree to provide your contribution under the GNU Affero General Public License. You retain copyright of your contribution, but the accepted code will be used, modified, and redistributed under the terms of the AGPLv3.

- Fork AviQtl and `clone` your fork.
- `cd AviQtl`
- `git checkout -b fix/some-change`
- Make your changes.
- `git add .`
- `git commit -m "Description of changes"`
- `git push -u origin fix/some-change`
- Create a pull request on Codeberg.

## License

AviQtl is released under the GNU Affero General Public License.

Remix Icon used in AviQtl is provided under the Remix Icon License.