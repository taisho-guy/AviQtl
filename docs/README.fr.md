<p align="center">
  <img src="../assets/splash.svg" width="256">
</p>

<p align="center"><b>Logiciel de montage vidéo de nouvelle génération qui suit et surpasse AviUtl</b></p>

<p align="center"><a href="../README.md">日本語</a> | <a href="README.en.md">English</a> | <a href="README.zh-Hans.md">简体中文</a> | <a href="README.zh-Hant.md">繁體中文</a> | <a href="README.ko.md">한국어</a> | <a href="README.es.md">Español</a> | Français | <a href="README.de.md">Deutsch</a></p>

## Qu'est-ce qu'AviQtl ?

AviQtl est un projet visant à développer un logiciel de montage vidéo qui conserve le flux de travail d'**AviUtl 1.10** & **ExEdit 0.92** tout en offrant des **performances supérieures à AviUtl**.
Il prend en charge des **effets accélérés par GPU rapides et puissants** ainsi que des effets audio tels que **VST3 et LV2**.
Le logiciel est conçu pour être indépendant de la plateforme, capable d'être compilé et exécuté sur **Linux**, **Windows** et **macOS**.

**Pour plus de détails, veuillez visiter AviQtl's Room.**

## Q & R

<details>
<summary>Qu'est-ce qui a déclenché le développement ?</summary>

### Barrières de l'OS et Défaite Décisive
Tout a commencé lorsque je n'ai pas réussi à faire fonctionner AviUtl sur mon **CachyOS** préféré. Maintenir un environnement Windows uniquement pour AviUtl était inacceptable.

### Écosystème Boursouflé
De nombreux utilisateurs continuent d'utiliser AviUtl "par nécessité". L'écosystème, qui s'est transformé au fil des ans en quelque chose ressemblant au "Château Ambulant de Howl", est une chose dont les gens ne peuvent se défaire malgré leurs frustrations.

### Objectifs et Mission du Projet
J'ai décidé de développer AviQtl de manière indépendante lors de mes recherches au Lycée Konan de la préfecture de Kagoshima pour briser ce statu quo.

- **Objectif personnel :** Créer des "Oto-MAD" (vidéos MAD basées sur l'audio) sur Linux en utilisant uniquement AviQtl, sans jongler entre Domino, VocalShifter, REAPER et AviUtl.
- **Mission d'AviQtl :** Être la solution optimale pour ceux qui utilisent AviUtl "par nécessité".
</details>

<details>
<summary>Pourquoi développer un clone d'AviUtl ?</summary>

AviQtl n'est pas une "réinvention d'AviUtl". Bien que fortement inspiré par AviUtl, ses composants internes sont complètement différents.

| Caractéristique | AviQtl | AviUtl 1.10 | ExEdit 0.92 |
| :--- | :--- | :--- | :--- |
| Tech de Base | Qt Quick / Vulkan | Win32 API | Win32 API |
| Parallélisme | Piloté par les données (ECS) | Monothread | Multithread |
| Espace Mémoire | 64 bits | 32 bits (Max 4 Go) | 64 bits |
| Aperçu | Vulkan | GDI | Direct3D |
| Moteur Audio | Carla (VST3/LV2, etc.) | Standard uniquement | Standard uniquement |
| Système de Plugins | LuaJIT / C++ / QML / GLSL | Lua / C++ | LuaJIT / C++ |
| OS Supportés | Linux, Windows, macOS, etc. | Windows | Windows |

AviQtl résout fondamentalement les faiblesses structurelles :
1. **Orienté données via ECS (Entity Component System) :** Maximise l'efficacité du cache CPU et accélère le traitement de quantités massives d'objets.
2. **Gestion moderne de la mémoire :** Utilise les pointeurs intelligents C++23 pour minimiser structurellement les plantages inexpliqués.
3. **Séparation de l'UI et du rendu :** Les opérations sur la timeline restent fluides même pendant les rendus lourds, et l'interface utilisateur reste nette dans les environnements High-DPI.
</details>

<details>
<summary>Origine du nom et de l'icône ?</summary>

Le nom est un mot-valise combinant "AviUtl" et "Qt". Le design de l'icône fusionne les logos de Qt et AviUtl.
</details>

## Télécharger

> [!WARNING]
> - Actuellement, AviQtl ne prend en charge que Linux (x86_64). **Les binaires pour Windows/macOS ne sont pas encore fournis.**
> - En raison de l'exigence de paquets récents, il peut ne pas fonctionner sur des distributions conservatrices comme Ubuntu.
> - **L'utilisation de CachyOS est fortement recommandée.**

### Étapes d'installation
1. Installez les dépendances suivantes :
   - Qt6, LuaJIT, implémentation Vulkan (Mesa, etc.), FFmpeg, Carla
2. Téléchargez `AviQtl-Linux-x86_64-v3.zip` depuis la page des Releases.
3. Extrayez le fichier, accordez la permission d'exécution à `AviQtl` et lancez-le.

## Instructions de compilation

Les utilisateurs de Linux peuvent facilement compiler en utilisant un seul fichier `BUILD.py`.

- Installer les dépendances :
  - Pacman : `sudo pacman -S --needed distrobox podman python pyside6 git`
  - APT : `sudo apt install distrobox podman python3 python3-pyside6 git`
  - DNF : `sudo dnf install distrobox podman python3 python3-pyside6 git`
  - MSYS2 : `pacman -S git mingw-w64-ucrt-x86_64-pyside6`

- Cloner le dépôt :
  - `git clone https://codeberg.org/taisho-guy/AviQtl.git`

- Compiler :
  - `cd AviQtl`
  - `python BUILD.py`

- Exécuter :
  - Linux : `./build/AviQtl`
  - MSYS2 : `./build/AviQtl.exe`

> [!IMPORTANT]
> - La vérification de la compilation n'a pas été effectuée sur Windows ou macOS.
> - La compilation se déroule dans Distrobox. Si vous souhaitez compiler dans l'environnement hôte, `BUILD.py` ne peut pas être utilisé.

## Liens connexes

AviQtl repose sur de nombreux projets merveilleux.

| Projet | Licence | Rôle |
| :--- | :--- | :--- |
| AviUtl | Non-libre | Inspiration originale |
| Carla | GPLv2+ | Hôte d'effets audio (VST3/LV2, etc.) |
| FFmpeg | GPLv2+ | Décodage et encodage vidéo/audio |
| LuaJIT | MIT | Moteur de script haute performance |
| Qt | GPLv3 | Framework UI/UX |
| Vulkan | Apache 2.0 | Rendu GPU / Base d'effets |
| Zrythm | AGPLv3 | Référence pour l'implémentation de plugins audio |
| Remix Icon | Remix Icon License | Icônes de symboles |

## Feedback & rapports de bogues

Veuillez créer un ticket.

Même de petits retours aident énormément au développement.
 
## Contribution

En soumettant une pull request ou une autre contribution à ce projet, vous acceptez de fournir votre contribution sous la GNU Affero General Public License. Vos droits d'auteur restent les vôtres, mais le code accepté sera utilisé, modifié et redistribué selon les termes de la GNU Affero General Public License.
 
- Forkez AviQtl et clonez votre fork.
- `cd AviQtl`
- `git checkout -b fix/some-change`
- Faites vos modifications.
- `git add .`
- `git commit -m "Description des changements"`
- `git push -u origin fix/some-change`
- Créez une pull request sur Codeberg.

## Licence

AviQtl est publié sous la GNU Affero General Public License.

Les Remix Icon utilisés dans AviQtl sont fournis sous la Remix Icon License.

## Commentaires et rapports de bogues

Créez un issue.
Même de petits retours aident énormément au développement.

## Contribution

En soumettant une pull request ou une autre contribution à ce projet, vous acceptez de fournir votre contribution sous la GNU Affero General Public License. Vous conservez vos droits d'auteur, mais le code accepté sera utilisé, modifié et redistribué selon les termes de l'AGPLv3.

- Forkez AviQtl et `cloné` votre fork.
- `cd AviQtl`
- `git checkout -b fix/some-change`
- Faites vos modifications.
- `git add .`
- `git commit -m "Description des changements"`
- `git push -u origin fix/some-change`
- Créez une pull request sur Codeberg.

## Licence

AviQtl est publié sous la GNU Affero General Public License.

Les Remix Icon utilisés dans AviQtl sont fournis sous la Remix Icon License.