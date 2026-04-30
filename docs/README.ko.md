<p align="center">
  <img src="../assets/splash.svg" width="256">
</p>

<p align="center"><b>AviUtl을 계승하고 능가하는 차세대 동영상 편집 소프트웨어</b></p>

<p align="center"><a href="../README.md">日本語</a> | <a href="README.en.md">English</a> | <a href="README.zh-Hans.md">简体中文</a> | <a href="README.zh-Hant.md">繁體中文</a> | 한국어 | <a href="README.es.md">Español</a> | <a href="README.fr.md">Français</a> | <a href="README.de.md">Deutsch</a></p>

## AviQtl이란?

**AviUtl 1.10** & **ExEdit 0.92**의 조작감을 계승하면서, **AviUtl을 뛰어넘는 성능**을 가진 동영상 편집 소프트웨어를 개발하는 프로젝트입니다.
GPU를 이용한 **빠르고 강력한 이펙트**와 VST3, LV2 등의 **오디오 이펙트**를 적용할 수 있습니다.
**Linux**, **Windows**, **macOS**에서 빌드 및 실행이 가능한 플랫폼 독립적 설계를 채택하고 있습니다.

**자세한 내용은 AviQtl의 방에서 확인해 주세요.**

## Q & A

<details>
<summary>개발 계기는 무엇인가요?</summary>

### OS의 장벽과 결정적 패배
애용하는 **CachyOS**에서 AviUtl을 구동하려다 실패한 것이 계기입니다. **AviUtl만을 위해 Windows 환경을 유지하는 것**은 받아들이기 힘들었습니다.

### 비대해진 에코시스템
이유는 다르더라도 AviUtl을 '어쩔 수 없이' 계속 사용하고 계신 분들이 적지 않을 것입니다. 오랜 기간 확장되어 비대해진 '하울의 움직이는 성'과 같은 에코시스템은, 불만을 가지면서도 손을 뗄 수 없는 존재가 되었습니다.

### 프로젝트의 목표와 미션
가고시마 현립 코난 고등학교의 과제 연구에서 이러한 현상을 타파하고자 AviQtl의 독자 개발을 결심했습니다.

- **개인적인 목표:** Domino, VocalShifter, REAPER, AviUtl을 오가지 않고, Linux의 AviQtl만으로 '오토매드(音MAD)'를 제작하는 것.
- **AviQtl의 미션:** AviUtl을 '어쩔 수 없이' 사용하고 있는 분들에게 최적의 해답이 되는 것.
</details>

<details>
<summary>왜 AviUtl의 클론을 개발하나요?</summary>

AviQtl은 'AviUtl의 재발명'이 아닙니다. AviUtl을 강하게 의식하고 있지만, 그 내부는 완전히 다릅니다.

| 항목 | AviQtl | AviUtl 1.10 | ExEdit 0.92 |
| :--- | :--- | :--- | :--- |
| 기반 기술 | Qt Quick / Vulkan | Win32 API | Win32 API |
| 병렬 처리 모델 | 데이터 기반(ECS) | 싱글 스레드 | 멀티 스레드 |
| 메모리 공간 | 64bit | 32bit (최대 4GB) | 64bit |
| 프리뷰 렌더링 | Vulkan | GDI | Direct3D |
| 오디오 엔진 | Carla (VST3/LV2 등) | 표준 기능만 | 표준 기능만 |
| 플러그인 방식 | LuaJIT / C++ / QML / GLSL | Lua / C++ | LuaJIT / C++ |
| 대응 OS | Linux, Windows, macOS 등 | Windows | Windows |

AviQtl은 구조적인 약점을 근본적으로 해결합니다:
1. **ECS(Entity Component System)를 통한 데이터 지향:** CPU 캐시 효율을 극한으로 높여 대량의 오브젝트 처리를 가속화합니다.
2. **근대적인 메모리 관리:** C++23의 스마트 포인터를 채택하여 원인 불명의 크래시를 구조적으로 최소화합니다.
3. **UI와 렌더링의 분리:** 무거운 렌더링 중에도 타임라인 조작이 방해받지 않으며, High-DPI 환경에서도 UI가 선명하게 표시됩니다.
</details>

<details>
<summary>명칭과 아이콘의 유래는?</summary>

명칭은 'AviUtl'과 'Qt'를 결합한 조어입니다. 아이콘 디자인은 Qt와 AviUtl의 로고를 융합한 것입니다.

<p align="center">
  <img src="../assets/qt.svg" width="64" align="middle"> + <img src="../assets/aviutl.svg" width="64" align="middle"> = <img src="../assets/icon.svg" width="64" align="middle">
</p>
</details>

<details>
<summary>Windows나 macOS에서도 작동합니까?</summary>
설계는 크로스 플랫폼이지만, 현재는 CachyOS에 최적화되어 있습니다. 향후 Windows/macOS에 대한 네이티브 지원도 공식적으로 진행할 예정입니다.
</details>

<details>
<summary>AviUtl 플러그인을 사용할 수 있습니까?</summary>
아니요. 구조가 다르기 때문에 바이너리 호환성은 없습니다. 하지만 LuaJIT를 채택하고 있어 스크립트의 이식성이 높으며, 보다 유연한 확장이 가능합니다.
</details>

## 다운로드

> [!WARNING]
> - 현재 AviQtl은 Linux(x86_64)만 지원합니다. **Windows/macOS용 바이너리는 제공되지 않습니다.**
> - 최신 패키지가 필요하므로 Ubuntu와 같은 보수적인 배포판에서는 동작하지 않을 수 있습니다.
> - **CachyOS에서의 사용을 강력히 권장합니다.**

### 설치 절차
1. 다음 의존성 패키지를 설치합니다:
   - Qt6 전체, LuaJIT, Vulkan 구현(Mesa 등), FFmpeg, Carla
2. 릴리스 페이지에서 `AviQtl-Linux-x86_64-v3.zip`을 다운로드합니다.
3. 파일의 압축을 풀고 `AviQtl`에 실행 권한을 부여한 뒤 실행합니다.

## 빌드 절차

Linux 사용자라면 `BUILD.py` 하나로 간단히 빌드할 수 있습니다.

- 의존성 패키지를 설치합니다.
  - Pacman: `sudo pacman -S --needed distrobox podman python pyside6 git`
  - APT: `sudo apt install distrobox podman python3 python3-pyside6 git`
  - DNF: `sudo dnf install distrobox podman python3 python3-pyside6 git`
  - MSYS2: `pacman -S git mingw-w64-ucrt-x86_64-pyside6`

- 저장소를 클론합니다.
  - `git clone https://codeberg.org/taisho-guy/AviQtl.git`

- 빌드합니다.
  - `cd AviQtl`
  - `python BUILD.py`

- 실행합니다.
  - Linux: `./build/AviQtl`
  - MSYS2: `./build/AviQtl.exe`

> [!IMPORTANT]
> - Windows, macOS에서의 빌드 검증은 수행되지 않았습니다.
> - Distrobox 내에서 빌드를 수행합니다. 호스트 환경에서 빌드하고 싶은 경우 `BUILD.py`는 사용할 수 없습니다.

## 관련 링크

AviQtl은 많은 훌륭한 프로젝트들 위에서 성립되었습니다.

| 프로젝트 | 라이선스 | 역할 |
| :--- | :--- | :--- |
| AviUtl | 비자유 | 리스펙트 대상 |
| Carla | GPLv2+ | 오디오 이펙트(VST3/LV2 등) 호스트 |
| FFmpeg | GPLv2+ | 동영상·오디오 디코드 / 인코드 |
| LuaJIT | MIT | 고속 스크립트 엔진 |
| Qt | GPLv3 | UI/UX 프레임워크 |
| Vulkan | Apache 2.0 | GPU 렌더링 / 이펙트 기반 |
| Zrythm | AGPLv3 | 오디오 플러그인 구현 참고 |
| Remix Icon | Remix Icon License | 심볼 아이콘 |

## 피드백 및 버그 보고

이슈를 생성해 주세요.
사소한 것이라도 개발에 큰 도움이 됩니다.

## 기여

본 프로젝트에 풀 리퀘스트나 기타 기여를 전송할 경우, 귀하는 자신의 기여물을 GNU Affero General Public License 하에 본 프로젝트에 제공하는 것에 동의한 것으로 간주됩니다. 귀하의 기여물에 대한 저작권은 귀하에게 남지만, 수리된 코드는 AGPLv3 조건에 따라 이용·개작·재배포됩니다.

- AviQtl을 포크하고 포크된 저장소를 `clone`합니다.
- `cd AviQtl`
- `git checkout -b fix/some-change`
- 변경을 수행합니다.
- `git add .`
- `git commit -m "변경 내용"`
- `git push -u origin fix/some-change`
- Codeberg에서 풀 리퀘스트를 생성합니다.

## 라이선스

AviQtl은 GNU Affero General Public License에 따라 공개되어 있습니다.

AviQtl 내에서 사용되는 Remix Icon은 Remix Icon License에 따라 제공됩니다.