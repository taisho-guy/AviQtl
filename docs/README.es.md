<p align="center">
  <img src="../assets/splash.svg" width="256">
</p>

<p align="center"><b>Software de edición de video de próxima generación que sigue y supera a AviUtl</b></p>

<p align="center"><a href="../README.md">日本語</a> | <a href="README.en.md">English</a> | <a href="README.zh-Hans.md">简体中文</a> | <a href="README.zh-Hant.md">繁體中文</a> | <a href="README.ko.md">한국어</a> | Español | <a href="README.fr.md">Français</a> | <a href="README.de.md">Deutsch</a></p>

## ¿Qué es AviQtl?

AviQtl es un proyecto para desarrollar un software de edición de video que mantiene el flujo de trabajo de **AviUtl 1.10** y **ExEdit 0.92**, al tiempo que ofrece un **rendimiento que supera a AviUtl**.
Soporta **efectos acelerados por GPU rápidos y potentes** y efectos de audio como **VST3 y LV2**.
El software está diseñado para ser independiente de la plataforma, capaz de compilarse y ejecutarse en **Linux**, **Windows** y **macOS**.

**Para más detalles, visite AviQtl's Room.**

## Preguntas y Respuestas

<details>
<summary>¿Qué desencadenó el desarrollo?</summary>

### Barreras del SO y Derrota Decisiva
Comenzó cuando no pude ejecutar AviUtl en mi **CachyOS** favorito. Mantener un entorno Windows únicamente para AviUtl era inaceptable.

### Ecosistema Hinchado
Muchos usuarios continúan usando AviUtl "por necesidad". El ecosistema, que ha crecido a lo largo de los años en algo parecido al "Castillo Ambulante de Howl", es algo que la gente no puede abandonar a pesar de sus frustraciones.

### Objetivos y Misión del Proyecto
Decidí desarrollar AviQtl de forma independiente durante mi investigación en la Escuela Secundaria Konan de la Prefectura de Kagoshima para romper este statu quo.

- **Objetivo Personal:** Crear "Oto-MAD" (videos MAD basados en audio) en Linux usando solo AviQtl, sin saltar entre Domino, VocalShifter, REAPER y AviUtl.
- **Misión de AviQtl:** Ser la solución óptima para aquellos que usan AviUtl "por necesidad".
</details>

<details>
<summary>¿Por qué desarrollar un clon de AviUtl?</summary>

AviQtl no es una "reinvención de AviUtl". Aunque está fuertemente inspirado en AviUtl, sus componentes internos son completamente diferentes.

| Característica | AviQtl | AviUtl 1.10 | ExEdit 0.92 |
| :--- | :--- | :--- | :--- |
| Tecnología Base | Qt Quick / Vulkan | Win32 API | Win32 API |
| Paralelismo | Basado en datos (ECS) | Un solo hilo | Multihilo |
| Espacio de Memoria | 64 bits | 32 bits (Máx 4GB) | 64 bits |
| Vista Previa | Vulkan | GDI | Direct3D |
| Motor de Audio | Carla (VST3/LV2, etc.) | Solo estándar | Solo estándar |
| Sistema de Plugins | LuaJIT / C++ / QML / GLSL | Lua / C++ | LuaJIT / C++ |
| SO Soportados | Linux, Windows, macOS, etc. | Windows | Windows |

AviQtl resuelve fundamentalmente las debilidades estructurales:
1. **Orientado a datos vía ECS (Entity Component System):** Maximiza la eficiencia de la caché de la CPU y acelera el procesamiento de cantidades masivas de objetos.
2. **Gestión de Memoria Moderna:** Utiliza punteros inteligentes C++23 para minimizar estructuralmente los bloqueos inexplicables.
3. **Separación de UI y Renderizado:** Las operaciones de la línea de tiempo permanecen fluidas incluso durante renderizados pesados, y la interfaz de usuario se mantiene nítida en entornos de alta densidad de píxeles (High-DPI).
</details>

<details>
<summary>¿Origen del nombre y el icono?</summary>

El nombre es una combinación de "AviUtl" y "Qt". El diseño del icono fusiona los logotipos de Qt y AviUtl.
</details>

## Descargar

> [!WARNING]
> - Actualmente, AviQtl solo es compatible con Linux (x86_64). **Aún no se proporcionan binarios para Windows/macOS.**
> - Debido al requisito de los paquetes más recientes, es posible que no funcione en distribuciones conservadoras como Ubuntu.
> - **Se recomienda encarecidamente el uso de CachyOS.**

### Pasos de Instalación
1. Instale las siguientes dependencias:
   - Qt6, LuaJIT, implementación de Vulkan (Mesa, etc.), FFmpeg, Carla
2. Descargue `AviQtl-Linux-x86_64-v3.zip` desde la página de Lanzamientos.
3. Extraiga el archivo, otorgue permiso de ejecución a `AviQtl` y ejecútelo.

## Instrucciones de Construcción

Los usuarios de Linux pueden construir fácilmente usando un solo `BUILD.py`.

- Instalar dependencias:
  - Pacman: `sudo pacman -S --needed distrobox podman python pyside6 git`
  - APT: `sudo apt install distrobox podman python3 python3-pyside6 git`
  - DNF: `sudo dnf install distrobox podman python3 python3-pyside6 git`
  - MSYS2: `pacman -S git mingw-w64-ucrt-x86_64-pyside6`

- Clonar el repositorio:
  - `git clone https://codeberg.org/taisho-guy/AviQtl.git`

- Construir:
  - `cd AviQtl`
  - `python BUILD.py`

- Ejecutar:
  - Linux: `./build/AviQtl`
  - MSYS2: `./build/AviQtl.exe`

> [!IMPORTANT]
> - No se ha realizado la verificación de construcción en Windows o macOS.
> - La construcción se lleva a cabo dentro de Distrobox. Si desea construir en el entorno del host, no puede usar `BUILD.py`.

## Enlaces Relacionados

AviQtl se apoya en muchos proyectos maravillosos.

| Proyecto | Licencia | Rol |
| :--- | :--- | :--- |
| AviUtl | No libre | Inspiración Original |
| Carla | GPLv2+ | Host de efectos de audio (VST3/LV2, etc.) |
| FFmpeg | GPLv2+ | Decodificación y Codificación de Video/Audio |
| LuaJIT | MIT | Motor de script de alto rendimiento |
| Qt | GPLv3 | Framework de UI/UX |
| Vulkan | Apache 2.0 | Renderizado GPU / Base de Efectos |
| Zrythm | AGPLv3 | Referencia para implementación de plugins de audio |
| Remix Icon | Remix Icon License | Iconos de Símbolos |

## Comentarios y Reportes de Errores

Cree un issue.
Incluso los pequeños comentarios ayudan significativamente al desarrollo.

## Contribución

Al enviar un pull request u otra contribución a este proyecto, usted acepta proporcionar su contribución bajo la GNU Affero General Public License. Usted conserva los derechos de autor de su contribución, pero el código aceptado será usado, modificado y redistribuido bajo los términos de la AGPLv3.

- Haga un Fork de AviQtl y haga `clone` de su fork.
- `cd AviQtl`
- `git checkout -b fix/some-change`
- Realice sus cambios.
- `git add .`
- `git commit -m "Descripción de los cambios"`
- `git push -u origin fix/some-change`
- Cree un pull request en Codeberg.

## Licencia

AviQtl se publica bajo la GNU Affero General Public License.

El Remix Icon utilizado en AviQtl se proporciona bajo la Remix Icon License.