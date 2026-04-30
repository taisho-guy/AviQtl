<p align="center">
  <img src="../assets/splash.svg" width="256">
</p>

<p align="center"><b>Videobearbeitungssoftware der nächsten Generation, die AviUtl folgt und es übertrifft</b></p>

<p align="center"><a href="../README.md">日本語</a> | <a href="README.en.md">English</a> | <a href="README.zh-Hans.md">简体中文</a> | <a href="README.zh-Hant.md">繁體中文</a> | <a href="README.ko.md">한국어</a> | <a href="README.es.md">Español</a> | <a href="README.fr.md">Français</a> | Deutsch</p>

## Was ist AviQtl?

AviQtl ist ein Projekt zur Entwicklung einer Videobearbeitungssoftware, die den Workflow von **AviUtl 1.10** & **ExEdit 0.92** beibehält und gleichzeitig eine **Leistung bietet, die AviUtl übertrifft**.
Es unterstützt **schnelle und leistungsstarke GPU-beschleunigte Effekte** sowie Audioeffekte wie **VST3 und LV2**.
Die Software ist plattformunabhängig konzipiert und kann unter **Linux**, **Windows** und **macOS** erstellt und ausgeführt werden.

**Weitere Details finden Sie im AviQtl's Room.**

## Q & A

<details>
<summary>Was hat die Entwicklung ausgelöst?</summary>

### OS-Barrieren und entscheidende Niederlage
Es begann, als es mir nicht gelang, AviUtl auf meinem bevorzugten **CachyOS** zum Laufen zu bringen. Eine Windows-Umgebung nur für AviUtl aufrechtzuerhalten, war inakzeptabel.

### Aufgeblähtes Ökosystem
Viele Nutzer verwenden AviUtl weiterhin „aus der Not heraus“. Das Ökosystem, das über die Jahre zu etwas wie „Das wandelnde Schloss“ angewachsen ist, ist etwas, das die Leute trotz ihrer Frustration nicht loslassen können.

### Projektziele und Mission
Ich habe mich während meiner Forschung an der Kagoshima Prefectural Konan High School dazu entschlossen, AviQtl unabhängig zu entwickeln, um diesen Status quo zu durchbrechen.

- **Persönliches Ziel:** „Oto-MADs“ (audiobasierte MAD-Videos) unter Linux nur mit AviQtl zu erstellen, ohne zwischen Domino, VocalShifter, REAPER und AviUtl hin- und herzuspringen.
- **AviQtls Mission:** Die optimale Lösung für diejenigen zu sein, die AviUtl „aus der Not heraus“ verwenden.
</details>

<details>
<summary>Warum einen AviUtl-Klon entwickeln?</summary>

AviQtl ist keine „Neuerfindung von AviUtl“. Obwohl es stark von AviUtl inspiriert ist, sind seine Interna völlig unterschiedlich.

| Feature | AviQtl | AviUtl 1.10 | ExEdit 0.92 |
| :--- | :--- | :--- | :--- |
| Basis-Technik | Qt Quick / Vulkan | Win32 API | Win32 API |
| Parallelität | Datengesteuert (ECS) | Single-threaded | Multi-threaded |
| Speicherplatz | 64-Bit | 32-Bit (Max 4GB) | 64-Bit |
| Vorschau | Vulkan | GDI | Direct3D |
| Audio-Engine | Carla (VST3/LV2, etc.) | Nur Standard | Nur Standard |
| Plugin-System | LuaJIT / C++ / QML / GLSL | Lua / C++ | LuaJIT / C++ |
| Unterstützte OS | Linux, Windows, macOS, etc. | Windows | Windows |

AviQtl löst grundlegend strukturelle Schwächen:
1. **Datenorientiert über ECS (Entity Component System):** Maximiert die CPU-Cache-Effizienz und beschleunigt die Verarbeitung massiver Objektmengen.
2. **Modernes Speichermanagement:** Verwendet C++23 Smart Pointers, um unerklärliche Abstürze strukturell zu minimieren.
3. **Trennung von UI und Rendering:** Timeline-Operationen bleiben auch bei schwerem Rendering flüssig, und die UI bleibt in High-DPI-Umgebungen gestochen scharf.
</details>

<details>
<summary>Herkunft des Namens und des Icons?</summary>

Der Name ist ein Kunstwort aus „AviUtl“ und „Qt“. Das Icon-Design kombiniert die Logos von Qt und AviUtl.
</details>

## Download

> [!WARNING]
> - Derzeit unterstützt AviQtl nur Linux (x86_64). **Binärdateien für Windows/macOS werden noch nicht bereitgestellt.**
> - Aufgrund der Anforderung neuester Pakete funktioniert es möglicherweise nicht auf konservativen Distributionen wie Ubuntu.
> - **Die Verwendung von CachyOS wird dringend empfohlen.**

### Installationsschritte
1. Installieren Sie die folgenden Abhängigkeiten:
   - Qt6, LuaJIT, Vulkan-Implementierung (Mesa, etc.), FFmpeg, Carla
2. Laden Sie `AviQtl-Linux-x86_64-v3.zip` von der Release-Seite herunter.
3. Entpacken Sie die Datei, geben Sie `AviQtl` Ausführungsrechte und starten Sie es.

## Bauanleitung

Linux-Nutzer können einfach mit einer einzigen `BUILD.py` bauen.

- Abhängigkeiten installieren:
  - Pacman: `sudo pacman -S --needed distrobox podman python pyside6 git`
  - APT: `sudo apt install distrobox podman python3 python3-pyside6 git`
  - DNF: `sudo dnf install distrobox podman python3 python3-pyside6 git`
  - MSYS2: `pacman -S git mingw-w64-ucrt-x86_64-pyside6`

- Repository klonen:
  - `git clone https://codeberg.org/taisho-guy/AviQtl.git`

- Bauen:
  - `cd AviQtl`
  - `python BUILD.py`

- Ausführen:
  - Linux: `./build/AviQtl`
  - MSYS2: `./build/AviQtl.exe`

> [!IMPORTANT]
> - Die Bauverifizierung wurde unter Windows oder macOS noch nicht durchgeführt.
> - Der Bau findet innerhalb von Distrobox statt. Wenn Sie in der Host-Umgebung bauen möchten, kann `BUILD.py` nicht verwendet werden.

## Verwandte Links

AviQtl steht auf den Schultern vieler wunderbarer Projekte.

| Projekt | Lizenz | Rolle |
| :--- | :--- | :--- |
| AviUtl | Non-free | Ursprüngliche Inspiration |
| Carla | GPLv2+ | Audio-Effekt-Host (VST3/LV2, etc.) |
| FFmpeg | GPLv2+ | Video/Audio Dekodierung & Kodierung |
| LuaJIT | MIT | Hochleistungs-Skript-Engine |
| Qt | GPLv3 | UI/UX Framework |
| Vulkan | Apache 2.0 | GPU-Rendering / Effekt-Basis |
| Zrythm | AGPLv3 | Referenz für Audio-Plugin-Implementierung |
| Remix Icon | Remix Icon License | Symbol-Icons |

## Feedback & Fehlermeldungen

Bitte erstellen Sie ein Issue.
Selbst kleines Feedback hilft der Entwicklung erheblich.

## Mitwirken

Durch das Einreichen eines Pull-Requests oder eines anderen Beitrags zu diesem Projekt erklären Sie sich damit einverstanden, Ihren Beitrag unter der GNU Affero General Public License bereitzustellen. Sie behalten das Urheberrecht an Ihrem Beitrag, aber der akzeptierte Code wird unter den Bedingungen der AGPLv3 verwendet, modifiziert und weiterverbreitet.

- Forken Sie AviQtl und `clone` Sie Ihren Fork.
- `cd AviQtl`
- `git checkout -b fix/some-change`
- Nehmen Sie Ihre Änderungen vor.
- `git add .`
- `git commit -m "Beschreibung der Änderungen"`
- `git push -u origin fix/some-change`
- Erstellen Sie einen Pull-Request auf Codeberg.

## Lizenz

AviQtl wird unter der GNU Affero General Public License veröffentlicht.

Die in AviQtl verwendeten Remix Icon werden unter der Remix Icon License bereitgestellt.