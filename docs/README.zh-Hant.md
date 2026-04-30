<p align="center">
  <img src="../assets/splash.svg" width="256">
</p>

<p align="center"><b>緊隨並超越 AviUtl 的次世代影片編輯軟體</b></p>

<p align="center"><a href="../README.md">日本語</a> | <a href="README.en.md">English</a> | <a href="README.zh-Hans.md">简体中文</a> | 繁體中文 | <a href="README.ko.md">한국어</a> | <a href="README.es.md">Español</a> | <a href="README.fr.md">Français</a> | <a href="README.de.md">Deutsch</a></p>

## 什麼是 AviQtl？

AviQtl 是一個旨在開發繼承 **AviUtl 1.10** & **ExEdit 0.92** 操作感，同時擁有**超越 AviUtl 性能**的影片編輯軟體項目。
它支持利用 GPU 的**高速且強大的特效**，以及 **VST3 和 LV2** 等音訊特效。
採用跨平台設計，可以在 **Linux**、**Windows** 和 **macOS** 上構建和運行。

**詳情請訪問 [AviQtl 的房間](https://aviqtl.taisho-guy.org)。**

## Q & A

<details>
<summary>開發契機是什麼？</summary>

### 作業系統的壁壘與徹底失敗
起因是我嘗試在心愛的 **CachyOS** 上運行 AviUtl 卻失敗了。為了使用 AviUtl 而不得不維持 Windows 環境，這對我來說是無法接受的。

### 臃腫的生態系統
雖然理由各異，但不少人是因為「別無選擇」才繼續使用 AviUtl。經過多年擴展而變得臃腫不堪、如同「哈爾的移動城堡」般的生態系統，讓人即使心存不滿也難以割捨。

### 項目目標與使命
在[鹿兒島縣立甲南高等學校](https://edunet002.synapse-blog.jp/konan/)的課題研究中，我決定通過自主開發 AviQtl 來打破現狀。

- **個人目標：** 不再往返於 Domino、VocalShifter、REAPER 和 AviUtl 之間，僅憑 Linux 上的 AviQtl 就能製作 Oto-MAD。
- **AviQtl 的使命：** 成為那些「別無選擇」而使用 AviUtl 的人們的最佳解決方案。
</details>

<details>
<summary>為什麼要開發 AviUtl 的克隆版？</summary>

AviQtl 並非「AviUtl 的重複發明」。雖然深受 AviUtl 啟發，但其內部實現完全不同。

| 項目 | AviQtl | AviUtl 1.10 | ExEdit 0.92 |
| :--- | :--- | :--- | :--- |
| 基礎技術 | Qt Quick / Vulkan | Win32 API | Win32 API |
| 並行處理模型 | 數據驅動 (ECS) | 單線程 | 多線程 |
| 內存空間 | 64位 | 32位 (最大 4GB) | 64位 |
| 預覽渲染 | Vulkan | GDI | Direct3D |
| 音訊引擎 | Carla (VST3/LV2 等) | 僅標準功能 | 僅標準功能 |
| 插件方式 | LuaJIT / C++ / QML / GLSL | Lua / C++ | LuaJIT / C++ |
| 對應作業系統 | Linux, Windows, macOS 等 | Windows | Windows |

AviQtl 從根本上解決結構性弱點：
1. **基於 ECS (實體組件系統) 的數據導向：** 極大提高 CPU 快取效率，加速處理海量對象。
2. **現代內存管理：** 採用 C++23 智能指針，從結構上最大限度減少不明原因的崩潰。
3. **UI 與渲染分離：** 即使在進行重型渲染時，時間軸操作也不會被卡頓，且在高分屏 (High-DPI) 環境下 UI 顯示依然清晰。
</details>

<details>
<summary>名稱與圖標的由來？</summary>

名稱是結合「AviUtl」與「Qt」的造詞。圖標設計則融合了 Qt 與 AviUtl 的標誌。
</details>

## 下載

> [!WARNING]
> - 目前 AviQtl 僅支持 Linux (x86_64)。**尚未提供 Windows/macOS 版二進制文件。**
> - 由於需要最新的軟體包，在 Ubuntu 等保守的發行版上可能無法運行。
> - **強烈建議在 [CachyOS](https://cachyos.org/) 上使用。**

### 安裝步驟
1. 安裝以下依賴項：
   - Qt6 全家桶、LuaJIT、Vulkan 實現 (如 Mesa)、FFmpeg、Carla
2. 從 [發佈頁面](https://codeberg.org/taisho-guy/AviQtl/releases) 下載 `AviQtl-Linux-x86_64-v3.zip`。
3. 解壓文件，為 `AviQtl` 賦予執行權限並運行。

## 構建步驟

Linux 用戶可以使用一個 `BUILD.py` 腳本輕鬆構建。

- 安裝依賴：
  - Pacman: `sudo pacman -S --needed distrobox podman python pyside6 git`
  - APT: `sudo apt install distrobox podman python3 python3-pyside6 git`
  - DNF: `sudo dnf install distrobox podman python3 python3-pyside6 git`
  - MSYS2: `pacman -S git mingw-w64-ucrt-x86_64-pyside6`

- 克隆倉庫：
  - `git clone https://codeberg.org/taisho-guy/AviQtl.git`

- 構建：
  - `cd AviQtl`
  - `python BUILD.py`

- 運行：
  - Linux: `./build/AviQtl`
  - MSYS2: `./build/AviQtl.exe`

> [!IMPORTANT]
> - 尚未在 Windows 或 macOS 上進行構建驗證。
> - 構建在 Distrobox 內進行。如果想在宿主機環境構建，則無法使用 `BUILD.py`。

## 相關鏈接

AviQtl 建立在許多優秀的開源項目之上。

| 項目 | 許可證 | 作用 |
| :--- | :--- | :--- |
| AviUtl | 非自由 | 致敬對象 |
| Carla | GPLv2+ | 音訊特效 (VST3/LV2 等) 宿主 |
| FFmpeg | GPLv2+ | 影片/音訊編解碼 |
| LuaJIT | MIT | 高速腳本引擎 |
| Qt | GPLv3 | UI/UX 框架 |
| Vulkan | Apache 2.0 | GPU 渲染 / 特效基礎 |
| Zrythm | AGPLv3 | 音訊插件實現參考 |
| Remix Icon | Remix Icon License | 符號圖標 |

## 反饋與錯誤報告

創建 Issue。
任何微小的反饋都對開發大有裨益。

## 貢獻

向本項目提交拉取請求或其他貢獻時，即視為您同意在 GNU Affero General Public License 下提供您的貢獻。您的貢獻著作權歸您所有，但受理後的代碼將根據 AGPLv3 的條款進行使用、修改和分發。

- Fork AviQtl 並 `clone` 您 Fork 的倉庫。
- `cd AviQtl`
- `git checkout -b fix/some-change`
- 進行修改。
- `git add .`
- `git commit -m "修改內容"`
- `git push -u origin fix/some-change`
- 在 Codeberg 上創建拉取請求。

## 許可證

AviQtl 基於 GNU Affero General Public License 發佈。

AviQtl 中使用的 Remix Icon 是基於 Remix Icon License 提供的。