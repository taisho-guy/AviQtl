import sys
import os
import subprocess
import shutil
import multiprocessing
from pathlib import Path
from PySide6 import QtCore
import platform
import shlex
import urllib.request

class BuildWorker(QtCore.QThread):
    progress_signal = QtCore.Signal(int, str)
    log_signal = QtCore.Signal(str)
    finished_signal = QtCore.Signal(bool, str)
    dist_dir = None

    def __init__(self, source_dir, temp_base, output_dir, is_debug=False, use_container=True):
        super().__init__()
        self.source_dir = source_dir
        self.temp_base = temp_base
        self.output_dir = output_dir
        self.is_debug = is_debug
        self.use_container_opt = use_container
        self.dist_dir = self.source_dir / "dist"
        self.system = platform.system()
        self.container_name = "archlinux-rina"

    def _run_cmd(self, cmd, shell=False, in_container=False):
        display_cmd = ' '.join(cmd) if isinstance(cmd, list) else cmd
        prefix = "[Container] " if in_container else ""
        self.log_signal.emit(f"{prefix}Run: {display_cmd}")

        # インタラクティブなプロンプト（Gitのユーザー名入力など）を無効化し、オートアップデートを抑制
        env = os.environ.copy()
        env["GIT_TERMINAL_PROMPT"] = "0"
        env["HOMEBREW_NO_AUTO_UPDATE"] = "1"

        actual_cmd = cmd
        if in_container and self.system == "Linux":
            if isinstance(cmd, str):
                cmd = shlex.split(cmd)
            
            # コンテナ内でカレントディレクトリを維持して実行
            cmd_str = shlex.join(cmd)
            cwd = os.getcwd()
            # プロセスをシェルで包んで実行 (shell=True)
            inner_cmd = f"cd {shlex.quote(cwd)} && {cmd_str}"
            actual_cmd = f"distrobox enter {self.container_name} -- bash -c {shlex.quote(inner_cmd)}"
            shell = True

        process = subprocess.Popen(
            actual_cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding='utf-8',
            errors='replace',
            shell=shell,
            env=env
        )
        for line in process.stdout:
            self.log_signal.emit(line.rstrip())
        process.wait()
        if process.returncode != 0:
            raise subprocess.CalledProcessError(process.returncode, actual_cmd)

    def _setup_carla_sdk(self):
        sdk_dir = self.source_dir / "carla-sdk"
        inc_dir = sdk_dir / "include"
        lib_dir = sdk_dir / "lib"
        
        if not inc_dir.exists():
            self.log_signal.emit("Carla ヘッダーを取得中...")
            temp_clone = self.source_dir / ".carla_tmp"
            if temp_clone.exists(): shutil.rmtree(temp_clone)
            
            # ヘッダーのみが必要なので、浅いクローンを実行
            self._run_cmd(["git", "clone", "--depth", "1", "https://github.com/falkTX/Carla.git", str(temp_clone)])
            inc_dir.mkdir(parents=True, exist_ok=True)
            shutil.copytree(temp_clone / "source/includes", inc_dir, dirs_exist_ok=True)
            shutil.rmtree(temp_clone)

        # This part is only for Windows, macOS will use Homebrew
        if self.system == "Windows" and not (lib_dir / "carla-standalone.dll").exists():
            self.log_signal.emit("Carla Windows バイナリをダウンロード中...")
            lib_dir.mkdir(parents=True, exist_ok=True)
            version = "2.6.0"
            suffix = "win64"
            carla_zip_url = f"https://github.com/falkTX/Carla/releases/download/v{version}/Carla_{version}-{suffix}.zip"
            zip_path = sdk_dir / "carla.zip"
            
            urllib.request.urlretrieve(carla_zip_url, zip_path)
            shutil.unpack_archive(zip_path, sdk_dir)
            
            # 必要なライブラリを整理
            ext = "*.dll"
            for lib_file in sdk_dir.rglob(ext):
                shutil.move(str(lib_file), lib_dir / lib_file.name)
            zip_path.unlink()

    def install_dependencies(self):
        self.log_signal.emit("依存関係を確認中...")
        
        if self.system == "Windows":
            # MSYS2 (UCRT64)
            if "MSYSTEM" in os.environ:
                if os.environ["MSYSTEM"] != "UCRT64":
                    self.log_signal.emit("警告: UCRT64 環境以外が検出されました。UCRT64 での実行を強く推奨します。")

                self.log_signal.emit("MSYS2 環境を検出しました。")
                deps = [
                    "mingw-w64-ucrt-x86_64-toolchain",
                    "mingw-w64-ucrt-x86_64-cmake",
                    "mingw-w64-ucrt-x86_64-ninja",
                    "git",
                    "mingw-w64-ucrt-x86_64-qt6-base",
                    "mingw-w64-ucrt-x86_64-qt6-declarative",
                    "mingw-w64-ucrt-x86_64-qt6-quick3d",
                    "mingw-w64-ucrt-x86_64-qt6-multimedia",
                    "mingw-w64-ucrt-x86_64-qt6-shadertools",
                    "mingw-w64-ucrt-x86_64-qt6-svg",
                    "mingw-w64-ucrt-x86_64-qt6-5compat",
                    "mingw-w64-ucrt-x86_64-qt6-tools",
                    "mingw-w64-ucrt-x86_64-ffmpeg",
                    "mingw-w64-ucrt-x86_64-luajit",
                    "mingw-w64-ucrt-x86_64-vulkan-loader",
                    "mingw-w64-ucrt-x86_64-vulkan-headers",
                    "mingw-w64-ucrt-x86_64-pkg-config",
                    "mingw-w64-ucrt-x86_64-mold",
                    "mingw-w64-ucrt-x86_64-lilv",
                    "mingw-w64-ucrt-x86_64-ladspa-sdk",
                    "mingw-w64-ucrt-x86_64-curl",
                    "mingw-w64-ucrt-x86_64-extra-cmake-modules",
                    "mingw-w64-ucrt-x86_64-kf6-kirigami",
                    "mingw-w64-ucrt-x86_64-kf6-kcolorscheme",
                    "zip",
                    "mingw-w64-ucrt-x86_64-clang-tools-extra"
                ]
                self._run_cmd(["pacman", "-Syu", "--needed", "--noconfirm"] + deps)
                
                # Audio Plugin SDKs
                if not (self.source_dir / "clap").exists():
                    self._run_cmd(["git", "clone", "https://github.com/free-audio/clap.git", "--depth", "1"])

                # Carla SDK 自動セットアップ
                self._setup_carla_sdk()

            else:
                self.log_signal.emit("警告: MSYS2 環境外で実行されています。依存関係の自動インストールはスキップされます。")

        elif self.system == "Darwin":
            self.log_signal.emit("macOS (Homebrew) 環境を検出しました。")
            if not shutil.which("brew"):
                self.log_signal.emit("エラー: Homebrew が見つかりません。")
                return

            # 公式ドキュメントの手順に従い、KDE Homebrew Tap を構成します
            self.log_signal.emit("KDE公式のHomebrew Tapを構成中...")
            tap_name = "kde-mac/kde"
            tap_url = "https://invent.kde.org/packaging/homebrew-kde.git"
            
            # 既存の誤った設定（もしあれば）をクリアするために untap を試行
            try:
                subprocess.run(["brew", "untap", tap_name], stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL)
            except:
                pass

            # 正しいURLで tap を実行
            try:
                self._run_cmd(["brew", "tap", tap_name, tap_url, "--force-auto-update"])
                
                # ECMやKF6のリンク問題を解決するための caveat スクリプトを実行
                repo_path = subprocess.check_output(["brew", "--repo", tap_name], text=True).strip()
                caveat_script = Path(repo_path) / "tools" / "do-caveats.sh"
                if caveat_script.exists():
                    self.log_signal.emit("KDE Caveats スクリプトを実行中...")
                    self._run_cmd(["sh", str(caveat_script)])
            except subprocess.CalledProcessError:
                self.log_signal.emit("警告: brew tap または caveats の実行に失敗しました。ネットワーク状態を確認してください。")

            deps = [
                "cmake", "ninja", "qt6", "ffmpeg", "luajit", "vulkan-headers", "vulkan-loader",
                "pkg-config", "lilv", "kf6-kirigami", "kf6-kcolorscheme", "extra-cmake-modules", "carla"
            ]
            self._run_cmd(["brew", "install"] + deps)
            
            if not (self.source_dir / "clap").exists():
                self._run_cmd(["git", "clone", "https://github.com/free-audio/clap.git", "--depth", "1"])
            self._setup_carla_sdk()

        elif self.system == "Linux":
            if not self.use_container_opt:
                self.log_signal.emit("コンテナビルドは無効です。ホスト環境でビルドします。")
                return

            # 1. ホスト環境: distrobox と podman の確認
            if not (shutil.which("distrobox") and shutil.which("podman")):
                self.log_signal.emit("警告: distrobox または podman が見つかりません。インストールしてください。")

            # 2. コンテナの作成 (archlinux-rina)
            self.log_signal.emit(f"ビルド用コンテナ '{self.container_name}' を準備中...")
            try:
                self._run_cmd(["distrobox", "create", "--name", self.container_name, "--image", "archlinux:latest", "--yes"])
            except subprocess.CalledProcessError:
                self.log_signal.emit("コンテナの作成をスキップします（既に存在するかエラーが発生しました）。")

            # 3. コンテナ内: ビルド依存関係のインストール
            self.log_signal.emit("コンテナ内の依存関係をインストール中...")
            
            container_deps = [
                "base-devel", "git", "cmake", "ninja", "clang", "mold", "zip",
                "mesa", "vulkan-devel", "libxkbcommon", "wayland", "wayland-protocols",
                "libffi", "ffmpeg", "luajit", "fftw", "qt6-base", "qt6-declarative",
                "qt6-quick3d", "qt6-multimedia", "qt6-shadertools", "qt6-svg",
                "qt6-5compat", "qt6-tools", "lilv", "ladspa", "clap", "vst3sdk", "carla", "kirigami", "openmp", "extra-cmake-modules", "kcolorscheme"
            ]
            # コンテナ内では sudo を使用してインストール
            # ロックファイルの解除を試みる (過去のプロセス中断時の対策)
            try:
                self._run_cmd(["sudo", "rm", "-f", "/var/lib/pacman/db.lck"], in_container=True)
            except Exception:
                pass

            self._run_cmd(["sudo", "pacman", "-Syu", "--needed", "--noconfirm"] + container_deps, in_container=True)

    def run(self):
        try:
            cpu_count = multiprocessing.cpu_count()
            j_slots = cpu_count

            self.log_signal.emit(f"リソース割り当て: {j_slots} 並列ジョブ")
            
            # 依存関係のインストール
            self.install_dependencies()
            
            name = "Debug" if self.is_debug else "Release"
            self.progress_signal.emit(10, f"{name} ビルド開始")
            self.log_signal.emit(f"ソースディレクトリ: {self.source_dir}")
            
            work_dir = self.temp_base / name
            
            # 1. CMake Configuration
            self.log_signal.emit(f"--- {name} 設定 ({self.system}) ---")
            
            conf_cmd = [
                "cmake", "-B", str(work_dir), "-G", "Ninja",
                f"-DCMAKE_BUILD_TYPE={name}",
            ]

            if self.system == "Linux":
                conf_cmd.extend([
                    "-DCMAKE_C_COMPILER=clang",
                    "-DCMAKE_CXX_COMPILER=clang++",
                ])
                if not self.is_debug:
                    conf_cmd.append("-DCMAKE_CXX_FLAGS=-O3 -flto -fno-semantic-interposition -funsafe-math-optimizations")
                    conf_cmd.append("-DCMAKE_POLICY_DEFAULT_CMP0056=NEW")
                    conf_cmd.append("-DCMAKE_SKIP_INSTALL_RPATH=ON")
            
            elif self.system == "Windows":
                 conf_cmd.append("-DCMAKE_BUILD_TYPE=Release")
                 conf_cmd.append(f"-DCLAP_SDK_DIR={self.source_dir}/clap")
                 if (self.source_dir / "carla-sdk").exists():
                     conf_cmd.append(f"-DCARLA_SDK_DIR={self.source_dir}/carla-sdk")
            
            elif self.system == "Darwin":
                 conf_cmd.append("-DCMAKE_BUILD_TYPE=Release")
                 # HomebrewのパスをCMakeの検索対象に追加 (ECMやKF6を見つけるため)
                 try:
                     brew_prefix = subprocess.check_output(["brew", "--prefix"], text=True).strip()
                     conf_cmd.append(f"-DCMAKE_PREFIX_PATH={brew_prefix}")
                 except:
                     conf_cmd.append("-DCMAKE_PREFIX_PATH=/opt/homebrew")

            conf_cmd.append(str(self.source_dir))
            
            # Linuxの場合はコンテナ内で実行
            use_container = (self.system == "Linux" and self.use_container_opt)
            self._run_cmd(conf_cmd, in_container=use_container)

            # 2.5 翻訳ソースファイル (.ts) を自動更新 (常に実行)
            self.log_signal.emit("翻訳ソースファイル (.ts) を更新中...")
            self._run_cmd(["cmake", "--build", str(work_dir), "--target", "Rina_lupdate"], in_container=use_container)

            # 3. Build
            self.log_signal.emit(f"{name} コンパイル中...")
            build_cmd = ["cmake", "--build", str(work_dir), "-j", str(j_slots)]
            self._run_cmd(build_cmd, in_container=use_container)

            # 4. パッケージング処理
            self.log_signal.emit(f"パッケージングを開始します...")
            self.output_dir.mkdir(parents=True, exist_ok=True)
            self.dist_dir.mkdir(parents=True, exist_ok=True)
            
            exe_name = "Rina.exe" if self.system == "Windows" else "Rina"

            if self.system == "Darwin":
                src_app = work_dir / "Rina.app"
                dest_app = self.output_dir / "Rina.app"
                if dest_app.exists(): shutil.rmtree(dest_app)
                if src_app.exists():
                    shutil.copytree(src_app, dest_app)
                    dest_bin = dest_app / "Contents/MacOS/Rina"
                    asset_dest = dest_app / "Contents/Resources"
                else:
                    raise FileNotFoundError(f"{src_app} not found")
            else:
                dest_bin = self.output_dir / exe_name
                src_bin = work_dir / exe_name
                if dest_bin.exists(): dest_bin.unlink()
                if src_bin.exists():
                    shutil.copy2(src_bin, dest_bin)
                else:
                    self.log_signal.emit(f"エラー: 実行ファイルが見つかりません: {src_bin}")
                    raise FileNotFoundError(f"{src_bin} not found")
                asset_dest = self.output_dir

            # アセットのコピー (アプリ動作に必要なデータのみ)
            for d in ["effects", "objects"]:
                src_path = self.source_dir / "ui/qml" / d
                dest_path = asset_dest / d
                if src_path.exists():
                    # Linuxの場合は不要なシェーダーソースを除外
                    ignore_pat = shutil.ignore_patterns("*.frag", "*.vert", "*.glsl") if self.system == "Linux" else None
                    shutil.copytree(src_path, dest_path, ignore=ignore_pat, dirs_exist_ok=True)

            # コンパイル済みシェーダー(.qsb)のコピー
            for d in ["effects", "objects"]:
                qsb_src_dir = work_dir / d
                qsb_dest_dir = asset_dest / d
                if qsb_src_dir.exists() and qsb_dest_dir.exists():
                    for qsb_file in qsb_src_dir.glob("*.qsb"):
                        shutil.copy2(qsb_file, qsb_dest_dir / qsb_file.name)

            # Plugins
            plugins_src = self.source_dir / "plugins"
            if plugins_src.exists() and any(plugins_src.iterdir()):
                plugins_dest = self.output_dir / "plugins"
                shutil.copytree(plugins_src, plugins_dest, dirs_exist_ok=True)

            # i18n (国際化ファイル - 言語拡張)
            i18n_dest = self.output_dir / "i18n"
            
            # CMake (LinguistTools) によって生成された .qm ファイルを収集
            # ビルドディレクトリ内を再帰的に探索して .qm ファイルだけを拾う
            i18n_dest.mkdir(parents=True, exist_ok=True)
            for qm_file in work_dir.rglob("*.qm"):
                # CMake内部のパス（CMakeFiles等）に含まれるものは除外
                if "CMakeFiles" not in qm_file.parts:
                    shutil.copy2(qm_file, i18n_dest / qm_file.name)

            # Windows固有のデプロイ処理 (windeployqtなど)
            if self.system == "Windows":
                self.log_signal.emit("Windows デプロイメント処理を実行中...")
                # windeployqtの実行
                # 注意: PATHにwindeployqtが含まれている必要がある
                qml_dir = self.source_dir / "ui/qml"
                deploy_cmd = [
                    "windeployqt",
                    "--qmldir", str(qml_dir),
                    "--no-translations",
                    "--no-compiler-runtime",
                    "--release" if not self.is_debug else "--debug",
                    str(dest_bin),
                    "--dir", str(self.output_dir)
                ]
                self._run_cmd(deploy_cmd)
                
                # qt.confの作成
                qt_conf = self.output_dir / "qt.conf"
                with open(qt_conf, "w") as f:
                    f.write("[Paths]\nPlugins = .\n")
                
                # 非Qt依存関係の収集 (簡易的)
                # lddの代わりに、MSYS2環境なら依存DLLをコピーするロジックが必要だが、
                # ここではwindeployqtが主要なQt依存関係を処理したと仮定する。
                # 必要であれば、objdump等で依存関係を解析してコピーする処理を追加する。

            elif self.system == "Darwin":
                self.log_signal.emit("macOS デプロイメント処理を実行中...")
                qt_prefix = subprocess.check_output(["brew", "--prefix", "qt6"], text=True).strip()
                macdeployqt = f"{qt_prefix}/bin/macdeployqt"
                deploy_cmd = [
                    macdeployqt, str(dest_app),
                    "-qmldir=" + str(self.source_dir / "ui/qml"),
                    "-verbose=1"
                ]
                self._run_cmd(deploy_cmd)

            # ZIP圧縮
            self.log_signal.emit("アーカイブを作成中...")
            archive_name = f"Rina-{self.system}-x64"
            if self.system == "Linux":
                archive_name = "Rina-Linux-x86_64-v3"
            elif self.system == "Darwin":
                archive_name = "Rina-macOS-Universal"
            
            if self.system == "Darwin":
                shutil.make_archive(
                    str(self.dist_dir / archive_name),
                    'zip',
                    root_dir=self.output_dir,
                    base_dir="Rina.app"
                )
            else:
                shutil.make_archive(
                    str(self.dist_dir / archive_name),
                    'zip',
                    root_dir=self.output_dir
                )

            self.log_signal.emit(f"ビルド完了：{dest_bin}")
            self.log_signal.emit(f"アーカイブ：{self.dist_dir / (archive_name + '.zip')}")
            if self.system == "Windows":
                 self.log_signal.emit("ヒント：buildフォルダ内のRina.exeを実行してください")
            else:
                 self.log_signal.emit("ヒント：python BUILD.py --runで直ちに起動できます")
            
            self.progress_signal.emit(100, "完了")
            self.finished_signal.emit(True, f"ビルド成功")
        except Exception as e:
            self.finished_signal.emit(False, str(e))

if __name__ == "__main__":
    # CLI Mode (Release Build)
    app = QtCore.QCoreApplication(sys.argv)
    
    source_dir = Path.cwd()
    temp_base = source_dir / ".build_tmp"
    output_dir = source_dir / "build"
    
    # Host Native Release Build
    use_container = "--no-container" not in sys.argv
    
    worker = BuildWorker(source_dir, temp_base, output_dir, is_debug=False, use_container=use_container)
    
    worker.log_signal.connect(print)
    worker.progress_signal.connect(lambda val, msg: print(f"[{val}%] {msg}"))
    
    def on_cli_finished(success, msg):
        if success:
            app.quit()
        else:
            print(f"\nビルド失敗: {msg}")
            app.exit(1)
    worker.finished_signal.connect(on_cli_finished)
    
    print("リリースビルドを開始します...")
    worker.start()
    sys.exit(app.exec())