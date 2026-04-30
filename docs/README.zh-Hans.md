<p align="center">
  <img src="../assets/splash.svg" width="256">
</p>

<p align="center"><b>紧随并超越 AviUtl 的次世代视频编辑软件</b></p>

<p align="center"><a href="../README.md">日本語</a> | <a href="README.en.md">English</a> | 简体中文 | <a href="README.zh-Hant.md">繁體中文</a> | <a href="README.ko.md">한국어</a> | <a href="README.es.md">Español</a> | <a href="README.fr.md">Français</a> | <a href="README.de.md">Deutsch</a></p>

## 什么是 AviQtl？

AviQtl 是一个旨在开发继承 **AviUtl 1.10** & **ExEdit 0.92** 操作感，同时拥有**超越 AviUtl 性能**的视频编辑软件项目。
它支持利用 GPU 的**高速且强大的特效**，以及 **VST3 和 LV2** 等音频特效。
采用跨平台设计，可以在 **Linux**、**Windows** 和 **macOS** 上构建和运行。

**详情请访问 AviQtl 的房间。**

## 问与答

<details>
<summary>开发契机是什么？</summary>

### 操作系统的壁垒与彻底失败
起因是我尝试在心爱的 **CachyOS** 上运行 AviUtl 却失败了。为了使用 AviUtl 而不得不维持 Windows 环境，这对我来说是无法接受的。

### 臃肿的生态系统
虽然理由各异，但不少人是因为“别无选择”才继续使用 AviUtl。经过多年扩展而变得臃肿不堪、如同“哈尔的移动城堡”般的生态系统，让人即使心存不满也难以割舍。

### 项目目标与使命
在鹿儿岛县立甲南高等学校的课题研究中，我决定通过自主开发 AviQtl 来打破现状。

- **个人目标：** 不再往返于 Domino、VocalShifter、REAPER 和 AviUtl 之间，仅凭 Linux 上的 AviQtl 就能制作 Oto-MAD。
- **AviQtl 的使命：** 成为那些“别无选择”而使用 AviUtl 的人们的最佳解决方案。
</details>

<details>
<summary>为什么要开发 AviUtl 的克隆版？</summary>

AviQtl 并非“AviUtl 的重复发明”。虽然深受 AviUtl 启发，但其实部实现完全不同。

| 项目 | AviQtl | AviUtl 1.10 | ExEdit 0.92 |
| :--- | :--- | :--- | :--- |
| 基础技术 | Qt Quick / Vulkan | Win32 API | Win32 API |
| 并行处理模型 | 数据驱动 (ECS) | 单线程 | 多线程 |
| 内存空间 | 64位 | 32位 (最大 4GB) | 64位 |
| 预览渲染 | Vulkan | GDI | Direct3D |
| 音频引擎 | Carla (VST3/LV2 等) | 仅标准功能 | 仅标准功能 |
| 插件方式 | LuaJIT / C++ / QML / GLSL | Lua / C++ | LuaJIT / C++ |
| 对应操作系统 | Linux, Windows, macOS 等 | Windows | Windows |

AviQtl 从根本上解决结构性弱点：
1. **基于 ECS (实体组件系统) 的数据导向：** 极大提高 CPU 缓存效率，加速处理海量对象。
2. **现代内存管理：** 采用 C++23 智能指针，从结构上最大限度减少不明原因的崩溃。
3. **UI 与渲染分离：** 即使在进行重型渲染时，时间轴操作也不会被卡顿，且在高分屏 (High-DPI) 环境下 UI 显示依然清晰。
</details>

<details>
<summary>名称和图标的由来？</summary>

名称是结合“AviUtl”与“Qt”的造词。图标设计则融合了 Qt 与 AviUtl 的标志。

<p align="center">
  <img src="../assets/qt.svg" width="64" align="middle"> + <img src="../assets/aviutl.svg" width="64" align="middle"> = <img src="../assets/icon.svg" width="64" align="middle">
</p>
</details>

<details>
<summary>它在 Windows 或 macOS 上运行吗？</summary>
虽然设计是跨平台的，但目前针对 CachyOS 进行了优化。未来计划正式提供对 Windows 和 macOS 的原生支持。
</details>

<details>
<summary>我可以使用 AviUtl 插件吗？</summary>
不可以。由于架构不同，无法实现二进制兼容。但由于采用了 LuaJIT，脚本的可移植性很高，可以进行更灵活的扩展。
</details>

## 下载

> [!WARNING]
> - 目前 AviQtl 仅支持 Linux (x86_64)。**尚未提供 Windows/macOS 版二进制文件。**
> - 由于需要最新的软件包，在 Ubuntu 等保守的发行版上可能无法运行。
> - **强烈建议在 CachyOS 上使用。**

### 安装步骤
1. 安装以下依赖项：
   - Qt6 全家桶、LuaJIT、Vulkan 实现 (如 Mesa)、FFmpeg、Carla
2. 从 发布页面 下载 `AviQtl-Linux-x86_64-v3.zip`。
3. 解压文件，为 `AviQtl` 赋予执行权限并运行。

## 构建步骤

Linux 用户可以使用一个 `BUILD.py` 脚本轻松构建。

- 安装依赖：
  - Pacman: `sudo pacman -S --needed distrobox podman python pyside6 git`
  - APT: `sudo apt install distrobox podman python3 python3-pyside6 git`
  - DNF: `sudo dnf install distrobox podman python3 python3-pyside6 git`
  - MSYS2: `pacman -S git mingw-w64-ucrt-x86_64-pyside6`

- 克隆仓库：
  - `git clone https://codeberg.org/taisho-guy/AviQtl.git`

- 构建：
  - `cd AviQtl`
  - `python BUILD.py`

- 运行：
  - Linux: `./build/AviQtl`
  - MSYS2: `./build/AviQtl.exe`

> [!IMPORTANT]
> - 尚未在 Windows 或 macOS 上进行构建验证。
> - 构建在 Distrobox 内进行。如果想在宿主机环境构建，则无法使用 `BUILD.py`。

## 相关链接

AviQtl 建立在许多优秀的开源项目之上。

| 项目 | 许可证 | 作用 |
| :--- | :--- | :--- |
| AviUtl | 非自由 | 致敬对象 |
| Carla | GPLv2+ | 音频特效 (VST3/LV2 等) 宿主 |
| FFmpeg | GPLv2+ | 视频/音频编解码 |
| LuaJIT | MIT | 高速脚本引擎 |
| Qt | GPLv3 | UI/UX 框架 |
| Vulkan | Apache 2.0 | GPU 渲染 / 特效基础 |
| Zrythm | AGPLv3 | 音频插件实现参考 |
| Remix Icon | Remix Icon License | 符号图标 |

## 反馈与错误报告

请创建 Issue。
任何微小的反馈都对开发大有裨益。

## 贡献

向本项目提交拉取请求或其他贡献时，即视为您同意在 GNU Affero General Public License 下提供您的贡献。您的贡献著作权归您所有，但受理后的代码将根据 AGPLv3 的条款进行使用、修改和分发。

- Fork AviQtl 并 `clone` 您 Fork 的仓库。
- `cd AviQtl`
- `git checkout -b fix/some-change`
- 进行修改。
- `git add .`
- `git commit -m "修改内容"`
- `git push -u origin fix/some-change`
- 在 Codeberg 上创建拉取请求。

## 许可证

AviQtl 基于 GNU Affero General Public License 发布。

AviQtl 中使用的 Remix Icon 是基于 Remix Icon License 提供的。